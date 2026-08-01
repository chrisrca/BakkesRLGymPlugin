#include "PipeClient.h"

#include <sstream>
#include <iterator>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

PipeClient::~PipeClient() {
	Disconnect();
}

string PipeClient::ParsePipeNameFromCommandLine() {
	string cmdLine = GetCommandLineA();

	std::istringstream iss(cmdLine);
	vector<string> tokens{ std::istream_iterator<string>{iss}, std::istream_iterator<string>{} };

	for (size_t i = 0; i + 1 < tokens.size(); i++) {
		if (tokens[i] == "-pipe") {
			string val = tokens[i + 1];
			constexpr const char* PIPE_PREFIX = "\\\\.\\pipe\\";
			if (val.rfind(PIPE_PREFIX, 0) == 0)
				return val;
			return string(PIPE_PREFIX) + val;
		}
	}

	return "";
}

bool PipeClient::ConnectFromCommandLine() {
	string pipeName = ParsePipeNameFromCommandLine();
	if (pipeName.empty()) {
		LOG("PipeClient: No \"-pipe\" argument found on the command line, RLGym integration will stay idle.");
		return false;
	}

	LOG("PipeClient: Connecting to \"" << pipeName << "\"...");
	return ConnectTo(pipeName);
}

bool PipeClient::ConnectTo(const string& pipeName) {
	constexpr int MAX_ATTEMPTS = 100;
	constexpr int WAIT_MS = 200;

	for (int attempt = 0; attempt < MAX_ATTEMPTS; attempt++) {
		HANDLE h = CreateFileA(
			pipeName.c_str(),
			GENERIC_READ | GENERIC_WRITE,
			0, NULL,
			OPEN_EXISTING,
			FILE_FLAG_OVERLAPPED, NULL
		);

		if (h != INVALID_HANDLE_VALUE) {
			DWORD mode = PIPE_READMODE_MESSAGE;
			SetNamedPipeHandleState(h, &mode, NULL, NULL);

			m_pipe = h;
			m_running = true;
			m_connected = true;
			m_readThread = std::thread(&PipeClient::ReadThreadFunc, this);
			m_writeThread = std::thread(&PipeClient::WriteThreadFunc, this);

			LOG("PipeClient: Connected to \"" << pipeName << "\"");
			return true;
		}

		DWORD err = GetLastError();
		if (err != ERROR_PIPE_BUSY && err != ERROR_FILE_NOT_FOUND) {
			LOG("PipeClient: CreateFile(\"" << pipeName << "\") failed, error " << err);
		}

		WaitNamedPipeA(pipeName.c_str(), WAIT_MS);
		std::this_thread::sleep_for(std::chrono::milliseconds(WAIT_MS));
	}

	LOG("PipeClient: Gave up connecting to \"" << pipeName << "\" after " << MAX_ATTEMPTS << " attempts.");
	return false;
}

void PipeClient::Disconnect() {
	m_running = false;
	m_connected = false;

	if (m_pipe) {
		// Unblocks any pending overlapped ReadFile/WriteFile in the read/write threads
		CancelIoEx((HANDLE)m_pipe, NULL);
		CloseHandle((HANDLE)m_pipe);
		m_pipe = nullptr;
	}

	m_outgoingCV.notify_all();

	if (m_readThread.joinable())
		m_readThread.join();
	if (m_writeThread.joinable())
		m_writeThread.join();
}

static bool WaitForEventOrShutdown(HANDLE hEvent, const std::atomic<bool>& running) {
	while (running) {
		if (WaitForSingleObject(hEvent, 250) == WAIT_OBJECT_0)
			return true;
	}
	return false;
}

bool PipeClient::ReadRawMessage(vector<byte>& outBytes) {
	outBytes.clear();

	byte buffer[4096];
	bool messageContinues;
	do {
		OVERLAPPED ov = {};
		ov.hEvent = CreateEventA(NULL, TRUE, FALSE, NULL);

		DWORD bytesRead = 0;
		BOOL immediate = ReadFile((HANDLE)m_pipe, buffer, sizeof(buffer), NULL, &ov);
		DWORD err = immediate ? 0 : GetLastError();

		if (!immediate && err != ERROR_IO_PENDING) {
			CloseHandle(ov.hEvent);
			return false; // Pipe broken/closed
		}

		if (!immediate && !WaitForEventOrShutdown(ov.hEvent, m_running)) {
			CancelIoEx((HANDLE)m_pipe, &ov);
			GetOverlappedResult((HANDLE)m_pipe, &ov, &bytesRead, TRUE);
			CloseHandle(ov.hEvent);
			return false;
		}

		BOOL ok = GetOverlappedResult((HANDLE)m_pipe, &ov, &bytesRead, FALSE);
		DWORD finalErr = ok ? 0 : GetLastError();
		CloseHandle(ov.hEvent);

		if (!ok && finalErr != ERROR_MORE_DATA)
			return false; // Pipe broken/closed

		messageContinues = !ok;
		outBytes.insert(outBytes.end(), buffer, buffer + bytesRead);
	} while (messageContinues);

	return true;
}

