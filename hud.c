/*!
@file       : hud.c
@author     : Lek Kar Lim (lekkar.lim@digipen.edu) - hearts and health display
@co-author  : Isaac Tan (isaacd.tan@digipen.edu) - score, coin, distance, multiplier panels
			  Ggwendolynn Lee Rasni (ggwendolynnlee.r@digipen.edu) - HUD styling, power-up timers, panel layout
@team       : IsaacACat
@module     : CSD1401 Software Engineering Project 1
@section    : B
@project    : Alley Runner
@brief      : Implementation of the in-game HUD.

All content © 2026 DigiPen Institute of Technology Singapore. All Rights Reserved.
*//*________________________________________________________________________________*/
#include "hud.h"
#include "ui.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

/* All positions are on the 1280 x 720 design canvas, multiplied by UI_GetScale(). */
#define HUD_ROW_Y         48.0f   /* centre line of every top panel */
#define HUD_PANEL_H       60.0f
#define HUD_SCORE_X       84.0f
#define HUD_COINS_X       222.0f
#define HUD_HEARTS_X      382.0f
#define HUD_TIMERS_LEFT   484.0f  /* power-up timers fill the gap between hearts and multiplier */
#define HUD_TIMERS_RIGHT  326.0f  /* measured from the right edge */
#define HUD_MULT_RIGHT    272.0f  /* measured from the right edge */
#define HUD_DIST_RIGHT    153.0f  /* measured from the right edge */
#define HUD_HEART_SIZE    44.0f
#define HUD_HEART_SPACING 52.0f
#define HUD_LOST_HEART_ALPHA 60   /* empty hearts are drawn faded */

static CP_Image heartImage = NULL;
static CP_Image coinImage = NULL;

void Hud_Load(void)
{
    heartImage = CP_Image_Load("Assets/hearts.png");
    coinImage = CP_Image_Load("Assets/Coin_solo.png");
}

void Hud_Free(void)
{
    if (heartImage != NULL)
    {
        CP_Image_Free(heartImage);
        heartImage = NULL;
    }

    if (coinImage != NULL)
    {
        CP_Image_Free(coinImage);
        coinImage = NULL;
    }
}

CP_Color Hud_PowerUpColor(PowerType type, int alpha)
{
    switch (type)
    {
    case PU_INVINCIBLE: return CP_Color_Create(255, 200, 0, alpha);
    case PU_SHIELD:     return CP_Color_Create(80, 180, 255, alpha);
    default:            return CP_Color_Create(170, 90, 255, alpha);
    }
}

/* Draws centred text, shrinking it if it would not fit in availableWidth.
   Press Start 2P is monospaced, so width = letters x size. */
static void DrawFittedText(const char* text, float x, float y, float preferredSize,
                           float availableWidth, CP_Color color)
{
    size_t length = strlen(text);
    float size = preferredSize;

    if (length > 0)
    {
        size = fminf(size, availableWidth / (float)length);
    }

    CP_Settings_TextAlignment(CP_TEXT_ALIGN_H_CENTER, CP_TEXT_ALIGN_V_MIDDLE);
    CP_Settings_TextSize(size);
    CP_Settings_Fill(color);
    CP_Font_DrawText(text, x, y);
}

static void DrawScorePanel(const HudData* data, float s, float y, float panelH)
{
    char text[32];
    float x = HUD_SCORE_X * s;

    UI_DrawHudPanel(x, y, 136.0f * s, panelH, false);
    DrawFittedText("SCORE", x, y - 15.0f * s, 13.0f * s, 112.0f * s, CP_Color_Create(245, 245, 245, 255));

    snprintf(text, sizeof(text), "%05d", data->score);
    DrawFittedText(text, x, y + 12.0f * s, 23.0f * s, 112.0f * s, CP_Color_Create(255, 220, 65, 255));
}

static void DrawCoinPanel(const HudData* data, float s, float y, float panelH)
{
    char text[32];
    float x = HUD_COINS_X * s;

    UI_DrawHudPanel(x, y, 116.0f * s, panelH, false);

    if (coinImage != NULL)
    {
        CP_Image_Draw(coinImage, x - 35.0f * s, y, 28.0f * s, 38.0f * s, 255);
    }
    else
    {
        CP_Settings_Fill(CP_Color_Create(255, 220, 65, 255));
        CP_Graphics_DrawCircle(x - 35.0f * s, y, 25.0f * s);
    }

    snprintf(text, sizeof(text), "x%02d", data->coins);
    DrawFittedText(text, x + 15.0f * s, y, 23.0f * s, 70.0f * s, CP_Color_Create(245, 245, 245, 255));
}

