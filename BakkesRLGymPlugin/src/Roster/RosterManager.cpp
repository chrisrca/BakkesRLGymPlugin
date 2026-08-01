#include "RosterManager.h"

// Currently unused cars
const Vector RosterManager::PARK_LOCATION = Vector(0, 6500, -300);

void RosterManager::ApplyConfig(ServerWrapper server, int teamSize, bool spawnOpponents, int carBodyId, const string& botName) {
	if (!server)
		return;

	int desiredBlue = CLAMP(teamSize, 0, MAX_TEAM_SIZE);
	int desiredOrange = spawnOpponents ? CLAMP(teamSize, 0, MAX_TEAM_SIZE) : 0;
	LOG("RosterManager::ApplyConfig: desiredBlue=" << desiredBlue << " desiredOrange=" << desiredOrange << " carBodyId=" << carBodyId);

	for (int i = 0; i < MAX_TEAM_SIZE; i++) {
		int specId = BLUE_ID_START + i;
		Slot& slot = m_slots[specId];
		if (slot.specId < 0) {
			slot.specId = specId;
			slot.teamNum = 0;
		}

		slot.active = i < desiredBlue;
		if (!slot.active && slot.spawned)
			ParkSlot(slot);
	}

	for (int i = 0; i < MAX_TEAM_SIZE; i++) {
		int specId = ORANGE_ID_START + i;
		Slot& slot = m_slots[specId];
		if (slot.specId < 0) {
			slot.specId = specId;
			slot.teamNum = 1;
		}

		slot.active = i < desiredOrange;
		if (!slot.active && slot.spawned)
			ParkSlot(slot);
	}

	if (!server.CanSpawnBots())
		LOG("RosterManager::ApplyConfig: WARNING - CanSpawnBots() is false, SpawnBot calls below may be ignored.");

	for (auto& [specId, slot] : m_slots) {
		if (!slot.active || slot.spawned || slot.spawnRequested)
			continue;

		LOG("RosterManager::ApplyConfig: SpawnBot(body=" << carBodyId << ") for specId=" << specId << " team=" << slot.teamNum);
		server.SpawnBot(carBodyId, botName);
		slot.spawnRequested = true;
	}
}

void RosterManager::ParkSlot(Slot& slot) {
	if (!slot.spawned || !slot.pri)
		return;

	CarWrapper car = slot.pri.GetCar();
	if (!car)
		return;

	auto rbs = car.GetRBState();
	rbs.Location = PARK_LOCATION + Vector((float)(slot.specId * 300), 0, 0);
	rbs.LinearVelocity = Vector(0, 0, 0);
	rbs.AngularVelocity = Vector(0, 0, 0);
	car.SetRBState(rbs);
	car.SetPhysicsState(rbs);

	if (auto boost = car.GetBoostComponent())
		boost.SetBoostAmount(0);
}

void RosterManager::ClaimHumanIfPresent(ServerWrapper server, int teamNum, int primarySpecId, const string& agentName) {
	if (!server)
		return;

	Slot& slot = m_slots[primarySpecId];
	if (slot.specId < 0) {
		slot.specId = primarySpecId;
		slot.teamNum = teamNum;
	}

	if (slot.spawned)
		return; // Already claimed, by a bot or a previous call here.

	for (auto pri : server.GetPRIs()) {
		if (!pri)
			continue;

		CarWrapper car = pri.GetCar();
		if (!car || car.GetAIController() || !car.GetPlayerController())
			continue; // Not spawned yet, or it's a bot, not a human.

		auto team = pri.GetTeam();
		if (!team || team.GetTeamNum() != teamNum)
			continue;

		slot.pri = pri;
		slot.spawned = true;
		LOG("RosterManager::ClaimHumanIfPresent: found human PRI for team " << teamNum << ", renaming to \"" << agentName << "\"...");
		pri.eventSetPlayerName(agentName);

		LOG("RosterManager: Claimed human player's car on team " << teamNum << " as spec id " << primarySpecId << ".");
		return;
	}
}

