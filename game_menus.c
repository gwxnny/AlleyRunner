/*!
@file       : game_menus.c
@author     : Lek Kar Lim (lekkar.lim@digipen.edu) - pause menu, confirmation, death screen
@co-author  : Isaac Tan (isaacd.tan@digipen.edu) - restart option, keyboard controls, own module
@team       : IsaacACat
@module     : CSD1401 Software Engineering Project 1
@section    : B
@project    : Alley Runner
@brief      : Implementation of the pause menu, confirmation dialog and
              results screen.

All content © 2026 DigiPen Institute of Technology Singapore. All Rights Reserved.
*//*________________________________________________________________________________*/
#include "game_menus.h"
#include "ui.h"
#include "cprocessing.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

/* Layout helpers: everything is relative to the window and UI_GetScale(). */
#define WIN_W ((float)CP_System_GetWindowWidth())
#define WIN_H ((float)CP_System_GetWindowHeight())
#define CENTER_X (WIN_W * 0.5f)

#define MENU_BTN_H        (80.0f * UI_GetScale())
#define PAUSE_BTN_W       (460.0f * UI_GetScale())
#define PAUSE_ICON_SIZE   (60.0f * UI_GetScale())
#define PAUSE_ICON_X      (WIN_W - 46.0f * UI_GetScale())
#define PAUSE_ICON_Y      (48.0f * UI_GetScale())
#define CONFIRM_BTN_W     (300.0f * UI_GetScale())
#define CONFIRM_GAP       (40.0f * UI_GetScale())
#define RESULT_BTN_W      (360.0f * UI_GetScale())
#define RESULT_BTN_Y      (WIN_H * 0.76f)
#define RESULT_BTN_OFFSET (200.0f * UI_GetScale())

/* Which destructive action is waiting for "YES". */
static MenuAction pendingConfirm = MENU_ACTION_NONE;

void GameMenus_Reset(void)
{
    pendingConfirm = MENU_ACTION_NONE;
}

/* Dims the whole scene, then draws a rounded, bordered panel in the centre. */
static void DrawMenuPanel(float panelW, float panelH)
{
    CP_Settings_NoStroke();
    CP_Settings_RectMode(CP_POSITION_CORNER);
    CP_Settings_Fill(CP_Color_Create(0, 0, 0, 160));
    CP_Graphics_DrawRect(0.0f, 0.0f, WIN_W, WIN_H);

    CP_Settings_RectMode(CP_POSITION_CENTER);
    CP_Settings_Stroke(CP_Color_Create(255, 230, 230, 255));
    CP_Settings_StrokeWeight(4.0f * UI_GetScale());
    CP_Settings_Fill(CP_Color_Create(25, 15, 20, 240));
    CP_Graphics_DrawRectAdvanced(CENTER_X, WIN_H * 0.5f, panelW, panelH, 0.0f, 24.0f * UI_GetScale());
    CP_Settings_NoStroke();
}

/* Large title with a drop shadow. */
static void DrawMenuTitle(const char* text, float y)
{
    float offset = 4.0f * UI_GetScale();

    if (UI_GetTitleFont() != NULL)
    {
        CP_Font_Set(UI_GetTitleFont());
    }

    CP_Settings_TextAlignment(CP_TEXT_ALIGN_H_CENTER, CP_TEXT_ALIGN_V_MIDDLE);
    CP_Settings_TextSize(WIN_H * 0.07f);
    CP_Settings_Fill(CP_Color_Create(0, 0, 0, 255));
    CP_Font_DrawText(text, CENTER_X + offset, WIN_H * y + offset);
    CP_Settings_Fill(CP_Color_Create(255, 230, 230, 255));
    CP_Font_DrawText(text, CENTER_X, WIN_H * y);
}

