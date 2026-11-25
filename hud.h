/*!
@file       : hud.h
@author     : Lek Kar Lim (lekkar.lim@digipen.edu) - hearts and health display
@co-author  : Isaac Tan (isaacd.tan@digipen.edu) - score, coin, distance, multiplier panels
			  Ggwendolynn Lee Rasni (ggwendolynnlee.r@digipen.edu) - HUD styling, power-up timers, panel layout
@team       : IsaacACat
@module     : CSD1401 Software Engineering Project 1
@section    : B
@project    : Alley Runner
@brief      : In-game heads-up display: score, coins, hearts, active power-up
              timers, multiplier and distance along the top of the screen.

All content © 2026 DigiPen Institute of Technology Singapore. All Rights Reserved.
*//*________________________________________________________________________________*/
#ifndef HUD_H
#define HUD_H

#include "cprocessing.h"
#include "game_types.h"

/* Everything the HUD shows. Filled in by the game each frame. */
typedef struct
{
    int score;
    int coins;
    int distance;                       /* metres */
    int multiplier;
    int lives;
    float powerUpTime[NUM_POWERUPS];    /* seconds left, 0 = not active */
} HudData;

void Hud_Load(void);
void Hud_Free(void);
void Hud_Draw(const HudData* data);

/* Colour that represents each power-up (HUD timers, roulette, fallbacks). */
CP_Color Hud_PowerUpColor(PowerType type, int alpha);

#endif /* HUD_H */
