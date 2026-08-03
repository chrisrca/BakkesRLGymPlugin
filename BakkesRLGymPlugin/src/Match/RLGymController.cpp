#include "RLGymController.h"

static constexpr float DEFAULT_BOOST_CONSUMPTION_RATE = 0.33333f;

// C4996 is disabled for this function
static void SetBotLoadoutExperimental(GameWrapper* gw, PriWrapper& pri, const BotLoadoutData& data) {
#pragma warning(push)
#pragma warning(disable: 4996)
	gw->SetBotLoadout(pri, data);
#pragma warning(pop)
}

// Builds the bot loadout for every spawned bot: the chosen body, 
// plus the antenna and topper slots explicitly zeroed.
static BotLoadoutData MakeCleanBotLoadout(int carBodyId) {
	BotLoadoutData data;
	data.products[0] = carBodyId;
	data.products[4] = 0; // ANTENNA
	data.products[5] = 0; // TOPPER
	return data;
}

void RLGymController::OnLoad(BakkesMod::Plugin::BakkesModPlugin* plugin) {
	LOG("RLGymController::OnLoad: starting up.");
	m_plugin = plugin;

	// Persist the car body across plugin reloads / game restarts.
	CVarWrapper bodyCvar = plugin->cvarManager->registerCvar("brlgym_car_body", "23",
		"Car body product ID for spawned bots (23=Octane, 4284=Fennec)");
	carBodyId = bodyCvar.getIntValue();
	bodyCvar.addOnValueChanged([this](std::string, CVarWrapper cvar) {
		carBodyId = cvar.getIntValue();
		LOG("RLGymController: car body set to " << carBodyId << " (applies to bots spawned for the next connection).");
	});

	// Bot name and map: same treatment as the car body.
	CVarWrapper nameCvar = plugin->cvarManager->registerCvar("brlgym_bot_name", "Agent",
		"Name applied to spawned bots");
	if (!nameCvar.getStringValue().empty())
		botNamePrefix = nameCvar.getStringValue();
	nameCvar.addOnValueChanged([this](std::string, CVarWrapper cvar) {
		std::string v = cvar.getStringValue();
		if (!v.empty())
			botNamePrefix = v;
	});

	CVarWrapper mapCvar = plugin->cvarManager->registerCvar("brlgym_map", "EuroStadium_Night_P",
		"Map the created match loads");
	if (!mapCvar.getStringValue().empty())
		matchSettings.mapName = mapCvar.getStringValue();
	mapCvar.addOnValueChanged([this](std::string, CVarWrapper cvar) {
		std::string v = cvar.getStringValue();
		if (!v.empty())
			matchSettings.mapName = v;
	});

	m_boostPads.Hook(plugin);

	plugin->gameWrapper->HookEventWithCallerPost<CarWrapper>(
		"Function TAGame.Car_TA.OnHitBall",
		[this](CarWrapper caller, void* params, string eventName) {
			if (caller)
				m_ballTouchedThisStep.insert(caller.memory_address);
		}
	);

	plugin->gameWrapper->HookEvent(
		"Function Engine.GameViewportClient.Tick",
		[this](string eventName) { OnGlobalTick(); }
	);

	// Bot-fill suppression
	plugin->gameWrapper->HookEventPost(
		"Function TAGame.GameEvent_Soccar_TA.InitGame",
		[this](string eventName) { DisableBotFill(eventName); }
	);
	plugin->gameWrapper->HookEventPost(
		"Function TAGame.GameEvent_TA.InitGame",
		[this](string eventName) { DisableBotFill(eventName); }
	);
	
	plugin->cvarManager->registerNotifier("brlgym_local_body",
		[this](std::vector<std::string> args) {
			CarWrapper car = m_plugin->gameWrapper->GetLocalCar();
			if (!car) { LOG("brlgym_local_body: no local car (be in freeplay/a match with the car you want equipped)."); return; }
			LOG("brlgym_local_body: local car GetLoadoutBody() = " << car.GetLoadoutBody());
		}, "Print the local player's equipped car body ID", PERMISSION_ALL);

	plugin->cvarManager->registerNotifier("brlgym_dump_roster",
		[this](std::vector<std::string> args) {
			auto server = m_plugin->gameWrapper->GetGameEventAsServer();
			if (!server) { LOG("brlgym_dump_roster: no server."); return; }
			LOG("brlgym_dump_roster: bRandomizedBotLoadouts=" << server.GetbRandomizedBotLoadouts());
			for (auto& slot : m_roster.GetActiveSlots()) {
				CarWrapper car = slot.pri.GetCar();
				LOG("  specId=" << slot.specId << " team=" << slot.teamNum
					<< " loadoutBody=" << (car ? car.GetLoadoutBody() : -1)
					<< " bLoadoutSet=" << slot.pri.GetbLoadoutSet()
					<< " bLoadoutsSet=" << slot.pri.GetbLoadoutsSet());
			}
		}, "Dump roster slots' loadout state (bot appearance debugging)", PERMISSION_ALL);

	plugin->cvarManager->registerNotifier("brlgym_random_loadouts",
		[this](std::vector<std::string> args) {
			auto server = m_plugin->gameWrapper->GetGameEventAsServer();
			if (!server || args.size() < 2) { LOG("brlgym_random_loadouts: usage: brlgym_random_loadouts <0|1> (needs live match)."); return; }
			unsigned long v = (unsigned long)atoi(args[1].c_str());
			server.SetbRandomizedBotLoadouts(v);
			LOG("brlgym_random_loadouts: SetbRandomizedBotLoadouts(" << v << ").");
		}, "Set the game event's bRandomizedBotLoadouts flag", PERMISSION_ALL);

	plugin->cvarManager->registerNotifier("brlgym_apply_loadout",
		[this](std::vector<std::string> args) {
			auto server = m_plugin->gameWrapper->GetGameEventAsServer();
			if (!server) { LOG("brlgym_apply_loadout: no server."); return; }
			
			for (auto& slot : m_roster.GetActiveSlots()) {
				if (!slot.pri.GetCar() || !slot.pri.GetCar().GetAIController())
					continue;
				slot.pri.SetbLoadoutSet(1);
				slot.pri.SetbLoadoutsSet(1);
				slot.pri.UpdateFromLoadout();
				LOG("brlgym_apply_loadout: specId=" << slot.specId << " flags set + UpdateFromLoadout().");
			}
		}, "Force bots' PRI loadout flags on and refresh appearance from loadout", PERMISSION_ALL);

	plugin->cvarManager->registerNotifier("brlgym_set_bot_loadout",
		[this](std::vector<std::string> args) {
			auto server = m_plugin->gameWrapper->GetGameEventAsServer();
			if (!server) { LOG("brlgym_set_bot_loadout: no server."); return; }
			for (auto pri : server.GetPRIs()) {
				if (!pri || !pri.GetbBot())
					continue;
				BotLoadoutData data = MakeCleanBotLoadout(carBodyId);
				SetBotLoadoutExperimental(m_plugin->gameWrapper.get(), pri, data);
				LOG("brlgym_set_bot_loadout: SetBotLoadout(body=" << carBodyId << ", cosmetics cleared) on bot PRI addr=" << pri.memory_address);
			}
		}, "Re-apply configured car body to all bot PRIs via SetBotLoadout", PERMISSION_ALL);

	plugin->cvarManager->registerNotifier("brlgym_set_archetype",
		[this](std::vector<std::string> args) {
			auto server = m_plugin->gameWrapper->GetGameEventAsServer();
			if (!server || args.size() < 2) { LOG("brlgym_set_archetype: usage: brlgym_set_archetype <specId> (needs live match)."); return; }
			auto slotOpt = m_roster.FindBySpecId(atoi(args[1].c_str()));
			if (!slotOpt || !slotOpt->pri.GetCar()) { LOG("brlgym_set_archetype: no claimed car for that specId."); return; }
			server.SetCarArchetype(slotOpt->pri.GetCar());
			LOG("brlgym_set_archetype: SetCarArchetype(car of specId=" << slotOpt->specId << ") called.");
		}, "EXPERIMENTAL: set game event car archetype from a roster car", PERMISSION_ALL);

	m_connectThread = std::thread([this]() {
		m_pipe.ConnectFromCommandLine();
	});
}

