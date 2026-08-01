#pragma once
#include "../Framework.h"
#include "BoostLocations.h"

// Tracks which of the 34 boost pad slots (BoostLocations::LOCATIONS,
// matching rlgym's common_values.BOOST_LOCATIONS) are currently active.
class BoostPadTracker {
public:
	static constexpr const char* PICKUP_HOOK_EVENT = "Function TAGame.VehiclePickup_TA.Pickup";

	void Hook(BakkesMod::Plugin::BakkesModPlugin* plugin);

	// Reactivate pads whose respawn delay has elapsed.
	void Update();

	// Resets all pads to active
	void ResetAllActive();

	// Returns a 34-length array, index-aligned with BoostLocations::LOCATIONS /
	// rlgym's BOOST_LOCATIONS, true = available for pickup.
	const bool* GetActiveStates() const { return m_active; }

private:
	void OnPickup(ActorWrapper caller, void* params, string eventName);

	bool m_active[BoostLocations::COUNT] = {};
	time_point m_respawnAt[BoostLocations::COUNT] = {};
	bool m_pendingRespawn[BoostLocations::COUNT] = {};
};