/* Centred line of body text. */
static void DrawBodyText(const char* text, float y, float size, CP_Color color)
{
    if (UI_GetBodyFont() != NULL)
    {
        CP_Font_Set(UI_GetBodyFont());
    }

    CP_Settings_TextAlignment(CP_TEXT_ALIGN_H_CENTER, CP_TEXT_ALIGN_V_MIDDLE);
    CP_Settings_TextSize(size);
    CP_Settings_Fill(color);
    CP_Font_DrawText(text, CENTER_X, WIN_H * y);
}

bool GameMenus_PauseButton(void)
{
    float s = UI_GetScale();
    bool hovered;
    bool clicked;

    UI_TrackPress();
    hovered = UI_IsMouseOver(PAUSE_ICON_X, PAUSE_ICON_Y, PAUSE_ICON_SIZE, PAUSE_ICON_SIZE);
    clicked = hovered && CP_Input_MouseReleased(MOUSE_BUTTON_LEFT);

    UI_DrawHudPanel(PAUSE_ICON_X, PAUSE_ICON_Y, PAUSE_ICON_SIZE, PAUSE_ICON_SIZE, hovered);

    /* Two vertical bars; yellow on hover, pushed down while held. */
    {
        float drop = (hovered && CP_Input_MouseDown(MOUSE_BUTTON_LEFT)) ? 2.0f * s : 0.0f;

        CP_Settings_RectMode(CP_POSITION_CENTER);
        CP_Settings_NoStroke();
        CP_Settings_Fill(hovered ? CP_Color_Create(255, 225, 100, 255) : CP_Color_Create(235, 235, 240, 255));
        CP_Graphics_DrawRect(PAUSE_ICON_X - 9.0f * s, PAUSE_ICON_Y + drop, 7.0f * s, 30.0f * s);
        CP_Graphics_DrawRect(PAUSE_ICON_X + 9.0f * s, PAUSE_ICON_Y + drop, 7.0f * s, 30.0f * s);
        CP_Settings_RectMode(CP_POSITION_CORNER);
    }

    if (clicked)
    {
        UI_PlayClick();
    }

    return clicked;
}

/* "Are you sure?" panel for restart / main menu. Returns the confirmed action. */
static MenuAction ConfirmDialog(bool acceptKeys)
{
    float s = UI_GetScale();
    float btnY = WIN_H * 0.68f;
    float yesX = CENTER_X - (CONFIRM_BTN_W + CONFIRM_GAP) * 0.5f;
    float noX = CENTER_X + (CONFIRM_BTN_W + CONFIRM_GAP) * 0.5f;
    bool restart = (pendingConfirm == MENU_ACTION_RESTART);
    bool yes;
    bool no;

    DrawMenuPanel(2.0f * CONFIRM_BTN_W + CONFIRM_GAP + 120.0f * s, WIN_H * 0.62f);
    DrawMenuTitle(restart ? "RESTART?" : "MAIN MENU?", 0.29f);

    DrawBodyText(restart ? "Restart this run from the beginning?" : "Leave this run and return to the main menu?",
        0.44f, WIN_H * 0.05f, CP_Color_Create(255, 230, 230, 255));
    DrawBodyText("Your current score will be lost.",
        0.52f, WIN_H * 0.04f, CP_Color_Create(255, 200, 90, 255));

    yes = UI_PillButton(yesX, btnY, CONFIRM_BTN_W, MENU_BTN_H, "YES (ENTER)");
    no = UI_PillButton(noX, btnY, CONFIRM_BTN_W, MENU_BTN_H, "NO (ESC)");

    if (acceptKeys)
    {
        if (CP_Input_KeyTriggered(KEY_ENTER) || CP_Input_KeyTriggered(KEY_KP_ENTER) || CP_Input_KeyTriggered(KEY_Y))
        {
            UI_PlayClick();
            yes = true;
        }
        else if (CP_Input_KeyTriggered(KEY_ESCAPE) || CP_Input_KeyTriggered(KEY_N))
        {
            UI_PlayClick();
            no = true;
        }
    }

    if (yes)
    {
        MenuAction confirmed = pendingConfirm;
        pendingConfirm = MENU_ACTION_NONE;
        return confirmed;
    }

    if (no)
    {
        pendingConfirm = MENU_ACTION_NONE;
    }

    return MENU_ACTION_NONE;
}

