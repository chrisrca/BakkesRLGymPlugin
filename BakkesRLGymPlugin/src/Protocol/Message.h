#pragma once
#include "../Framework.h"

// See rlgym/communication/message.py
namespace RLGymMessage {
	const vector<float> HEADER_END_TOKEN = { 13771, 83712, 83770 };
	const vector<float> BODY_END_TOKEN = { 82772, 83273, 83774 };

	const vector<float> NULL_MESSAGE_HEADER = { 83373, 83734, 83775 };
	const vector<float> NULL_MESSAGE_BODY = { 83744, 83774, 83876 };
	const vector<float> CONFIG_MESSAGE_HEADER = { 83775, 53776, 83727 };
	const vector<float> STATE_MESSAGE_HEADER = { 63776, 83777, 83778 };
	const vector<float> AGENT_ACTION_MESSAGE_HEADER = { 87777, 83778, 83779 };
	const vector<float> RESET_GAME_STATE_MESSAGE_HEADER = { 83878, 83779, 83780 };
	const vector<float> AGENT_ACTION_IMMEDIATE_RESPONSE_MESSAGE_HEADER = { 83799, 83780, 83781 };
	const vector<float> REQUEST_LAST_BOT_INPUT_MESSAGE_HEADER = { 83781, 83781, 83682 };
	const vector<float> LAST_BOT_INPUT_MESSAGE_HEADER = { 11781, 83782, 83983 };
	const vector<float> RESET_TO_SPECIFIC_GAME_STATE_MESSAGE_HEADER = { 12782, 83783, 80784 };

	enum class Type {
		Unknown = 0,
		Null,
		Config,
		State,
		AgentAction,
		ResetGameState,
		AgentActionImmediateResponse,
		RequestLastBotInput,
		LastBotInput,
		ResetToSpecificGameState
	};

	inline optional<size_t> FindFirst(const vector<float>& haystack, const vector<float>& target) {
		if (target.empty() || haystack.size() < target.size())
			return std::nullopt;

		for (size_t i = 0; i + target.size() <= haystack.size(); i++) {
			if (haystack[i] == target[0]) {
				bool match = true;
				for (size_t j = 1; j < target.size(); j++) {
					if (haystack[i + j] != target[j]) {
						match = false;
						break;
					}
				}
				if (match)
					return i;
			}
		}

		return std::nullopt;
	}

	inline Type IdentifyHeader(const vector<float>& header) {
		if (header == CONFIG_MESSAGE_HEADER) return Type::Config;
		if (header == STATE_MESSAGE_HEADER) return Type::State;
		if (header == AGENT_ACTION_MESSAGE_HEADER) return Type::AgentAction;
		if (header == RESET_GAME_STATE_MESSAGE_HEADER) return Type::ResetGameState;
		if (header == AGENT_ACTION_IMMEDIATE_RESPONSE_MESSAGE_HEADER) return Type::AgentActionImmediateResponse;
		if (header == REQUEST_LAST_BOT_INPUT_MESSAGE_HEADER) return Type::RequestLastBotInput;
		if (header == LAST_BOT_INPUT_MESSAGE_HEADER) return Type::LastBotInput;
		if (header == RESET_TO_SPECIFIC_GAME_STATE_MESSAGE_HEADER) return Type::ResetToSpecificGameState;
		if (header == NULL_MESSAGE_HEADER) return Type::Null;
		return Type::Unknown;
	}

	inline const vector<float>& HeaderFor(Type type) {
		switch (type) {
			case Type::Config: return CONFIG_MESSAGE_HEADER;
			case Type::State: return STATE_MESSAGE_HEADER;
			case Type::AgentAction: return AGENT_ACTION_MESSAGE_HEADER;
			case Type::ResetGameState: return RESET_GAME_STATE_MESSAGE_HEADER;
			case Type::AgentActionImmediateResponse: return AGENT_ACTION_IMMEDIATE_RESPONSE_MESSAGE_HEADER;
			case Type::RequestLastBotInput: return REQUEST_LAST_BOT_INPUT_MESSAGE_HEADER;
			case Type::LastBotInput: return LAST_BOT_INPUT_MESSAGE_HEADER;
			case Type::ResetToSpecificGameState: return RESET_TO_SPECIFIC_GAME_STATE_MESSAGE_HEADER;
			default: return NULL_MESSAGE_HEADER;
		}
	}

	struct Message {
		vector<float> header = NULL_MESSAGE_HEADER;
		vector<float> body = NULL_MESSAGE_BODY;

		// See Message.serialize()
		vector<float> Serialize() const {
			vector<float> result;
			result.reserve(header.size() + HEADER_END_TOKEN.size() + body.size() + BODY_END_TOKEN.size());
			result.insert(result.end(), header.begin(), header.end());
			result.insert(result.end(), HEADER_END_TOKEN.begin(), HEADER_END_TOKEN.end());
			result.insert(result.end(), body.begin(), body.end());
			result.insert(result.end(), BODY_END_TOKEN.begin(), BODY_END_TOKEN.end());
			return result;
		}

		// See Message.deserialize(). Returns false if the framing tokens can't be found.
		static bool Deserialize(const vector<float>& floats, Message& out) {
			auto headerEnd = FindFirst(floats, HEADER_END_TOKEN);
			if (!headerEnd)
				return false;

			size_t bodyStart = *headerEnd + HEADER_END_TOKEN.size();

			auto bodyEnd = FindFirst(floats, BODY_END_TOKEN);
			if (!bodyEnd || *bodyEnd < bodyStart)
				return false;

			out.header.assign(floats.begin(), floats.begin() + *headerEnd);
			out.body.assign(floats.begin() + bodyStart, floats.begin() + *bodyEnd);
			return true;
		}
	};
}
