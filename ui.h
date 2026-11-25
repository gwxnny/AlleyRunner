/*!
@file       : ui.h
@author     : Ggwendolynn Lee Rasni (ggwendolynnlee.r@digipen.edu) - button and text style, HUD panel styling
@co-author  : Lek Kar Lim (lekkar.lim@digipen.edu) - HUD panel (hearts)
              Isaac Tan (isaacd.tan@digipen.edu) - shared module, press/release feedback, HUD panel (power-ups, coins, score etc.)
@team       : IsaacACat
@module     : CSD1401 Software Engineering Project 1
@section    : B
@project    : Alley Runner
@brief      : Shared UI system used by every screen: fonts, mouse mapping,
              pill buttons with hover / hold / release feedback, outlined text,
              HUD panels and the button click sound.

All content © 2026 DigiPen Institute of Technology Singapore. All Rights Reserved.
*//*________________________________________________________________________________*/
#ifndef UI_H
#define UI_H

#include "cprocessing.h"
#include <stdbool.h>

// every layout in the game is designed on this canvas and scaled to the window.
#define UI_DESIGN_WIDTH  1280.0f
#define UI_DESIGN_HEIGHT 720.0f

// loads the shared fonts and click sound. safe to call more than once.
void UI_Load(void);

// frees the shared fonts and click sound. called once when the program closes.
void UI_Unload(void);

// Press Start 2P: titles, buttons and HUD numbers.
CP_Font UI_GetTitleFont(void);

// VT323: body text, descriptions and the rage overlay.
CP_Font UI_GetBodyFont(void);

// min(window / design canvas): one scale factor shared by all screens.
float UI_GetScale(void);

// mouse position mapped from the native client area to drawing coordinates. needed for all mouse input.
void UI_GetMouse(float* x, float* y);

// true if the mouse is inside the centre-anchored rectangle.
bool UI_IsMouseOver(float centerX, float centerY, float width, float height);

// draws a pill button and handles its input.
// hover: lighter colour. hold: darker, pressed-in look.
// returns true on the frame the left button is released over the button
// (only if the press also started on it) and plays the click sound.
bool UI_PillButton(float x, float y, float w, float h, const char* label);

// same look as UI_PillButton but never reacts to input (used while a
// screen is busy, e.g. during a pending action or transition).
void UI_DrawPillButton(float x, float y, float w, float h, const char* label, bool hovered, bool pressed);

// draws text with a solid outline by drawing offset copies behind it.
void UI_DrawTextWithOutline(const char* text, float x, float y, float size,
                               CP_Color fillColor, CP_Color outlineColor, float outlineWidth);

// dark eight-sided HUD panel with a light border (score, coins, pause button).
void UI_DrawHudPanel(float x, float y, float w, float h, bool hovered);

// plays the shared button click sound on the SFX group.
void UI_PlayClick(void);

// records where the current left click started. called every frame by UI_PillButton.
void UI_TrackPress(void);

#endif /* UI_H */