void RosterManager::Update(ServerWrapper server, const string& botName) {
	if (!server)
		return;

	unordered_set<uintptr_t> claimed;
	for (const auto& [specId, slot] : m_slots) {
		if (slot.spawned && slot.pri)
			claimed.insert(slot.pri.memory_address);
	}

	for (auto pri : server.GetPRIs()) {
		if (!pri || claimed.count(pri.memory_address))
			continue;

		CarWrapper car = pri.GetCar();
		if (!car || !car.GetAIController())
			continue; // Not spawned yet, or a human (claimed separately by ClaimHumanIfPresent).

		auto team = pri.GetTeam();
		if (!team)
			continue;
		int teamNum = team.GetTeamNum();
		if (teamNum != 0 && teamNum != 1)
			continue;

		// Only claim into a slot ApplyConfig actually requested a spawn for active AND not yet spawned
		auto findFreeSlot = [this](int wantedTeam) -> int {
			int idStart = (wantedTeam == 0) ? BLUE_ID_START : ORANGE_ID_START;
			for (int i = 0; i < MAX_TEAM_SIZE; i++) {
				int specId = idStart + i;
				auto it = m_slots.find(specId);
				if (it != m_slots.end() && it->second.active && !it->second.spawned)
					return specId;
			}
			return -1;
		};

		int freeSpecId = findFreeSlot(teamNum);
		if (freeSpecId < 0) {
			int otherTeam = 1 - teamNum;
			freeSpecId = findFreeSlot(otherTeam);
			if (freeSpecId < 0)
				continue; // No active slot on either team wants a bot right now.

			LOG("RosterManager::Update: bot PRI auto-balanced onto team " << teamNum
				<< " but only team " << otherTeam << " has a free slot - ServerChangeTeam(" << otherTeam << ").");
			pri.ServerChangeTeam(otherTeam);
			teamNum = otherTeam;
		}

		Slot& slot = m_slots[freeSpecId];
		slot.specId = freeSpecId;
		slot.teamNum = teamNum;
		slot.pri = pri;
		slot.spawned = true;

		LOG("RosterManager::Update: claiming spawned bot PRI for specId=" << freeSpecId << " team=" << teamNum
			<< " loadoutBody=" << car.GetLoadoutBody() << ", renaming.");
		pri.eventSetPlayerName(botName);

		claimed.insert(pri.memory_address);
	}

	for (auto& [specId, slot] : m_slots) {
		if (slot.spawned && !slot.active)
			ParkSlot(slot);
	}
}

void RosterManager::SetActiveForReset(const vector<int>& specIds) {
	unordered_set<int> wanted(specIds.begin(), specIds.end());

	for (auto& [specId, slot] : m_slots) {
		if (!slot.spawned)
			continue;

		bool shouldBeActive = wanted.count(specId) > 0;
		if (shouldBeActive == slot.active)
			continue;

		slot.active = shouldBeActive;
		if (!shouldBeActive)
			ParkSlot(slot); 
	}
}

vector<RosterManager::Slot> RosterManager::GetActiveSlots() const {
	vector<Slot> result;
	for (const auto& [specId, slot] : m_slots) {
		if (slot.active && slot.spawned)
			result.push_back(slot);
	}
	return result;
}

bool RosterManager::AllActiveSlotsSpawned() const {
	for (const auto& [specId, slot] : m_slots) {
		if (slot.active && !slot.spawned)
			return false;
	}
	return true;
}

optional<RosterManager::Slot> RosterManager::FindBySpecId(int specId) const {
	auto it = m_slots.find(specId);
	if (it == m_slots.end() || !it->second.spawned)
		return std::nullopt;
	return it->second;
}

vector<uintptr_t> RosterManager::FindStrayBotAddresses(ServerWrapper server) const {
	vector<uintptr_t> strays;
	if (!server)
		return strays;

	unordered_set<uintptr_t> known;
	for (const auto& [specId, slot] : m_slots) {
		if (slot.spawned && slot.pri)
			known.insert(slot.pri.memory_address);
	}

	for (auto pri : server.GetPRIs()) {
		if (!pri || known.count(pri.memory_address))
			continue;

		CarWrapper car = pri.GetCar();
		if (car && car.GetAIController())
			strays.push_back(pri.memory_address);
	}
	return strays;
}

void RosterManager::RemoveUnknownBots(ServerWrapper server) const {
	if (!server)
		return;

	int strayCount = (int)FindStrayBotAddresses(server).size();

	if (strayCount != m_lastWarnedStrayCount) {
		if (strayCount > 0) {
			LOG("RosterManager: " << strayCount << " extra AI-controlled car(s) present that aren't part of our roster. "
				"Queuing for removal on the next global tick.");
		}
		m_lastWarnedStrayCount = strayCount;
	}
}
