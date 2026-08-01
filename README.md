# BakkesRLGymPlugin

An open-source, drop-in replacement for the **closed-source `RLGym.dll`** that [RLGym](https://rlgym.org/) uses to talk to Rocket League.

[![Build](https://github.com/chrisrca/BakkesRLGymPlugin/actions/workflows/build.yml/badge.svg)](https://github.com/chrisrca/BakkesRLGymPlugin/actions/workflows/build.yml)
[![Platform](https://img.shields.io/badge/platform-Windows%20x64-blue)](#building)
[![BakkesMod](https://img.shields.io/badge/BakkesMod-plugin-9cf)](https://bakkesmod.com/)

RLGym trains reinforcement-learning agents inside Rocket League. To do that, its Python side spins up the game and drives it through a native DLL that ships as a compiled binary with no source. **BakkesRLGymPlugin re-implements that** providing an open-source version of that bridge while adding car-body and map customization on top.

> **Nothing on the Python side has to change.** Point RLGym at Rocket League the way you always have and add BakkesRLGymPlugin to your plugin folder; this plugin connects itself, builds the match, and drives it. <sub><sup>Note: If you are having difficulties starting Rocket League with rlgym Post-EAC I have provided rlgym v1.2.2 in this repository with updated launch mechanisms</sub></sup>

<img width="1920" height="1080" alt="image" src="https://github.com/user-attachments/assets/bcca8651-e3fc-4b0a-9bcf-ee466da81e15" />

## Installation

**Prerequisites**

- Windows (x64)
- [BakkesMod](https://bakkesmod.com/) installed and working with Rocket League
- A working [RLGym](https://rlgym.org/) Python setup

**Steps**

1. Grab `BakkesRLGymPlugin.dll` — either download it from the [latest CI build artifact](https://github.com/chrisrca/BakkesRLGymPlugin/actions) or [build it yourself](#building).
2. Copy the DLL into your BakkesMod plugins folder and remove RLGym.dll if it exists:
   ```
   %APPDATA%\bakkesmod\bakkesmod\plugins\
   ```
3. Tell BakkesMod to load it on startup by adding this line to
   `%APPDATA%\bakkesmod\bakkesmod\cfg\plugins.cfg`:
   ```
   plugin load BakkesRLGymPlugin
   ```
   (or load it once from the BakkesMod console with `plugin load BakkesRLGymPlugin`).

That's it — you don't inject the stock `RLGym.dll` anymore; BakkesMod loads this plugin instead.

## Usage

Run your RLGym training script exactly as you normally would:

```python
import rlgym

env = rlgym.make(
    team_size=1,
    spawn_opponents=True,
    tick_skip=8,
    game_speed=100,
)

obs = env.reset()
while True:
    actions = my_policy(obs)          # your agent
    obs, reward, done, info = env.step(actions)
    if done:
        obs = env.reset()
```

## Configuration

Settings live in the plugin's page in the BakkesMod settings window (F2 → Plugins → BakkesRLGymPlugin) and are backed by cvars so they persist across restarts:

| Setting | Cvar | Default | Notes |
|---|---|---|---|
| Car Body ID | `brlgym_car_body` | `23` (Octane) | e.g. `4284` = Fennec |
| Bot Name | `brlgym_bot_name` | `Agent` | Name prefix for agents |
| Map | `brlgym_map` | `EuroStadium_Night_P` | Any valid map name |
| Extra mutators | *(settings window)* | — | Comma-separated GameTags, appended to the built-in `BotsNone,UnlimitedTime,DisableGoalDelay,PlayerCount8` |

> **Note:** These apply to the match created for the **first** connection. After changing them, reconnect (restart your training script) to rebuild the match. A mutator/GameTag reference is on the [BakkesMod wiki](https://bakkesmod.fandom.com/wiki/Unreal_command).

## Building

**Requirements**

- Visual Studio 2022 (or the VS Build Tools) with the C++ / MSVC toolset
- Windows 10/11 SDK
- The vendored [BakkesModSDK](BakkesModSDK/) (already included in the repo)

**Build**

```powershell
git clone https://github.com/chrisrca/BakkesRLGymPlugin.git --recurse-submodules
cd BakkesRLGymPlugin
msbuild BakkesRLGymPlugin\BakkesRLGymPlugin.vcxproj /p:Configuration=Release /p:Platform=x64 /m
```

If a local BakkesMod plugins folder exists, the project's post-build step attempts to copy the DLL there automatically.

---

*This is an unofficial project and is not affiliated with Psyonix, Epic Games, or the RLGym team.*
