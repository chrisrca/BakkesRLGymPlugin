#pragma once
#include "../Framework.h"
#include "../Math/Math.h"

// Encodes/decodes the STATE and RESET_GAME_STATE message bodies 
namespace GameStateCodec {
	constexpr int BOOST_PADS_LENGTH = 34;
	constexpr int BALL_STATE_LENGTH = 18; // 9 regular + 9 inverted
	constexpr int PLAYER_CAR_STATE_LENGTH = 13; // pos3 + quat4 + linvel3 + angvel3
	constexpr int PLAYER_TERTIARY_INFO_LENGTH = 11;
	constexpr int PLAYER_INFO_LENGTH = 2 + 2 * PLAYER_CAR_STATE_LENGTH + PLAYER_TERTIARY_INFO_LENGTH; // 39

	struct PlayerStateSnapshot {
		int specId = -1;
		int teamNum = 0; // 0 = blue, 1 = orange

		RBState carState = {};

		int matchGoals = 0;
		int matchSaves = 0;
		int matchShots = 0;
		int matchDemolishes = 0;
		int boostPickups = 0;

		bool isDemoed = false;
		bool onGround = false;
		bool ballTouched = false;
		bool hasJump = false;
		bool hasFlip = false;

		float boostAmount = 0;
	};

	vector<float> EncodeState(
		int ticksSinceLast,
		int blueScore,
		int orangeScore,
		const bool boostActive[BOOST_PADS_LENGTH],
		const RBState& ballState,
		const vector<PlayerStateSnapshot>& players
	);

	struct ResetCarTarget {
		int specId = -1;
		Vector pos, vel, angVel;
		float pitch = 0, yaw = 0, roll = 0;
		float boost = 0;
	};

	struct ResetState {
		Vector ballPos, ballVel, ballAngVel;
		vector<ResetCarTarget> cars;
	};

	bool DecodeResetState(const vector<float>& body, ResetState& out);
}
