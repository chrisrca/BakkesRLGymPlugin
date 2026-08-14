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

	// startDelayMs: milliseconds to wait before issuing the level-load command. The
	// initial launch needs this so Rocket League finishes coming up first; a
	// mid-session map rotation passes 0 (the game is already running).
	void CreateMatch(BakkesMod::Plugin::BakkesModPlugin* plugin, const Settings& settings, int startDelayMs = 10000);
}
