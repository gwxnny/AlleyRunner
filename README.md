# Alley Runner [By IsaacACat]


https://github.com/user-attachments/assets/5002a311-9333-4d35-bb30-2b9ee957dfbc


A three-lane endless runner where you play a bowling pin running for its life from a giant bowling ball. The alley keeps speeding up, the hazards keep coming, and every reward sits in a lane that makes you take a risk to grab it.

Made in C with CProcessing for CSD1401 Software Engineering Project 1 (Section B), DigiPen Institute of Technology Singapore.

## Concept (SCQA)

- **Situation:** Endless runners are easy to pick up and suit quick gaming breaks.
- **Complication:** Static-speed runners become monotonous once players learn the obstacle patterns.
- **Question:** How do we make a runner that stays tense and rewards risky play without overwhelming the controls?
- **Answer:** A reverse-chase runner: a bowling pin flees a giant bowling ball on three lanes. The pace never stops rising, hazards cost hearts, and the best rewards force risky lane switches. Only two keys to move.

## How to Play

| Key | Action |
| --- | --- |
| **W / S** | Move up / down one lane |
| **ESC / SPACE / P** | Pause and resume (or click the pause button, top right) |
| **R / Q** (pause menu) | Restart / return to Main Menu (both ask for confirmation) |
| **ENTER / ESC** | Yes / No on any confirmation |
| **R / Q** (results screen) | Restart / Main Menu |

All menus also work with the mouse. On the splash screens, one click, SPACE, ENTER or ESC skips straight to the menu.

## Rules

| System | Behaviour |
| --- | --- |
| Speed | The world scrolls faster every second for the whole run. |
| Score | Grows with distance travelled. Your **final score = score × multiplier**. |
| Hearts | Start with 3, maximum 3. An unprotected hit costs 1 heart, with a brief flicker and slowdown. |
| Barriers | Cost 1 heart and break when hit. |
| Oil spills | Cost 1 heart. Losing your last heart on oil plays the slip animation. |
| Holes | Cost 1 heart and **ignore Shield and Invincibility**. Losing your last heart in a hole plays the fall animation. |
| The ball | Chases from the left edge, following your lane. Lose your last heart on a barrier and it catches you. |
| Coins | Collected and counted on the HUD and results screen. The Magnet pulls them in. |
| Multiplier balls | Each one raises the multiplier by 1, up to **×5**. |
| Heart pickups | Only appear when you are missing a heart. Restore 1 heart (never above 3). |
| Mystery boxes | Every box looks the same. Collect one and a roulette at the top of the screen picks a power-up. |
| Invincibility | 5s. Smash through barriers and oil. |
| Shield | Up to 10s. Blocks one barrier or oil hit, then breaks. |
| Magnet | 16s. Pulls nearby coins towards you. |
| Ragebait words | At random moments, rows of taunts sweep across the screen to distract you. They never cause damage. |
| Game over | The results screen shows score, multiplier, final score, coins, what ended the run, and a **NEW!** tag for a high score. |

Durations and spawn rates above are the defaults and can be changed in the tuning file (see below).

## Features

- Splash screens (DigiPen Singapore, then the team logo), main menu with an animated chase, How To Play, Settings, Credits and Quit.
- Settings: Music and SFX volume sliders; Windowed / Borderless / Fullscreen. The game always starts windowed at 1600×900.
- Pause menu with Resume, Restart and Main Menu; every destructive action asks "are you sure?".
- Every screen change fades through black. Buttons react on hover, look pressed while held, and activate on release with a click sound.
- High score shown on the main menu (kept for the current session).

## Debug Tools

All are **off by default** and only work during a run.

| Key | Tool |
| --- | --- |
| **F1** | Debug overlay: FPS, live values for every entity, a section profiler (update / draw world / draw UI in ms), a frame-time graph, and hitboxes drawn with the same numbers the collision code uses |
| **F2** | God mode (obstacles cannot hurt you) |
| **F3 / F4** | Freeze the world / step exactly one frame while frozen |
| **F5** | Give a power-up (Invincible → Shield → Magnet in turn) |
| **F6** | Add a heart |

## Gameplay Tuning (Content Tool)

Every balance value — start speed, speed gain, obstacle spacing, all spawn intervals, power-up durations, magnet range and ragebait timing — lives in [`Assets/Data/gameplay_tuning.txt`](./Assets/Data/gameplay_tuning.txt).

- **Hot reload:** edit and save the file (the copy next to the `.exe`) while the game is running. It reloads within half a second.
- **In-game editor:** press **F7** during a run. **Up/Down** pick a value, **Left/Right** change it (hold **Shift** for ×10), **F8** saves to the file, **F9** restores the defaults.

Values are range-checked, so a typo in the file can never break the game.

## Building and Running

1. Open `AlleyRunner.slnx` in Visual Studio 2026.
2. Choose **Debug** or **Release** and **x64**, then **Build → Rebuild Solution**.
3. Run with **F5**. A post-build step copies the CProcessing DLLs and the `Assets` folder next to the `.exe`, so it runs straight from Visual Studio or from `x64/Release`.

## Source Layout

| Filter | Files | Purpose |
| --- | --- | --- |
| Core | `main.c`, `transition.c` | Entry point (1600×900 window), fade-to-black state changes |
| States | `splash.c`, `mainmenu.c`, `game.c` | Splash screens, main menu and pop-ups, the run itself |
| Gameplay | `animation.c` | Sprite-sheet animation |
| | `rage_overlay.c` | Ragebait distraction words |
| | `game_types.h` | Shared power-up types |
| Gameplay / UI | `hud.c` | Score, coins, hearts, power-up timers, multiplier, distance |
| | `game_menus.c` | Pause button, pause menu, confirmation, results screen |
| UI | `ui.c` | Shared fonts, buttons, outlined text, HUD panels, click sound |
| Tools | `debug_tools.c` | Debug overlay, profiler, god mode, frame stepping |
| | `tuning.c` | Tuning file, hot reload and in-game editor |