void RLGymController::DisableBotFill(const string& source) {
	auto server = m_plugin->gameWrapper->GetGameEventAsServer();
	if (!server || !server.HasAuthority())
		return;

	int& count = m_disableBotFillLoggedCounts[source];
	if (count < 5) {
		count++;
		LOG("RLGymController::DisableBotFill: SetbFillWithAI(0) (from " << source << ")");
	}

	server.SetbFillWithAI(0);
}

void RLGymController::OnUnload() {
	m_pipe.Disconnect();
	if (m_connectThread.joinable())
		m_connectThread.join();
}

string RLGymController::GetStatusText() const {
	if (!m_pipe.IsConnected())
		return "Waiting for a \"-pipe\" connection from rlgym...";

	std::stringstream ss;
	ss << "Connected to rlgym.";
	if (m_haveConfig) {
		ss << " team_size=" << m_teamSize
			<< " spawn_opponents=" << (m_spawnOpponents ? "true" : "false")
			<< " tick_skip=" << m_tickSkip;
	} else {
		ss << " Waiting for CONFIG message...";
	}
	return ss.str();
}

void RLGymController::OnGlobalTick() {
	RLGymMessage::Message msg;
	while (m_pipe.TryPopMessage(msg))
		HandleMessage(msg);

	TryAutoJoinBlue();
	ProcessPendingBotRemoval();
}

