#pragma once
#include "../Framework.h"

class RosterManager {
public:
	static constexpr int MAX_TEAM_SIZE = 4; // rlgym's StateWrapper caps blue at 1-4, orange at 5-8
	static constexpr int BLUE_ID_START = 1;
	static constexpr int ORANGE_ID_START = 5;
	static const Vector PARK_LOCATION;

	struct Slot {
		int specId = -1;
		int teamNum = 0; // 0 = blue, 1 = orange
		bool active = false; 
		bool spawned = false; 
		bool spawnRequested = false; 
		PriWrapper pri = PriWrapper(0); 
	};

	void ApplyConfig(ServerWrapper server, int teamSize, bool spawnOpponents, int carBodyId, const string& botName);

	void ClaimHumanIfPresent(ServerWrapper server, int teamNum, int primarySpecId, const string& agentName);

	void Update(ServerWrapper server, const string& botName);

	void SetActiveForReset(const vector<int>& specIds);

	vector<Slot> GetActiveSlots() const;
	optional<Slot> FindBySpecId(int specId) const;

	bool AllActiveSlotsSpawned() const;

	void RemoveUnknownBots(ServerWrapper server) const;

	vector<uintptr_t> FindStrayBotAddresses(ServerWrapper server) const;

private:
	void ParkSlot(Slot& slot);

	map<int, Slot> m_slots;
	mutable int m_lastWarnedStrayCount = -1;
};