MenuAction GameMenus_PauseMenu(bool acceptKeys)
{
    if (pendingConfirm != MENU_ACTION_NONE)
    {
        return ConfirmDialog(acceptKeys);
    }

    DrawMenuPanel(PAUSE_BTN_W + 160.0f * UI_GetScale(), WIN_H * 0.62f);
    DrawMenuTitle("PAUSED", 0.27f);

    if (UI_PillButton(CENTER_X, WIN_H * 0.42f, PAUSE_BTN_W, MENU_BTN_H, "RESUME (ESC)"))
    {
        return MENU_ACTION_RESUME;
    }

    if (UI_PillButton(CENTER_X, WIN_H * 0.55f, PAUSE_BTN_W, MENU_BTN_H, "RESTART (R)"))
    {
        pendingConfirm = MENU_ACTION_RESTART;
    }

    if (UI_PillButton(CENTER_X, WIN_H * 0.68f, PAUSE_BTN_W, MENU_BTN_H, "MAIN MENU (Q)"))
    {
        pendingConfirm = MENU_ACTION_MAIN_MENU;
    }

    if (acceptKeys && pendingConfirm == MENU_ACTION_NONE)
    {
        if (CP_Input_KeyTriggered(KEY_ESCAPE) || CP_Input_KeyTriggered(KEY_SPACE) || CP_Input_KeyTriggered(KEY_P))
        {
            UI_PlayClick();
            return MENU_ACTION_RESUME;
        }

        if (CP_Input_KeyTriggered(KEY_R))
        {
            UI_PlayClick();
            pendingConfirm = MENU_ACTION_RESTART;
        }
        else if (CP_Input_KeyTriggered(KEY_Q))
        {
            UI_PlayClick();
            pendingConfirm = MENU_ACTION_MAIN_MENU;
        }
    }

    return MENU_ACTION_NONE;
}

/* ---------------------------------------------------------------- results screen */

static void DrawDiamondIcon(float x, float y, float s)
{
    CP_Settings_Fill(CP_Color_Create(255, 210, 50, 255));
    CP_Graphics_DrawTriangle(x - 20.0f * s, y, x + 20.0f * s, y, x, y - 22.0f * s);
    CP_Graphics_DrawTriangle(x - 20.0f * s, y, x + 20.0f * s, y, x, y + 22.0f * s);
}

/* Writes value with thousands separators, e.g. 12345 -> "12,345". */
static void FormatNumber(int value, char* output, size_t outputSize)
{
    char digits[32];
    size_t length;
    size_t written = 0;

    if (output == NULL || outputSize == 0)
    {
        return;
    }

    if (value < 0)
    {
        value = 0;
    }

    snprintf(digits, sizeof(digits), "%d", value);
    length = strlen(digits);

    for (size_t i = 0; i < length && written + 1 < outputSize; i++)
    {
        if (i > 0 && (length - i) % 3 == 0)
        {
            output[written++] = ',';

            if (written + 1 >= outputSize)
            {
                break;
            }
        }

        output[written++] = digits[i];
    }

    output[written] = '\0';
}

/* "GAME OVER" with each letter spaced out and outlined, plus an underline. */
static void DrawResultsTitle(const char* title, float s)
{
    float size = WIN_H * 0.065f;
    float gap = size * 0.18f;
    float y = WIN_H * 0.205f;
    int length = (int)strlen(title);
    float totalWidth = (float)length * size + (float)(length > 0 ? length - 1 : 0) * gap;
    float x = CENTER_X - totalWidth * 0.5f + size * 0.5f;

    CP_Settings_TextAlignment(CP_TEXT_ALIGN_H_CENTER, CP_TEXT_ALIGN_V_MIDDLE);

    for (int i = 0; i < length; i++)
    {
        char letter[2] = { title[i], '\0' };

        UI_DrawTextWithOutline(letter, x, y, size,
            CP_Color_Create(255, 255, 255, 255), CP_Color_Create(0, 0, 0, 255), 4.0f * s);
        x += size + gap;
    }

    CP_Settings_RectMode(CP_POSITION_CORNER);
    CP_Settings_Fill(CP_Color_Create(255, 255, 255, 255));
    CP_Graphics_DrawRect(CENTER_X - totalWidth * 0.5f, WIN_H * 0.255f, totalWidth, 6.0f * s);
}