void RLGymController::TryAutoJoinBlue() {
	if (m_autoJoinedBlue || !m_haveConfig)
		return;

	// Must run off the global tick, during the pre-round "CHOOSE TEAM" screen
	auto server = m_plugin->gameWrapper->GetGameEventAsServer();
	if (!server)
		return;

	PlayerControllerWrapper pc = m_plugin->gameWrapper->GetPlayerController();
	if (!pc)
		return;

	// Already on blue
	PriWrapper pri = pc.GetPRI();
	if (pri && (int)pri.GetTeamNum() == 0) {
		LOG("RLGymController::TryAutoJoinBlue: local player already on blue.");
		m_autoJoinedBlue = true;
		return;
	}

	m_autoJoinAttempts++;
	bool ok = server.ChooseTeam(0, pc);
	if (m_autoJoinAttempts <= 3 || ok || m_autoJoinAttempts % 120 == 0)
		LOG("RLGymController::TryAutoJoinBlue: ChooseTeam(0) attempt " << m_autoJoinAttempts
			<< " returned " << (ok ? "true" : "false") << ".");
	if (ok)
		m_autoJoinedBlue = true;
}

void RLGymController::ApplyBotLoadouts(ServerWrapper server) {
	for (auto pri : server.GetPRIs()) {
		if (!pri || !pri.GetbBot() || m_botLoadoutAppliedPris.count(pri.memory_address))
			continue;

		BotLoadoutData data = MakeCleanBotLoadout(carBodyId);
		SetBotLoadoutExperimental(m_plugin->gameWrapper.get(), pri, data);
		m_botLoadoutAppliedPris.insert(pri.memory_address);

		CarWrapper car = pri.GetCar();
		LOG("RLGymController::ApplyBotLoadouts: SetBotLoadout(body=" << carBodyId << ") on bot PRI addr=" << pri.memory_address
			<< (car ? " (car already existed - appearance may not update)" : " (no car yet - should render correctly)"));
	}
}

