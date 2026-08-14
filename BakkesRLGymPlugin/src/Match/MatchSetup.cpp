#include "MatchSetup.h"

void MatchSetup::CreateMatch(BakkesMod::Plugin::BakkesModPlugin* plugin, const Settings& settings, int startDelayMs) {
	string gameTags = "BotsNone,UnlimitedTime,DisableGoalDelay,PlayerCount8";
	if (!settings.gameTags.empty())
		gameTags += "," + settings.gameTags;

	std::stringstream cmd;
	if (startDelayMs > 0)
		cmd << "sleep " << startDelayMs << "; ";
	cmd << "unreal_command \"start " << settings.mapName
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