static void DrawResultsPanel(float s)
{
    float boxW = WIN_W * 0.76f;
    float boxH = WIN_H * 0.76f;
    float boxX = CENTER_X - boxW * 0.5f;
    float boxY = WIN_H * 0.12f;
    float border = 6.0f * s;

    CP_Settings_NoStroke();
    CP_Settings_RectMode(CP_POSITION_CORNER);

    /* Dark red tint over gameplay, then the panel and its four borders. */
    CP_Settings_Fill(CP_Color_Create(40, 0, 0, 170));
    CP_Graphics_DrawRect(0.0f, 0.0f, WIN_W, WIN_H);

    CP_Settings_Fill(CP_Color_Create(25, 5, 5, 215));
    CP_Graphics_DrawRect(boxX, boxY, boxW, boxH);

    CP_Settings_Fill(CP_Color_Create(180, 50, 50, 255));
    CP_Graphics_DrawRect(boxX, boxY, boxW, border);
    CP_Graphics_DrawRect(boxX, boxY + boxH - border, boxW, border);
    CP_Graphics_DrawRect(boxX, boxY, border, boxH);
    CP_Graphics_DrawRect(boxX + boxW - border, boxY, border, boxH);

    /* Divider between the results and the buttons. */
    CP_Settings_Fill(CP_Color_Create(255, 255, 255, 255));
    CP_Graphics_DrawRect(boxX + boxW * 0.08f, WIN_H * 0.655f, boxW * 0.84f, 4.0f * s);
}

/* Pulsing, tilted "NEW!" tag next to the score. */
static void DrawNewHighScoreTag(float x, float y, float rowSize, float s)
{
    float pulse = 1.0f + 0.08f * sinf(CP_System_GetSeconds() * 6.0f);
    float tagSize = rowSize * 0.9f * pulse;

    CP_Settings_Translate(x + 2.0f * tagSize, y);
    CP_Settings_Rotate(-12.0f);
    CP_Settings_TextAlignment(CP_TEXT_ALIGN_H_CENTER, CP_TEXT_ALIGN_V_MIDDLE);
    UI_DrawTextWithOutline("NEW!", 0.0f, 0.0f, tagSize,
        CP_Color_Create(255, 220, 50, 255), CP_Color_Create(120, 20, 20, 255), 2.0f * s);
    CP_Settings_ResetMatrix();
}