static void DrawHearts(const HudData* data, float s, float y, float panelH)
{
    float x = HUD_HEARTS_X * s;
    float firstX = x - ((float)(MAX_LIVES - 1) * HUD_HEART_SPACING * s) * 0.5f;

    UI_DrawHudPanel(x, y, 180.0f * s, panelH, false);

    if (heartImage == NULL)
    {
        return;
    }

    for (int i = 0; i < MAX_LIVES; i++)
    {
        CP_Image_Draw(heartImage, firstX + (float)i * HUD_HEART_SPACING * s, y,
            HUD_HEART_SIZE * s, HUD_HEART_SIZE * s,
            i < data->lives ? 255 : HUD_LOST_HEART_ALPHA);
    }
}

static void DrawRightPanels(const HudData* data, float s, float y, float panelH)
{
    char text[32];
    float windowW = (float)CP_System_GetWindowWidth();
    float distanceX = windowW - HUD_DIST_RIGHT * s;
    float multiplierX = windowW - HUD_MULT_RIGHT * s;

    UI_DrawHudPanel(distanceX, y, 130.0f * s, panelH, false);
    snprintf(text, sizeof(text), "%dm", data->distance);
    DrawFittedText(text, distanceX, y, 23.0f * s, 108.0f * s, CP_Color_Create(245, 245, 245, 255));

    UI_DrawHudPanel(multiplierX, y, 84.0f * s, panelH, false);
    snprintf(text, sizeof(text), "x%d", data->multiplier);
    DrawFittedText(text, multiplierX, y, 25.0f * s, 64.0f * s, CP_Color_Create(255, 220, 65, 255));
}

/* One panel per active power-up showing its name and seconds left.
   Names are shortened when more than one is active so they fit. */
static void DrawPowerUpTimers(const HudData* data, float s, float y, float panelH)
{
    static const char* longNames[NUM_POWERUPS] = { "INVINCIBLE", "SHIELD", "MAGNET" };
    static const char* shortNames[NUM_POWERUPS] = { "INV", "SHD", "MAG" };

    float timerLeft = HUD_TIMERS_LEFT * s;
    float timerRight = (float)CP_System_GetWindowWidth() - HUD_TIMERS_RIGHT * s;
    float areaWidth = timerRight - timerLeft;
    float panelGap = 8.0f * s;
    float panelWidth;
    int activeCount = 0;
    int slot = 0;

    for (int t = 0; t < NUM_POWERUPS; t++)
    {
        if (data->powerUpTime[t] > 0.0f)
        {
            activeCount++;
        }
    }

    if (activeCount == 0 || areaWidth <= 0.0f)
    {
        return;
    }

    panelWidth = (areaWidth - panelGap * (float)(activeCount - 1)) / (float)activeCount;

    for (int t = 0; t < NUM_POWERUPS; t++)
    {
        const char* name;
        char seconds[16];
        float centerX;
        float available;
        float textSize;
        float nameWidth;
        float secondsWidth;
        float gap;
        float groupLeft;

        if (data->powerUpTime[t] <= 0.0f)
        {
            continue;
        }

        name = (activeCount == 1) ? longNames[t] : shortNames[t];
        snprintf(seconds, sizeof(seconds), "%.1fs", (double)data->powerUpTime[t]);

        centerX = timerLeft + panelWidth * 0.5f + (float)slot * (panelWidth + panelGap);
        slot++;

        UI_DrawHudPanel(centerX, y, panelWidth, panelH, false);

        /* Shrink the text if name + gap + countdown is wider than the panel. */
        available = fmaxf(1.0f, panelWidth - 28.0f * s);
        textSize = fminf(23.0f * s, available / ((float)strlen(name) + (float)strlen(seconds) + 1.5f));
        nameWidth = (float)strlen(name) * textSize;
        secondsWidth = (float)strlen(seconds) * textSize;
        gap = 1.5f * textSize;
        groupLeft = centerX - (nameWidth + gap + secondsWidth) * 0.5f;

        CP_Settings_TextSize(textSize);
        CP_Settings_TextAlignment(CP_TEXT_ALIGN_H_LEFT, CP_TEXT_ALIGN_V_MIDDLE);
        CP_Settings_Fill(Hud_PowerUpColor((PowerType)t, 255));
        CP_Font_DrawText(name, groupLeft, y);

        CP_Settings_Fill(CP_Color_Create(245, 245, 245, 255));
        CP_Font_DrawText(seconds, groupLeft + nameWidth + gap, y);
    }
}

void Hud_Draw(const HudData* data)
{
    float s = UI_GetScale();
    float y = HUD_ROW_Y * s;
    float panelH = HUD_PANEL_H * s;
    CP_Font font = UI_GetTitleFont();

    if (font != NULL)
    {
        CP_Font_Set(font);
    }

    CP_Settings_ImageMode(CP_POSITION_CENTER);
    CP_Settings_NoStroke();

    DrawScorePanel(data, s, y, panelH);
    DrawCoinPanel(data, s, y, panelH);
    DrawHearts(data, s, y, panelH);
    DrawPowerUpTimers(data, s, y, panelH);
    DrawRightPanels(data, s, y, panelH);

    CP_Settings_NoStroke();
    CP_Settings_RectMode(CP_POSITION_CORNER);
}
