#include "Framework.h"

#include "../../libsrc/imgui/imgui.h"
#include "Match/RLGymController.h"

class BakkesRLGymPlugin : public BakkesMod::Plugin::BakkesModPlugin, public BakkesMod::Plugin::PluginSettingsWindow {
public:
	RLGymController controller;

	void hkOnTick(string eventName);
	void hkOnSetVehicleInput(CarWrapper car, void* params, string eventName);

	virtual void onLoad();
	virtual void onUnload();

	void RenderSettings() override;
	string GetPluginName() override { return "BakkesRLGymPlugin"; };
	void SetImGuiContext(uintptr_t ctx) override;

	float m_lastWorldTime = -1;
};

BAKKESMOD_PLUGIN(BakkesRLGymPlugin, "BakkesRLGymPlugin", BRGP_VERSION, PLUGINTYPE_FREEPLAY)

void BakkesRLGymPlugin::hkOnTick(string eventName) {
	auto server = gameWrapper->GetGameEventAsServer();
	if (!server)
		return;

	float curWorldTime = server.GetWorldInfo().GetTimeSeconds();
	if (m_lastWorldTime == curWorldTime)
		return; // Already processed this physics tick

	m_lastWorldTime = curWorldTime;
	controller.OnTick(server);
}

void BakkesRLGymPlugin::hkOnSetVehicleInput(CarWrapper car, void* params, string eventName) {
	auto server = gameWrapper->GetGameEventAsServer();
	if (!server)
		return;

	controller.OnVehicleInputSet(car, (ControllerInput*)params);
}

void BakkesRLGymPlugin::onLoad() {
	g_PluginInstMutex.lock();
	g_PluginInst = this;
	g_PluginInstMutex.unlock();

	gameWrapper->HookEventPost(
		"Function TAGame.Car_TA.SetVehicleInput",
		std::bind(&BakkesRLGymPlugin::hkOnTick, this, std::placeholders::_1)
	);
	gameWrapper->HookEventWithCaller<CarWrapper>(
		"Function TAGame.Car_TA.SetVehicleInput",
		std::bind(&BakkesRLGymPlugin::hkOnSetVehicleInput, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)
	);

	controller.OnLoad(this);
}

void BakkesRLGymPlugin::onUnload() {
	controller.OnUnload();

	g_PluginInstMutex.lock();
	g_PluginInst = NULL;
	g_PluginInstMutex.unlock();
}

void BakkesRLGymPlugin::RenderSettings() {
	constexpr const char* DIVIDER = "\n\n======================\n\n";

	{ // Instructions
		string instructions = STR(
			"BakkesRLGymPlugin - drop-in replacement transport for rlgym's closed-source"
			"\nRLGym.dll, adding car body / map customization."
			"\n\nLaunch your rlgym training script as usual (it passes \"-pipe <id>\" to"
			"\nRocket League) - this plugin will connect automatically, create the match"
			"\nitself using the settings below, and drive it from there. Nothing on the"
			"\nPython side needs to change."
		);
		instructions += DIVIDER;
		ImGui::Text(instructions.c_str());
	}

	{ // Status
		string status = controller.GetStatusText();
		status += DIVIDER;
		ImGui::Text(status.c_str());
	}

	{ // Car body
		CVarWrapper bodyCvar = cvarManager->getCvar("brlgym_car_body");
		if (bodyCvar) {
			int carBodyId = bodyCvar.getIntValue();
			if (ImGui::InputInt("Car Body ID (23=Octane, 4284=Fennec)", &carBodyId))
				bodyCvar.setValue(MAX(carBodyId, 0)); // fires the change handler -> updates controller + saves to cfg
		}
	}

	// Bot name and map carry across restarts
	auto renderCvarText = [this](const char* label, const char* cvarName) {
		CVarWrapper cvar = cvarManager->getCvar(cvarName);
		if (!cvar)
			return;
		constexpr int BUFFER_SIZE = 256;
		static std::map<std::string, std::array<char, BUFFER_SIZE>> buffers;
		auto& buf = buffers[cvarName];
		static std::set<std::string> seeded;
		if (!seeded.count(cvarName)) {
			std::string v = cvar.getStringValue();
			size_t n = MIN(v.size(), (size_t)(BUFFER_SIZE - 1));
			memcpy(buf.data(), v.c_str(), n);
			buf[n] = '\0';
			seeded.insert(cvarName);
		}
		if (ImGui::InputText(label, buf.data(), BUFFER_SIZE))
			cvar.setValue(std::string(buf.data()));
	};

	renderCvarText("Bot Name", "brlgym_bot_name");
	renderCvarText("Map", "brlgym_map");

	{ // Mutators / GameTags
		constexpr int BUFFER_SIZE = 512;
		static char tagsBuf[BUFFER_SIZE] = "";
		ImGui::Text("Extra mutators (GameTags, comma separated), appended to the built-in"
			"\nBotsNone,UnlimitedTime,DisableGoalDelay,PlayerCount8");
		if (ImGui::InputText("##GameTags", tagsBuf, BUFFER_SIZE))
			controller.matchSettings.gameTags = tagsBuf;
	}

	ImGui::Text(STR(
		"Note: this only applies to the match created for the FIRST connection."
		"\nReconnect (restart your training script) after changing these to rebuild the match."
	).c_str());
}

void BakkesRLGymPlugin::SetImGuiContext(uintptr_t ctx) {
	ImGui::SetCurrentContext((ImGuiContext*)ctx);
}
