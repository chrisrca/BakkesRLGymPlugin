#pragma once
#include "../Framework.h"
#include "../Pipe/PipeClient.h"
#include "../Protocol/Message.h"
#include "../Protocol/GameStateCodec.h"
#include "../Roster/RosterManager.h"
#include "BoostPadTracker.h"
#include "MatchSetup.h"
#include "CarAction.h"

class RLGymController {
public:
	MatchSetup::Settings matchSettings;
	int carBodyId = 23; // Octane
	string botNamePrefix = "Agent";

	void OnLoad(BakkesMod::Plugin::BakkesModPlugin* plugin);
	void OnUnload();

	void OnGlobalTick();
	void OnTick(ServerWrapper server);
	void OnVehicleInputSet(CarWrapper car, ControllerInput* inputs);

	bool IsPipeConnected() const { return m_pipe.IsConnected(); }
	string GetStatusText() const;

private:
	void HandleMessage(const RLGymMessage::Message& msg);
	void HandleConfig(const vector<float>& body);
	void HandleReset(const vector<float>& body);
	void HandleAction(const vector<float>& body);

	void ApplyPhysicsSettings(ServerWrapper server);
	void ApplyResetState(ServerWrapper server, const GameStateCodec::ResetState& reset);
	void SendState(ServerWrapper server);

	// Bot-fill suppression
	void DisableBotFill(const string& source);

	void ProcessPendingBotRemoval();

	// Joins the local player to blue from the pre-round "CHOOSE TEAM" screen 
	void TryAutoJoinBlue();

	// Applies BotLoadoutData
	void ApplyBotLoadouts(ServerWrapper server);

	BakkesMod::Plugin::BakkesModPlugin* m_plugin = nullptr;

	PipeClient m_pipe;
	RosterManager m_roster;
	BoostPadTracker m_boostPads;

	std::thread m_connectThread;

	bool m_haveConfig = false;
	int m_teamSize = 1;
	bool m_spawnOpponents = false;
	int m_tickSkip = 8;
	float m_gameSpeed = 1;
	float m_gravity = 1;
	float m_boostConsumption = 1;
	bool m_infiniteBoostActive = false;

	bool m_matchCreateRequested = false;
	bool m_autoJoinedBlue = false;
	int m_autoJoinAttempts = 0;   
	bool m_rosterAppliedForThisMatch = false;
	bool m_matchRulesApplied = false;
	int m_roundActiveTicks = 0;
	bool m_physicsSettingsLoggedOnce = false;
	bool m_firstActionLogged = false;
	int m_vehicleInputSetLoggedCount = 0;
	unordered_map<string, int> m_disableBotFillLoggedCounts;

	unordered_set<uintptr_t> m_strayBotAddressesPendingRemoval;

	unordered_set<uintptr_t> m_botLoadoutAppliedPris;

	enum class PendingInstr { None, Reset, Action };
	PendingInstr m_pendingInstr = PendingInstr::None;
	GameStateCodec::ResetState m_pendingReset;
	unordered_map<int, CarAction> m_currentActions;
	bool m_awaitingState = false;
	int m_ticksSinceInstr = 0;
	int m_ticksWaitingForRosterReset = 0;

	// Ball-touch tracking for the current step 
	unordered_set<uintptr_t> m_ballTouchedThisStep;

	unordered_map<uintptr_t, int> m_carAddressToSpecId;

	float m_lastWorldTime = -1;
};