void RLGymController::ProcessPendingBotRemoval() {
	if (m_strayBotAddressesPendingRemoval.empty())
		return;

	auto server = m_plugin->gameWrapper->GetGameEventAsServer();
	if (!server)
		return;

	// Re-derive who is actually a stray *right now*. PRI memory is pooled and reused,
	// so an address queued a few ticks ago can now belong to a legitimate roster bot -
	// removing it would delete that bot's car and nametag. Only remove addresses that
	// are still strays in a fresh scan.
	auto currentStrays = m_roster.FindStrayBotAddresses(server);
	unordered_set<uintptr_t> stillStray(currentStrays.begin(), currentStrays.end());

	auto pris = server.GetPRIs();
	for (int i = pris.Count() - 1; i >= 0; i--) {
		PriWrapper pri = pris.Get(i);
		if (!pri)
			continue;

		if (!m_strayBotAddressesPendingRemoval.count(pri.memory_address))
			continue;

		if (!stillStray.count(pri.memory_address)) {
			LOG("RLGymController::ProcessPendingBotRemoval: queued addr=" << pri.memory_address
				<< " is no longer a stray (address likely reused by a roster bot), skipping removal.");
			continue;
		}

		LOG("RLGymController::ProcessPendingBotRemoval: removing stray bot PRI queued from OnTick (addr=" << pri.memory_address << ").");
		server.RemovePRI(pri);
	}

	m_strayBotAddressesPendingRemoval.clear();
}

void RLGymController::HandleMessage(const RLGymMessage::Message& msg) {
	RLGymMessage::Type type = RLGymMessage::IdentifyHeader(msg.header);
	switch (type) {
		case RLGymMessage::Type::Config:
			HandleConfig(msg.body);
			break;
		case RLGymMessage::Type::ResetGameState:
			HandleReset(msg.body);
			break;
		case RLGymMessage::Type::AgentActionImmediateResponse:
			HandleAction(msg.body);
			break;
		default:
			break;
	}
}

void RLGymController::HandleConfig(const vector<float>& body) {
	if (body.size() < 6) {
		LOG("RLGymController: CONFIG message too short (" << body.size() << " floats), ignoring.");
		return;
	}

	m_teamSize = (int)body[0];
	m_spawnOpponents = body[1] > 0.5f;
	m_tickSkip = MAX((int)body[2], 1);
	m_gameSpeed = body[3];
	m_gravity = body[4];
	m_boostConsumption = body[5];
	m_haveConfig = true;

	LOG("RLGymController::HandleConfig: team_size=" << m_teamSize
		<< " spawn_opponents=" << m_spawnOpponents
		<< " tick_skip=" << m_tickSkip
		<< " game_speed=" << m_gameSpeed
		<< " gravity=" << m_gravity
		<< " boost_consumption=" << m_boostConsumption);

	if (!m_matchCreateRequested) {
		m_matchCreateRequested = true;
		LOG("RLGymController::HandleConfig: requesting match creation.");
		MatchSetup::CreateMatch(m_plugin, matchSettings);
	}
}

void RLGymController::ApplyPhysicsSettings(ServerWrapper server) {
	float effectiveGameSpeed = CLAMP(m_gameSpeed, 0.1f, 10.0f);
	if (!m_physicsSettingsLoggedOnce) {
		LOG("RLGymController::ApplyPhysicsSettings: first call, SetGameSpeed(" << effectiveGameSpeed << ") (raw game_speed=" << m_gameSpeed << ")");
		m_physicsSettingsLoggedOnce = true;
	}
	server.SetGameSpeed(effectiveGameSpeed);

	bool infiniteBoost = m_boostConsumption <= 0.0001f;
	if (infiniteBoost != m_infiniteBoostActive) {
		LOG("RLGymController::ApplyPhysicsSettings: infinite boost " << (infiniteBoost ? "ON" : "OFF")
			<< " (boost_consumption=" << m_boostConsumption << ").");
		m_infiniteBoostActive = infiniteBoost;
	}

	for (auto& slot : m_roster.GetActiveSlots()) {
		CarWrapper car = slot.pri.GetCar();
		if (!car)
			continue;
		if (auto boost = car.GetBoostComponent()) {
			// rlgym's boost_consumption is a multiplier where 1.0 == normal, but
			// SetBoostConsumptionRate wants an absolute rate
			boost.SetBoostConsumptionRate(m_boostConsumption * DEFAULT_BOOST_CONSUMPTION_RATE);
			if (infiniteBoost)
				boost.SetBoostAmount(1.0f);
		}
	}
}