MenuAction GameMenus_DeathScreen(const RunResults* results)
{
    float s = UI_GetScale();
    float rowSize = WIN_H * 0.028f;
    float rowGap = WIN_H * 0.072f;
    float firstRowY = WIN_H * 0.37f;
    float resultsLeft = CENTER_X - 225.0f * s;
    float labelX = resultsLeft + 32.0f * s;
    float valueX = resultsLeft + 310.0f * s;
    CP_Color rowColor = CP_Color_Create(255, 220, 220, 255);
    CP_Color goldColor = CP_Color_Create(255, 220, 50, 255);
    char scoreText[32];
    char multiplierText[16];
    char finalText[32];
    char coinsText[32];

    FormatNumber(results->baseScore, scoreText, sizeof(scoreText));
    FormatNumber(results->finalScore, finalText, sizeof(finalText));
    FormatNumber(results->coins, coinsText, sizeof(coinsText));
    snprintf(multiplierText, sizeof(multiplierText), "x%d", results->multiplier);

    DrawResultsPanel(s);

    if (UI_GetTitleFont() != NULL)
    {
        CP_Font_Set(UI_GetTitleFont());
    }

    DrawResultsTitle("GAME OVER", s);

    /* What ended the run, so the player knows what to avoid next time. */
    if (results->causeText != NULL)
    {
        DrawBodyText(results->causeText, 0.295f, WIN_H * 0.045f, CP_Color_Create(255, 200, 90, 255));
    }

    if (UI_GetTitleFont() != NULL)
    {
        CP_Font_Set(UI_GetTitleFont());
    }

    CP_Settings_TextAlignment(CP_TEXT_ALIGN_H_LEFT, CP_TEXT_ALIGN_V_MIDDLE);
    CP_Settings_TextSize(rowSize);

    /* Row 1: score before the multiplier. */
    DrawDiamondIcon(resultsLeft, firstRowY, s);
    CP_Settings_Fill(rowColor);
    CP_Font_DrawText("SCORE:", labelX, firstRowY);
    CP_Font_DrawText(scoreText, valueX, firstRowY);

    /* Row 2: multiplier. */
    DrawDiamondIcon(resultsLeft, firstRowY + rowGap, s);
    CP_Settings_Fill(rowColor);
    CP_Font_DrawText("MULTIPLIER:", labelX, firstRowY + rowGap);
    CP_Settings_Fill(goldColor);
    CP_Font_DrawText(multiplierText, valueX, firstRowY + rowGap);

    /* Row 3: final score (with the NEW! tag for a high score). */
    DrawDiamondIcon(resultsLeft, firstRowY + rowGap * 2.0f, s);
    CP_Settings_Fill(rowColor);
    CP_Font_DrawText("FINAL SCORE:", labelX, firstRowY + rowGap * 2.0f);
    CP_Settings_Fill(goldColor);
    CP_Font_DrawText(finalText, valueX, firstRowY + rowGap * 2.0f);

    if (results->isNewHighScore)
    {
        DrawNewHighScoreTag(valueX + (float)strlen(finalText) * rowSize + 12.0f * s,
            firstRowY + rowGap * 2.0f, rowSize, s);
        CP_Settings_TextAlignment(CP_TEXT_ALIGN_H_LEFT, CP_TEXT_ALIGN_V_MIDDLE);
        CP_Settings_TextSize(rowSize);
    }

    /* Row 4: coins, with a simple coin icon. */
    CP_Settings_Fill(CP_Color_Create(230, 150, 30, 255));
    CP_Graphics_DrawCircle(resultsLeft, firstRowY + rowGap * 3.0f, 32.0f * s);
    CP_Settings_Fill(CP_Color_Create(255, 200, 60, 255));
    CP_Graphics_DrawCircle(resultsLeft, firstRowY + rowGap * 3.0f, 20.0f * s);
    CP_Settings_Fill(rowColor);
    CP_Font_DrawText("TOTAL COINS:", labelX, firstRowY + rowGap * 3.0f);
    CP_Font_DrawText(coinsText, valueX, firstRowY + rowGap * 3.0f);

    /* Buttons and keyboard shortcuts. */
    if (UI_PillButton(CENTER_X - RESULT_BTN_OFFSET, RESULT_BTN_Y, RESULT_BTN_W, MENU_BTN_H, "RESTART (R)"))
    {
        return MENU_ACTION_RESTART;
    }

    if (UI_PillButton(CENTER_X + RESULT_BTN_OFFSET, RESULT_BTN_Y, RESULT_BTN_W, MENU_BTN_H, "MAIN MENU (Q)"))
    {
        return MENU_ACTION_MAIN_MENU;
    }

    if (CP_Input_KeyTriggered(KEY_R) || CP_Input_KeyTriggered(KEY_ENTER))
    {
        UI_PlayClick();
        return MENU_ACTION_RESTART;
    }

    if (CP_Input_KeyTriggered(KEY_Q) || CP_Input_KeyTriggered(KEY_ESCAPE))
    {
        UI_PlayClick();
        return MENU_ACTION_MAIN_MENU;
    }

    return MENU_ACTION_NONE;
}
