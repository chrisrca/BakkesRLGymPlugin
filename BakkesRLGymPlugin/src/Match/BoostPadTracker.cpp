#include "BoostPadTracker.h"

void BoostPadTracker::ResetAllActive() {
	for (int i = 0; i < BoostLocations::COUNT; i++) {
		m_active[i] = true;
		m_pendingRespawn[i] = false;
	}
}

void BoostPadTracker::Hook(BakkesMod::Plugin::BakkesModPlugin* plugin) {
	ResetAllActive();

	plugin->gameWrapper->HookEventWithCallerPost<ActorWrapper>(
		PICKUP_HOOK_EVENT,
		[this](ActorWrapper caller, void* params, string eventName) {
			this->OnPickup(caller, params, eventName);
		}
	);
}

void BoostPadTracker::OnPickup(ActorWrapper caller, void* params, string eventName) {
	if (!caller)
		return;

	BoostPickupWrapper pickup(caller.memory_address);
	if (!pickup)
		return;

	int index = BoostLocations::NearestIndex(pickup.GetLocation());
	LOG("BoostPadTracker::OnPickup: pad index=" << index << " picked up.");

	m_active[index] = false;
	m_pendingRespawn[index] = true;

	float respawnDelay = pickup.GetRespawnDelay();
	if (respawnDelay <= 0.0f)
		respawnDelay = pickup.GetbPickedUp() ? 10.0f : 4.0f; // Big vs small pad fallback if the delay field reads 0 before it's set.

	m_respawnAt[index] = CURTIME() + std::chrono::duration_cast<time_point::duration>(std::chrono::duration<float>(respawnDelay));
}

void BoostPadTracker::Update() {
	auto now = CURTIME();
	for (int i = 0; i < BoostLocations::COUNT; i++) {
		if (m_pendingRespawn[i] && now >= m_respawnAt[i]) {
			m_active[i] = true;
			m_pendingRespawn[i] = false;
		}
	}
}