## Team and Contributions

| Member | Degree | Contributions |
| --- | --- | --- |
| **Ggwendolynn Lee Rasni (Gwen)** | IMGD | Main menu, settings and pop-ups; obstacles (barrier, oil, hole); barrier collision; ragebait overlay; display modes; DigiPen splash screen |
| **Isaac Tan** | IMGD | Lane movement; speed curve; scoring and multiplier; coins; power-up system (Invincibility, Shield, Magnet, mystery box); hole death; death flow and ball catch; splash screens, credits and confirmations; debug and tuning tools |
| **Ang Xiao Hui** | IMGD | Character animations (pin, ball, deaths); coin, power-up and multiplier art; mystery box |
| **Lek Kar Lim (Lekkar)** | RTIS | Hearts and health display; heart pickups (Extra Life); hit reaction; pause menu; results screen |

### Task Tracker

#### Gwen

- [x] **UI / Menus — Main Menu:** Play, How to Play, Settings, Credits and Quit, with menu music, click sounds and Music/SFX sliders.
- [x] **Collision + Health — Barrier Collision:** 1-heart damage on an ordinary barrier hit; respects active Shield and Invincibility.
- [x] **Obstacles — Barrier:** Asset, spawning, rendering, and a hitbox aligned with its visible shape.
- [x] **Obstacles — Oil Spill:** Asset, spawn animation and contact detection.
- [x] **Obstacles — Hole in Ground:** Asset, spawning, rendering, and the detection hook for the hole death.
- [x] **Distractions — Ragebait Words:** Large moving text in unpredictable, brief bursts with quiet periods; independent of obstacles and hidden on menus.

#### Isaac

- [x] **Player Movement — Lane Switching:** Switch between 3 lanes using W/S without leaving the lane bounds.
- [x] **Difficulty — Increasing Speed:** Speed increases over time, with obstacle spacing growing with speed so reaction time stays fair.
- [x] **Death System — Hole Collision:** Holes ignore Invincibility and Shield.
- [x] **Death System — Falling Into Hole Animation:** Fall animation plays fully before the results screen.
- [x] **Collectibles — Coins:** Spawn coins, detect collection, remove collected coins, and update the run's coin count.
- [x] **Scoring — Score Multiplier:** Raise the multiplier when a multiplier ball is collected and apply it to the final score.
- [x] **Scoring — Run Score:** Score grows while running and stops on death.
- [x] **Power-Ups — Shield:** Pickup and state; blocks 1 barrier or oil hit and expires after 10 seconds.
- [x] **Power-Ups — Invincibility:** Pickup and 5-second effect; ignores barrier and oil damage but not holes.
- [x] **Power-Ups — Coin Magnet:** Attracts nearby coins while active.
- [x] **Core Systems — Ball Chase:** The ball follows the pin's lane and catches it when the last heart is lost.

#### Xiao Hui

- [x] **Animation — Bowling Ball Running/Rolling:** The chasing ball's rolling animation.
- [x] **Animation — Pin Running:** The player pin's running animation.
- [x] **Collectibles / Animation — Floating Coin:** The coin's animation.
- [x] **Collectibles — Score Multiplier:** Multiplier ball art, integrated with Isaac's multiplier logic.
- [x] **Power-Up Animation — Shield:** Shield effect while active; removed on use or expiry.
- [x] **Power-Up Animation — Invincibility:** Effect for the active 5-second period, removed on expiry.

#### Lekkar

- [x] **Collision + Health — Heart System:** Start with 3 hearts, cap health at 3, damage and healing.
- [x] **Death System — Death at 0 Hearts:** Death triggers exactly once when health reaches 0.
- [x] **UI / Menus — Death Screen:** Final score and coins; Restart and return to the main menu.
- [x] **UI / Menus — Pause Menu:** Pauses gameplay; Resume and return-to-menu actions.
- [x] **UI / Menus — Health Display:** 3 heart indicators that update when health changes.
- [x] **Power-Ups — Extra Life:** Heart pickups restore 1 heart only when below 3, connected to the health display.

#### Shared — Xiao Hui & Isaac

- [x] **Animation / Death System — Pin Death:** Xiao Hui made the death animations; Isaac integrated their triggers and playback into the death flow.

## Playtest Checklist

- [x] Clean build in Debug and Release with no unresolved symbols.
- [x] Images, fonts and sounds load from the executable's folder.
- [x] All three lanes are reachable, with no out-of-bounds lane selection.
- [x] Barrier damage, health display and death triggers agree.
- [x] Shield blocks exactly 1 hit and expires correctly.
- [x] Invincibility expires after 5 seconds and never protects against holes.
- [x] Heart pickups never raise health above 3.
- [x] Multiplier and coin totals reset correctly on restart.
- [x] Pause freezes gameplay timers and movement, and pauses the music.
- [x] Ragebait events have quiet gaps, cause no damage, and do not cover menus.
- [x] Returning to the menu and restarting do not duplicate audio playback.

## Credits

- Fonts: [Press Start 2P](https://fonts.google.com/specimen/Press+Start+2P) (CodeMan38) and [VT323](https://fonts.google.com/specimen/VT323) (Peter Hull), SIL Open Font License.
- Built with CProcessing, DigiPen Institute of Technology.

All content © 2026 DigiPen Institute of Technology Singapore. All Rights Reserved.
