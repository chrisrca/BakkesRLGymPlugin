#include "GameStateCodec.h"

namespace {
	// A 180-degree-yaw mirror of the field is a proper rotation
	Vector InvertVec(Vector v) {
		return Vector(-v.X, -v.Y, v.Z);
	}

	Quat InvertQuat(Quat q) {
		Vector fwd = RotateVectorWithQuat(Vector(1, 0, 0), q);
		Vector right = RotateVectorWithQuat(Vector(0, 1, 0), q);
		Vector up = RotateVectorWithQuat(Vector(0, 0, 1), q);

		return Math::RotMatToQuat(InvertVec(fwd), InvertVec(right), InvertVec(up));
	}

	void AppendVec(vector<float>& out, Vector v) {
		out.push_back(v.X);
		out.push_back(v.Y);
		out.push_back(v.Z);
	}

	void AppendQuat(vector<float>& out, Quat q) {
		out.push_back(q.W);
		out.push_back(q.X);
		out.push_back(q.Y);
		out.push_back(q.Z);
	}
}

vector<float> GameStateCodec::EncodeState(
	int ticksSinceLast,
	int blueScore,
	int orangeScore,
	const bool boostActive[BOOST_PADS_LENGTH],
	const RBState& ballState,
	const vector<PlayerStateSnapshot>& players
) {
	vector<float> out;
	out.reserve(3 + BOOST_PADS_LENGTH + BALL_STATE_LENGTH + players.size() * PLAYER_INFO_LENGTH);

	out.push_back((float)ticksSinceLast);
	out.push_back((float)blueScore);
	out.push_back((float)orangeScore);

	for (int i = 0; i < BOOST_PADS_LENGTH; i++)
		out.push_back(boostActive[i] ? 1.0f : 0.0f);

	// Ball (regular)
	AppendVec(out, ballState.Location);
	AppendVec(out, ballState.LinearVelocity);
	AppendVec(out, ballState.AngularVelocity);

	// Ball (inverted)
	AppendVec(out, InvertVec(ballState.Location));
	AppendVec(out, InvertVec(ballState.LinearVelocity));
	AppendVec(out, InvertVec(ballState.AngularVelocity));

	for (const auto& p : players) {
		out.push_back((float)p.specId);
		out.push_back((float)p.teamNum);

		// Car (regular)
		AppendVec(out, p.carState.Location);
		AppendQuat(out, p.carState.Quaternion);
		AppendVec(out, p.carState.LinearVelocity);
		AppendVec(out, p.carState.AngularVelocity);

		// Car (inverted)
		AppendVec(out, InvertVec(p.carState.Location));
		AppendQuat(out, InvertQuat(p.carState.Quaternion));
		AppendVec(out, InvertVec(p.carState.LinearVelocity));
		AppendVec(out, InvertVec(p.carState.AngularVelocity));

		// Tertiary
		out.push_back((float)p.matchGoals);
		out.push_back((float)p.matchSaves);
		out.push_back((float)p.matchShots);
		out.push_back((float)p.matchDemolishes);
		out.push_back((float)p.boostPickups);
		out.push_back(p.isDemoed ? 1.0f : 0.0f);
		out.push_back(p.onGround ? 1.0f : 0.0f);
		out.push_back(p.ballTouched ? 1.0f : 0.0f);
		out.push_back(p.hasJump ? 1.0f : 0.0f);
		out.push_back(p.hasFlip ? 1.0f : 0.0f);
		out.push_back(p.boostAmount);
	}

	return out;
}

bool GameStateCodec::DecodeResetState(const vector<float>& body, ResetState& out) {
	constexpr int BALL_LEN = 9; // pos3 + vel3 + angvel3, see rlgym PhysicsWrapper._encode()
	constexpr int CAR_LEN = 14; // id + pos3 + vel3 + angvel3 + euler3 + boost, see rlgym CarWrapper._encode()

	if (body.size() < BALL_LEN)
		return false;

	size_t remaining = body.size() - BALL_LEN;
	if (remaining % CAR_LEN != 0)
		return false; // Malformed body

	out.cars.clear();

	size_t pos = 0;
	out.ballPos = Vector(body[pos], body[pos + 1], body[pos + 2]);
	out.ballVel = Vector(body[pos + 3], body[pos + 4], body[pos + 5]);
	out.ballAngVel = Vector(body[pos + 6], body[pos + 7], body[pos + 8]);
	pos += BALL_LEN;

	size_t numCars = remaining / CAR_LEN;
	out.cars.reserve(numCars);

	for (size_t i = 0; i < numCars; i++) {
		ResetCarTarget car;
		car.specId = (int)body[pos + 0];
		car.pos = Vector(body[pos + 1], body[pos + 2], body[pos + 3]);
		car.vel = Vector(body[pos + 4], body[pos + 5], body[pos + 6]);
		car.angVel = Vector(body[pos + 7], body[pos + 8], body[pos + 9]);
		car.pitch = body[pos + 10];
		car.yaw = body[pos + 11];
		car.roll = body[pos + 12];
		car.boost = body[pos + 13];
		out.cars.push_back(car);

		pos += CAR_LEN;
	}

	return true;
}