void RLGymController::HandleReset(const vector<float>& body) {
	GameStateCodec::ResetState reset;
	if (!GameStateCodec::DecodeResetState(body, reset)) {
		LOG("RLGymController: Malformed RESET_GAME_STATE body (" << body.size() << " floats), ignoring.");
		return;
	}

	m_pendingReset = std::move(reset);
	m_pendingInstr = PendingInstr::Reset;
}

void RLGymController::HandleAction(const vector<float>& body) {
	constexpr int ACTION_LEN = 9; // id + throttle,steer,pitch,yaw,roll,jump,boost,handbrake

	if (body.size() % ACTION_LEN != 0) {
		LOG("RLGymController: Malformed action body (" << body.size() << " floats), ignoring.");
		return;
	}

	for (size_t pos = 0; pos + ACTION_LEN <= body.size(); pos += ACTION_LEN) {
		int specId = (int)body[pos];

		CarAction action;
		action.throttle = body[pos + 1];
		action.steer = body[pos + 2];
		action.pitch = body[pos + 3];
		action.yaw = body[pos + 4];
		action.roll = body[pos + 5];
		action.jump = body[pos + 6];
		action.boost = body[pos + 7];
		action.handbrake = body[pos + 8];

		m_currentActions[specId] = action;
	}

	if (!m_firstActionLogged) {
		LOG("RLGymController::HandleAction: first action message received (" << (body.size() / ACTION_LEN) << " car(s)); further ones won't be logged individually.");
		m_firstActionLogged = true;
	}

	m_pendingInstr = PendingInstr::Action;
}

void RLGymController::ApplyResetState(ServerWrapper server, const GameStateCodec::ResetState& reset) {
	vector<int> specIds;
	for (auto& carTarget : reset.cars)
		specIds.push_back(carTarget.specId);
	m_roster.SetActiveForReset(specIds);

	if (auto ball = server.GetBall()) {
		auto rbs = ball.GetRBState();
		rbs.Location = reset.ballPos;
		rbs.LinearVelocity = reset.ballVel;
		rbs.AngularVelocity = reset.ballAngVel;
		ball.SetRBState(rbs);
		ball.SetPhysicsState(rbs);
	}

	for (auto& carTarget : reset.cars) {
		auto slotOpt = m_roster.FindBySpecId(carTarget.specId);
		if (!slotOpt) {
			LOG("RLGymController::ApplyResetState: no roster slot for specId=" << carTarget.specId << ", skipping.");
			continue;
		}

		CarWrapper car = slotOpt->pri.GetCar();
		if (!car) {
			LOG("RLGymController::ApplyResetState: specId=" << carTarget.specId << " has no car yet, skipping.");
			continue;
		}

		if (car.GetbHidden())
			car.RespawnInPlace();

		Vector teleportPos = carTarget.pos;
		Rotator teleportRot;
		car.Teleport(teleportPos, teleportRot, /*bStopVelocity=*/1, /*bUpdateRotation=*/0, 0.0f);

		auto rbs = car.GetRBState();
		rbs.Location = carTarget.pos;
		rbs.LinearVelocity = carTarget.vel;
		rbs.AngularVelocity = carTarget.angVel;
		rbs.Quaternion = Math::EulerToQuat(carTarget.pitch, carTarget.yaw, carTarget.roll);
		car.SetRBState(rbs);
		car.SetPhysicsState(rbs);

		if (auto boost = car.GetBoostComponent())
			boost.SetBoostAmount(carTarget.boost);
	}

	m_boostPads.ResetAllActive();
	m_ballTouchedThisStep.clear();
}

