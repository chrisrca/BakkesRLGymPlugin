#pragma once
#include "../Framework.h"

namespace MatchSetup {
	struct Settings {
		string mapName = "EuroStadium_Night_P"; // Default map

		// EXTRA mutator tags (comma separated), appended after the fixed
		// baked-in set (BotsNone,UnlimitedTime,DisableGoalDelay,PlayerCount8)
		// that mirrors RLGym.dll. Tag table:
		// https://bakkesmod.fandom.com/wiki/Unreal_command
		string gameTags = "";
	};

	void CreateMatch(BakkesMod::Plugin::BakkesModPlugin* plugin, const Settings& settings);
}
