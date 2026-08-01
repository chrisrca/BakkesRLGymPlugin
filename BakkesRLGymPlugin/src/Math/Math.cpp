#include "Math.h"

Quat Math::RotMatToQuat(Vector forward, Vector right, Vector up) {
	// Copied from BulletPhysics btMatrix3x3::getRotation()

	float m_el[3][3] = {
		{ forward.X, right.X, up.X },
		{ forward.Y, right.Y, up.Y },
		{ forward.Z, right.Z, up.Z }
	};

	float trace = m_el[0][0] + m_el[1][1] + m_el[2][2];

	float temp[4];

	if (trace > 0.0f) {
		float s = sqrt(trace + 1.0f);
		temp[3] = (s * 0.5f);
		s = 0.5f / s;

		temp[0] = ((m_el[2][1] - m_el[1][2]) * s);
		temp[1] = ((m_el[0][2] - m_el[2][0]) * s);
		temp[2] = ((m_el[1][0] - m_el[0][1]) * s);
	} else {
		int i = m_el[0][0] < m_el[1][1] ? (m_el[1][1] < m_el[2][2] ? 2 : 1) : (m_el[0][0] < m_el[2][2] ? 2 : 0);
		int j = (i + 1) % 3;
		int k = (i + 2) % 3;

		float s = sqrtf(m_el[i][i] - m_el[j][j] - m_el[k][k] + 1.0f);
		temp[i] = s * 0.5f;
		s = 0.5f / s;

		temp[3] = (m_el[k][j] - m_el[j][k]) * s;
		temp[j] = (m_el[j][i] + m_el[i][j]) * s;
		temp[k] = (m_el[k][i] + m_el[i][k]) * s;
	}

	return Quat(temp[3], temp[0], temp[1], temp[2]);
}

namespace {
	// Row-major 3x3: m[row][col], row = world axis (X,Y,Z), col = {front, left, up} local axis.
	struct Mat3 {
		float m[3][3] = {};
	};

	// See rlgym/utils/math.py: euler_to_rotation(pyr)
	Mat3 EulerToRotMtx(float pitch, float yaw, float roll) {
		float cp = cosf(pitch), cy = cosf(yaw), cr = cosf(roll);
		float sp = sinf(pitch), sy = sinf(yaw), sr = sinf(roll);

		Mat3 theta;

		// front
		theta.m[0][0] = cp * cy;
		theta.m[1][0] = cp * sy;
		theta.m[2][0] = sp;

		// left
		theta.m[0][1] = cy * sp * sr - cr * sy;
		theta.m[1][1] = sy * sp * sr + cr * cy;
		theta.m[2][1] = -cp * sr;

		// up
		theta.m[0][2] = -cr * cy * sp - sr * sy;
		theta.m[1][2] = -cr * sy * sp + sr * cy;
		theta.m[2][2] = cp * cr;

		return theta;
	}

	// See rlgym/utils/math.py: rotation_to_quaternion(m)
	Quat RotMtxToQuatRLGym(const Mat3& mat) {
		float trace = mat.m[0][0] + mat.m[1][1] + mat.m[2][2];
		float q[4] = {}; // [w, x, y, z]

		if (trace > 0) {
			float s = sqrtf(trace + 1.0f);
			q[0] = s * 0.5f;
			s = 0.5f / s;
			q[1] = (mat.m[2][1] - mat.m[1][2]) * s;
			q[2] = (mat.m[0][2] - mat.m[2][0]) * s;
			q[3] = (mat.m[1][0] - mat.m[0][1]) * s;
		} else if (mat.m[0][0] >= mat.m[1][1] && mat.m[0][0] >= mat.m[2][2]) {
			float s = sqrtf(1.0f + mat.m[0][0] - mat.m[1][1] - mat.m[2][2]);
			float invS = 0.5f / s;
			q[1] = 0.5f * s;
			q[2] = (mat.m[1][0] + mat.m[0][1]) * invS;
			q[3] = (mat.m[2][0] + mat.m[0][2]) * invS;
			q[0] = (mat.m[2][1] - mat.m[1][2]) * invS;
		} else if (mat.m[1][1] > mat.m[2][2]) {
			float s = sqrtf(1.0f + mat.m[1][1] - mat.m[0][0] - mat.m[2][2]);
			float invS = 0.5f / s;
			q[1] = (mat.m[0][1] + mat.m[1][0]) * invS;
			q[2] = 0.5f * s;
			q[3] = (mat.m[1][2] + mat.m[2][1]) * invS;
			q[0] = (mat.m[0][2] - mat.m[2][0]) * invS;
		} else {
			float s = sqrtf(1.0f + mat.m[2][2] - mat.m[0][0] - mat.m[1][1]);
			float invS = 0.5f / s;
			q[1] = (mat.m[0][2] + mat.m[2][0]) * invS;
			q[2] = (mat.m[1][2] + mat.m[2][1]) * invS;
			q[3] = 0.5f * s;
			q[0] = (mat.m[1][0] - mat.m[0][1]) * invS;
		}

		// rlgym negates the whole quaternion before returning (`return -q`).
		return Quat(-q[0], -q[1], -q[2], -q[3]);
	}
}

Quat Math::EulerToQuat(float pitch, float yaw, float roll) {
	Mat3 mat = EulerToRotMtx(pitch, yaw, roll);
	return RotMtxToQuatRLGym(mat);
}