void RLGymController::SendState(ServerWrapper server) {
	auto ball = server.GetBall();
	RBState ballState = ball ? ball.GetRBState() : RBState{};

	int blueScore = 0, orangeScore = 0;
	for (auto team : server.GetTeams()) {
		if (!team)
			continue;
		if (team.GetTeamNum() == 0)
			blueScore = team.GetScore();
		else if (team.GetTeamNum() == 1)
			orangeScore = team.GetScore();
	}

	vector<GameStateCodec::PlayerStateSnapshot> players;
	for (auto& slot : m_roster.GetActiveSlots()) {
		CarWrapper car = slot.pri.GetCar();
		if (!car)
			continue;

		GameStateCodec::PlayerStateSnapshot p;
		p.specId = slot.specId;
		p.teamNum = slot.teamNum;
		p.carState = car.GetRBState();
		p.matchGoals = slot.pri.GetMatchGoals();
		p.matchSaves = slot.pri.GetMatchSaves();
		p.matchShots = slot.pri.GetMatchShots();
		p.matchDemolishes = slot.pri.GetMatchDemolishes();
		p.boostPickups = slot.pri.GetBoostPickups();
		p.isDemoed = car.GetbHidden() != 0; // No direct "IsDemolished"; demolished cars are hidden until respawn.
		p.onGround = car.IsOnGround();
		p.ballTouched = m_ballTouchedThisStep.count(car.memory_address) > 0;
		p.hasJump = car.GetbJumped() == 0;       // "has_jump" = jump still available, i.e. hasn't jumped yet.
		p.hasFlip = car.GetbDoubleJumped() == 0; // "has_flip" = dodge still available, i.e. hasn't double-jumped/dodged yet.

		if (auto boost = car.GetBoostComponent())
			p.boostAmount = boost.GetCurrentBoostAmount();

		players.push_back(p);
	}

	auto body = GameStateCodec::EncodeState(
		m_ticksSinceInstr,
		blueScore, orangeScore,
		m_boostPads.GetActiveStates(),
		ballState,
		players
	);

	RLGymMessage::Message msg;
	msg.header = RLGymMessage::STATE_MESSAGE_HEADER;
	msg.body = body;
	m_pipe.Send(msg);

	m_ballTouchedThisStep.clear();
}

