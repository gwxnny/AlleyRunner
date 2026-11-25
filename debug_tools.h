/*!
@file       : debug_tools.h
@author     : Isaac Tan (isaacd.tan@digipen.edu)
              (module written with AI assistance and reviewed by the team)
@team       : IsaacACat
@module     : CSD1401 Software Engineering Project 1
@section    : B
@project    : Alley Runner
@brief      : In-game debugging tools. All are OFF by default and only react
              during gameplay:
                F1  overlay: live values, hitboxes, section profiler, frame graph
                F2  god mode (obstacles cannot hurt the pin)
                F3  freeze / unfreeze the world
                F4  advance exactly one frame while frozen
                F5  give the next power-up (invincible, shield, magnet in turn)
                F6  add one heart

All content © 2026 DigiPen Institute of Technology Singapore. All Rights Reserved.
*//*________________________________________________________________________________*/
#ifndef DEBUG_TOOLS_H
#define DEBUG_TOOLS_H

#include "cprocessing.h"
#include <stdbool.h>

/* Code sections timed by the profiler. */
typedef enum
{
    DEBUG_SECTION_UPDATE,       /* gameplay logic */
    DEBUG_SECTION_DRAW_WORLD,   /* background, obstacles, characters */
    DEBUG_SECTION_DRAW_UI,      /* HUD and menus */
    DEBUG_SECTION_COUNT
} DebugSection;

/* One-shot requests the game should carry out this frame. */
typedef enum
{
    DEBUG_COMMAND_NONE,
    DEBUG_COMMAND_GIVE_POWERUP,
    DEBUG_COMMAND_ADD_LIFE
} DebugCommand;

#define DEBUG_FIXED_STEP (1.0f / 60.0f)  /* dt used when stepping one frame */

/* Resets every toggle (called when a run starts). */
void Debug_Reset(void);

/* Reads the debug keys. Call once at the start of each gameplay frame. */
void Debug_Update(void);

bool Debug_IsOverlayOn(void);
bool Debug_IsGodMode(void);
bool Debug_IsFrozen(void);

/* True on the single frame after F4 is pressed while frozen. */
bool Debug_IsSteppingFrame(void);

/* Returns this frame's request (and clears it). */
DebugCommand Debug_TakeCommand(void);

/* Profiler: wrap a section of code with Begin/End. */
void Debug_ProfileBegin(DebugSection section);
void Debug_ProfileEnd(DebugSection section);

/* Hitbox helpers (centre-anchored). Only draw when the overlay is on. */
void Debug_DrawBox(float centerX, float centerY, float w, float h, CP_Color color);
void Debug_DrawCircle(float centerX, float centerY, float radius, CP_Color color);

/* Draws the info panel, profiler bars and frame-time graph.
   lines: text rows supplied by the game (live entity values). */
void Debug_DrawOverlay(const char* const* lines, int lineCount);

#endif /* DEBUG_TOOLS_H */
