#pragma once
#include "../Framework.h"

static_assert(sizeof(Vector) == sizeof(float) * 3, "Vector is not the size of 3 floats?");
static_assert(sizeof(Quat) == sizeof(float) * 4, "Quat is not the size of 4 floats?");

namespace Math {
	Quat RotMatToQuat(Vector forward, Vector right, Vector up);

	// Converts (pitch, yaw, roll) radians into a BakkesMod Quat.
	Quat EulerToQuat(float pitch, float yaw, float roll);
}
