#include "MatchSetup.h"

void MatchSetup::CreateMatch(BakkesMod::Plugin::BakkesModPlugin* plugin, const Settings& settings) {
	string gameTags = "BotsNone,UnlimitedTime,DisableGoalDelay,PlayerCount8";
	if (!settings.gameTags.empty())
		gameTags += "," + settings.gameTags;

	std::stringstream cmd;
	cmd << "sleep 10000; unreal_command \"start " << settings.mapName
		<< "?game=TAGame.GameInfo_Soccar_TA"
		<< "?Playtest"
		<< "?GameTags=" << gameTags
		<< "?NumPublicConnections=8"
		<< "?NumOpenPublicConnections=8"
		<< "?Offline\"";

	LOG("MatchSetup: executing: " << cmd.str());
	plugin->cvarManager->executeCommand(cmd.str(), false);
	LOG("MatchSetup: executeCommand returned.");
}