void RLGymController::OnTick(ServerWrapper server) {
	if (!server || !m_haveConfig)
		return;

	if (!server.GetbRoundActive()) {
		if (m_roundActiveTicks != 0)
			LOG("RLGymController::OnTick: round went inactive again (was active for " << m_roundActiveTicks << " ticks).");
		m_roundActiveTicks = 0;
		return;
	}

	if (m_roundActiveTicks == 0)
		LOG("RLGymController::OnTick: round is now active, starting settle countdown.");
	m_roundActiveTicks++;

	// Exhibition-match rules: unlimited time, no goal reset/kickoff interruption.
	// Doesn't touch PRIs/cars, so safe to apply as soon as the round is active.
	if (!m_matchRulesApplied) {
		LOG("RLGymController::OnTick: applying match rules (SetbUnlimitedTime, DisableGoalReset, SetBotSkill2).");
		server.SetbUnlimitedTime(1);
		server.DisableGoalReset();
		server.SetBotSkill2(0.0f);

		// Force the team names to plain BLUE/ORANGE. 
		for (auto team : server.GetTeams()) {
			if (!team)
				continue;
			int tn = team.GetTeamNum();
			if (tn == 0)
				team.SetCustomTeamName("BLUE");
			else if (tn == 1)
				team.SetCustomTeamName("ORANGE");
		}

		m_matchRulesApplied = true;
		LOG("RLGymController::OnTick: match rules applied.");
	}

	constexpr int ROSTER_SETTLE_TICKS = 60;
	if (m_roundActiveTicks < ROSTER_SETTLE_TICKS)
		return;

	if (!m_rosterAppliedForThisMatch) {
		LOG("RLGymController::OnTick: settle period elapsed, claiming human PRIs and applying roster config.");

		m_roster.ClaimHumanIfPresent(server, 0, RosterManager::BLUE_ID_START, botNamePrefix);
		m_roster.ClaimHumanIfPresent(server, 1, RosterManager::ORANGE_ID_START, botNamePrefix);

		m_roster.ApplyConfig(server, m_teamSize, m_spawnOpponents, carBodyId, botNamePrefix);
		m_rosterAppliedForThisMatch = true;
		LOG("RLGymController::OnTick: roster config applied.");

		m_plugin->cvarManager->executeCommand("cl_anonymizer_mode_team 0", false);
		m_plugin->cvarManager->executeCommand("cl_anonymizer_mode_opponent 0", false);
	}

	bool rosterComplete = m_roster.AllActiveSlotsSpawned();

	ApplyPhysicsSettings(server);

	ApplyBotLoadouts(server);

	m_boostPads.Update();
	m_roster.Update(server, botNamePrefix);
	if (rosterComplete) {
		m_roster.RemoveUnknownBots(server);
		for (uintptr_t addr : m_roster.FindStrayBotAddresses(server))
			m_strayBotAddressesPendingRemoval.insert(addr);
	}

	m_carAddressToSpecId.clear();
	for (auto& slot : m_roster.GetActiveSlots()) {
		CarWrapper car = slot.pri.GetCar();
		if (car)
			m_carAddressToSpecId[car.memory_address] = slot.specId;
	}

	if (m_pendingInstr == PendingInstr::Reset) {
		bool allSlotsReady = true;
		for (auto& carTarget : m_pendingReset.cars) {
			if (!m_roster.FindBySpecId(carTarget.specId)) {
				allSlotsReady = false;
				break;
			}
		}

		if (allSlotsReady) {
			m_ticksWaitingForRosterReset = 0;
			ApplyResetState(server, m_pendingReset);
			m_pendingInstr = PendingInstr::None;
			m_ticksSinceInstr = 0;
			SendState(server);
			m_awaitingState = false;
		} else {
			m_ticksWaitingForRosterReset++;
			if (m_ticksWaitingForRosterReset == 240) { 
				LOG("RLGymController::OnTick: still waiting on roster slots for a queued reset 2s later - "
					"a specId the reset references may never get claimed (a SpawnBot call silently failed?).");
			}
		}
		return;
	}

	if (m_pendingInstr == PendingInstr::Action) {
		m_pendingInstr = PendingInstr::None;
		m_ticksSinceInstr = 0;
		m_awaitingState = true;
	}

	if (m_awaitingState) {
		m_ticksSinceInstr++;
		if (m_ticksSinceInstr >= m_tickSkip) {
			SendState(server);
			m_awaitingState = false;
		}
	}
}

void RLGymController::OnVehicleInputSet(CarWrapper car, ControllerInput* inputs) {
	if (!car)
		return;

	auto it = m_carAddressToSpecId.find(car.memory_address);
	if (it == m_carAddressToSpecId.end()) {
		*inputs = {};
		return;
	}

	auto actionIt = m_currentActions.find(it->second);
	if (actionIt == m_currentActions.end()) {
		*inputs = {};
		return;
	}

	const CarAction& action = actionIt->second;

	if (m_vehicleInputSetLoggedCount < 10) {
		m_vehicleInputSetLoggedCount++;
		LOG("RLGymController::OnVehicleInputSet: applying action to specId=" << it->second
			<< " throttle=" << action.throttle << " steer=" << action.steer << " boost=" << action.boost);
	}

	// Suppress the empty-tank boost sound
	float boostInput = action.boost;
	if (boostInput > 0.0f) {
		if (auto boost = car.GetBoostComponent()) {
			if (boost.GetCurrentBoostAmount() < 0.001f)
				boostInput = 0.0f;
		}
	}

	inputs->Throttle = action.throttle;
	inputs->Steer = action.steer;
	inputs->Yaw = action.yaw;
	inputs->Pitch = action.pitch;
	inputs->Roll = action.roll;
	inputs->DodgeForward = -inputs->Pitch;
	inputs->DodgeStrafe = inputs->Yaw;
	inputs->Jump = action.jump;
	inputs->ActivateBoost = inputs->HoldingBoost = boostInput;
	inputs->Handbrake = action.handbrake;
}