void PipeClient::ReadThreadFunc() {
	while (m_running) {
		vector<byte> raw;
		if (!ReadRawMessage(raw)) {
			if (m_running) {
				LOG("PipeClient: Read failed, pipe appears to be closed.");
				m_connected = false;
				m_running = false;
			}
			break;
		}

		if (raw.empty())
			continue;

		if (raw.size() % sizeof(float) != 0) {
			LOG("PipeClient: Received a message with a non-float-aligned size (" << raw.size() << " bytes), discarding.");
			continue;
		}

		vector<float> floats(raw.size() / sizeof(float));
		memcpy(floats.data(), raw.data(), raw.size());

		RLGymMessage::Message msg;
		if (RLGymMessage::Message::Deserialize(floats, msg)) {
			std::lock_guard<std::mutex> lock(m_queueMutex);
			m_incoming.push_back(std::move(msg));
		} else {
			LOG("PipeClient: Received a message that couldn't be parsed (missing framing tokens), discarding.");
		}
	}
}

bool PipeClient::TryPopMessage(RLGymMessage::Message& out) {
	std::lock_guard<std::mutex> lock(m_queueMutex);
	if (m_incoming.empty())
		return false;

	out = std::move(m_incoming.front());
	m_incoming.pop_front();
	return true;
}

bool PipeClient::Send(const RLGymMessage::Message& msg) {
	if (!m_connected)
		return false;

	{
		std::lock_guard<std::mutex> lock(m_outgoingMutex);
		m_outgoing.push_back(msg.Serialize());
	}
	m_outgoingCV.notify_one();

	return true;
}

bool PipeClient::WriteRawMessage(const vector<float>& floats) {
	DWORD byteCount = (DWORD)(floats.size() * sizeof(float));

	OVERLAPPED ov = {};
	ov.hEvent = CreateEventA(NULL, TRUE, FALSE, NULL);

	long long startMs = CUR_MS();
	DWORD bytesWritten = 0;
	BOOL immediate = WriteFile((HANDLE)m_pipe, floats.data(), byteCount, NULL, &ov);
	DWORD err = immediate ? 0 : GetLastError();

	if (!immediate && err != ERROR_IO_PENDING) {
		LOG("PipeClient: WriteFile failed to start, error " << err);
		CloseHandle(ov.hEvent);
		return false;
	}

	if (!immediate) {
		long long lastWarnMs = startMs;
		bool signalled = false;
		while (m_running) {
			if (WaitForSingleObject(ov.hEvent, 250) == WAIT_OBJECT_0) {
				signalled = true;
				break;
			}
			long long now = CUR_MS();
			if (now - lastWarnMs >= WRITE_WARN_MS) {
				LOG("PipeClient: WriteFile has been in progress for " << (now - startMs)
					<< "ms (python likely hasn't called ReadFile yet) - still waiting.");
				lastWarnMs = now;
			}
		}

		if (!signalled) {
			CancelIoEx((HANDLE)m_pipe, &ov);
			GetOverlappedResult((HANDLE)m_pipe, &ov, &bytesWritten, TRUE);
			CloseHandle(ov.hEvent);
			return false;
		}
	}

	BOOL ok = GetOverlappedResult((HANDLE)m_pipe, &ov, &bytesWritten, FALSE);
	DWORD finalErr = ok ? 0 : GetLastError();
	CloseHandle(ov.hEvent);

	long long elapsed = CUR_MS() - startMs;
	if (!ok || bytesWritten != byteCount) {
		LOG("PipeClient: WriteFile failed after " << elapsed << "ms, error " << finalErr << " (wrote " << bytesWritten << " bytes)");
		return false;
	}

	return true;
}

void PipeClient::WriteThreadFunc() {
	while (m_running) {
		vector<float> next;
		{
			std::unique_lock<std::mutex> lock(m_outgoingMutex);
			m_outgoingCV.wait(lock, [this] { return !m_running || !m_outgoing.empty(); });

			if (!m_running)
				break;

			next = std::move(m_outgoing.front());
			m_outgoing.pop_front();
		}

		if (!WriteRawMessage(next)) {
			if (m_running) {
				m_connected = false;
				m_running = false;
			}
			break;
		}
	}
}
