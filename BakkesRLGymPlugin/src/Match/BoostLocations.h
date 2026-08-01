#pragma once
#include "../Framework.h"

// Mirrors rlgym/utils/common_values.py: BOOST_LOCATIONS.
namespace BoostLocations {
	inline const vector<Vector> LOCATIONS = {
		Vector(0.0f, -4240.0f, 70.0f),
		Vector(-1792.0f, -4184.0f, 70.0f),
		Vector(1792.0f, -4184.0f, 70.0f),
		Vector(-3072.0f, -4096.0f, 73.0f),
		Vector(3072.0f, -4096.0f, 73.0f),
		Vector(-940.0f, -3308.0f, 70.0f),
		Vector(940.0f, -3308.0f, 70.0f),
		Vector(0.0f, -2816.0f, 70.0f),
		Vector(-3584.0f, -2484.0f, 70.0f),
		Vector(3584.0f, -2484.0f, 70.0f),
		Vector(-1788.0f, -2300.0f, 70.0f),
		Vector(1788.0f, -2300.0f, 70.0f),
		Vector(-2048.0f, -1036.0f, 70.0f),
		Vector(0.0f, -1024.0f, 70.0f),
		Vector(2048.0f, -1036.0f, 70.0f),
		Vector(-3584.0f, 0.0f, 73.0f),
		Vector(-1024.0f, 0.0f, 70.0f),
		Vector(1024.0f, 0.0f, 70.0f),
		Vector(3584.0f, 0.0f, 73.0f),
		Vector(-2048.0f, 1036.0f, 70.0f),
		Vector(0.0f, 1024.0f, 70.0f),
		Vector(2048.0f, 1036.0f, 70.0f),
		Vector(-1788.0f, 2300.0f, 70.0f),
		Vector(1788.0f, 2300.0f, 70.0f),
		Vector(-3584.0f, 2484.0f, 70.0f),
		Vector(3584.0f, 2484.0f, 70.0f),
		Vector(0.0f, 2816.0f, 70.0f),
		Vector(-940.0f, 3310.0f, 70.0f),
		Vector(940.0f, 3308.0f, 70.0f),
		Vector(-3072.0f, 4096.0f, 73.0f),
		Vector(3072.0f, 4096.0f, 73.0f),
		Vector(-1792.0f, 4184.0f, 70.0f),
		Vector(1792.0f, 4184.0f, 70.0f),
		Vector(0.0f, 4240.0f, 70.0f),
	};

	constexpr int COUNT = 34;
	static_assert(COUNT == 34, "BOOST_PADS_LENGTH in rlgym's GameState is 34");

	// Finds the pad index nearest to a world position (XY distance).
	inline int NearestIndex(Vector pos) {
		int best = 0;
		float bestDistSq = FLT_MAX;
		for (int i = 0; i < (int)LOCATIONS.size(); i++) {
			float dx = LOCATIONS[i].X - pos.X;
			float dy = LOCATIONS[i].Y - pos.Y;
			float distSq = dx * dx + dy * dy;
			if (distSq < bestDistSq) {
				bestDistSq = distSq;
				best = i;
			}
		}
		return best;
	}
}
