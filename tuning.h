/*!
@file       : tuning.h
@author     : Isaac Tan (isaacd.tan@digipen.edu)
@team       : IsaacACat
@module     : CSD1401 Software Engineering Project 1
@section    : B
@project    : Alley Runner
@brief      : Content creation tool for gameplay data.

              All gameplay balance values live in Assets/Data/gameplay_tuning.txt.
              - Hot reload: edit and save the file in any text editor while the
                game runs; the change is picked up within half a second.
              - In-game editor: press F7 during a run to open a live editor panel
                (Up/Down pick a value, Left/Right change it, hold Shift for x10,
                F8 saves back to the file, F9 restores the defaults).

All content © 2026 DigiPen Institute of Technology Singapore. All Rights Reserved.
*//*________________________________________________________________________________*/
#ifndef TUNING_H
#define TUNING_H

#include <stdbool.h>

#define TUNING_FILE_PATH "Assets/Data/gameplay_tuning.txt"

/* Every value a designer can change without recompiling. */
typedef struct
{
    /* Speed and spacing */
    float startSpeed;          /* world speed when a run starts (applies on restart) */
    float speedRamp;           /* speed gained per second */
    float obstacleSpacing;     /* gap between obstacles at start speed */

    /* Spawning (seconds between spawns) */
    float coinSpawnMin;
    float coinSpawnMax;
    float powerUpSpawnMin;
    float powerUpSpawnMax;
    float heartSpawnMin;
    float heartSpawnMax;
    float multiplierSpawnMin;
    float multiplierSpawnMax;

    /* Power-ups */
    float invincibleTime;
    float shieldTime;
    float magnetTime;
    float magnetRange;

    /* Rage overlay */
    float rageQuietMin;
    float rageQuietMax;
    float rageDuration;
} GameTuning;

/* Sets the defaults, then loads the tuning file if it exists. */
void Tuning_Init(void);

/* Read-only access for gameplay code. */
const GameTuning* Tuning_Get(void);

/* Polls the file for changes and handles the editor keys. Call once per frame. */
void Tuning_Update(float dt);

/* Draws the editor panel (if open) and the "reloaded / saved" message. */
void Tuning_Draw(void);

bool Tuning_IsEditorOpen(void);

#endif /* TUNING_H */