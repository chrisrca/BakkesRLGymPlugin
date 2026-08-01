#pragma once
#include "../Framework.h"
#include "../Protocol/Message.h"

// Named-pipe CLIENT that speaks the same wire protocol rlgym's
// CommunicationHandler (rlgym/communication/communication_handler.py) speaks as
// the pipe SERVER. Python creates the pipe and waits for us to connect
class PipeClient {
public:
	~PipeClient();

	// Reads Rocket League's own command line for the "-pipe <name>" argument
	// (see rlgym/gamelaunch/launch.py) and connects to it.
	bool ConnectFromCommandLine();

	bool IsConnected() const { return m_connected; }
	void Disconnect();

	// Thread-safe. Returns false if nothing is queued.
	bool TryPopMessage(RLGymMessage::Message& out);

	// Thread-safe, non-blocking: hands the message to a dedicated writer
	// thread and returns immediately.
	bool Send(const RLGymMessage::Message& msg);

	static string ParsePipeNameFromCommandLine();

private:
	bool ConnectTo(const string& pipeName);
	void ReadThreadFunc();
	void WriteThreadFunc();
	bool ReadRawMessage(vector<byte>& outBytes);
	bool WriteRawMessage(const vector<float>& floats);

	void* m_pipe = nullptr;
	std::thread m_readThread;
	std::thread m_writeThread;
	std::atomic<bool> m_running = false;
	std::atomic<bool> m_connected = false;

	static constexpr long long WRITE_WARN_MS = 3000;

	std::mutex m_queueMutex;
	std::deque<RLGymMessage::Message> m_incoming;

	std::mutex m_outgoingMutex;
	std::condition_variable m_outgoingCV;
	std::deque<vector<float>> m_outgoing;
};
