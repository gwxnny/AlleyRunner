/*!
@file       : ui.c
@author     : Ggwendolynn Lee Rasni (ggwendolynnlee.r@digipen.edu) - button and text style
@co-author  : Lek Kar Lim (lekkar.lim@digipen.edu) - HUD panel style
              Isaac Tan (isaacd.tan@digipen.edu) - shared module, press/release feedback
@team       : IsaacACat
@module     : CSD1401 Software Engineering Project 1
@section    : B
@project    : Alley Runner
@brief      : Implementation of the shared UI system (see ui.h).

All content © 2026 DigiPen Institute of Technology Singapore. All Rights Reserved.
*//*________________________________________________________________________________*/
#include "ui.h"

#include <math.h>

#define UI_TITLE_FONT_PATH  "Assets/PressStart2P-Regular.ttf"
#define UI_BODY_FONT_PATH   "Assets/VT323-Regular.ttf"
#define UI_CLICK_SOUND_PATH "Assets/main_menu_btn_sound.mp3"

#define UI_CLICK_VOLUME     1.0f
#define UI_BUTTON_TEXT_RATIO 0.30f  /* label height as a fraction of button height */
#define UI_PRESS_SHRINK     0.96f   /* pressed buttons shrink slightly */
#define UI_PRESS_DROP       3.0f    /* pressed buttons move down (design pixels) */

static CP_Font titleFont = NULL;
static CP_Font bodyFont = NULL;
static CP_Sound clickSound = NULL;

/* Where the current left-button press started (drawing coordinates). */
static float pressX = -1.0f;
static float pressY = -1.0f;
static unsigned pressFrame = 0xFFFFFFFFu;   /* no frame recorded yet */

/*! Specification for function UI_Load:
@brief Loads the shared fonts and button click sound.

Safe to call more than once: anything already loaded is kept, so fonts are
not loaded again on every state change.

@param none.
@return none.
*//*________________________________________________________________________________*/
void UI_Load(void)
{
    /* Fonts are loaded once for the whole program. Loading them again on
       every state change would keep adding fonts inside CProcessing. */
    if (titleFont == NULL)
    {
        titleFont = CP_Font_Load(UI_TITLE_FONT_PATH);
    }

    if (bodyFont == NULL)
    {
        bodyFont = CP_Font_Load(UI_BODY_FONT_PATH);
    }

    if (clickSound == NULL)
    {
        clickSound = CP_Sound_Load(UI_CLICK_SOUND_PATH);
    }
}

/*! Specification for function UI_Unload:
@brief Frees the shared fonts and button click sound.

@param none.
@return none.
*//*________________________________________________________________________________*/
void UI_Unload(void)
{
    if (titleFont != NULL)
    {
        CP_Font_Free(titleFont);
        titleFont = NULL;
    }

    if (bodyFont != NULL)
    {
        CP_Font_Free(bodyFont);
        bodyFont = NULL;
    }

    if (clickSound != NULL)
    {
        CP_Sound_Free(clickSound);
        clickSound = NULL;
    }
}

/*! Specification for function UI_GetTitleFont:
@brief Gets the title font (Press Start 2P).

@param none.
@return The title font, or NULL if it failed to load.
*//*________________________________________________________________________________*/
CP_Font UI_GetTitleFont(void)
{
    return titleFont;
}

/*! Specification for function UI_GetBodyFont:
@brief Gets the body font (VT323).

@param none.
@return The body font, or NULL if it failed to load.
*//*________________________________________________________________________________*/
CP_Font UI_GetBodyFont(void)
{
    return bodyFont;
}

/*! Specification for function UI_GetScale:
@brief Gets the scale factor from the 1280 x 720 design canvas to the window.

@param none.
@return The smaller of the width and height ratios, so layouts keep their proportions.
*//*________________________________________________________________________________*/
float UI_GetScale(void)
{
    return fminf((float)CP_System_GetWindowWidth() / UI_DESIGN_WIDTH,
        (float)CP_System_GetWindowHeight() / UI_DESIGN_HEIGHT);
}

/*! Specification for function UI_GetMouse:
@brief Gets the mouse position in drawing coordinates.

Maps the native client-area position to the canvas, which is needed when
borderless or fullscreen mode stretches the window.

@param x - Receives the mouse's horizontal position.
@param y - Receives the mouse's vertical position.
@return none.
*//*________________________________________________________________________________*/
void UI_GetMouse(float* x, float* y)
{
    HWND window = CP_System_GetWindowHandle();
    POINT cursor;
    RECT client;

    *x = CP_Input_GetMouseX();
    *y = CP_Input_GetMouseY();

    if (window != NULL && GetCursorPos(&cursor) &&
        ScreenToClient(window, &cursor) && GetClientRect(window, &client))
    {
        int clientW = client.right - client.left;
        int clientH = client.bottom - client.top;

        if (clientW > 0 && clientH > 0)
        {
            *x = (float)cursor.x * (float)CP_System_GetWindowWidth() / (float)clientW;
            *y = (float)cursor.y * (float)CP_System_GetWindowHeight() / (float)clientH;
        }
    }
}

/*! Specification for function IsPointInRect:
@brief Checks whether a point is inside a centre-anchored rectangle.

@param px - The point's horizontal position.
@param py - The point's vertical position.
@param centerX - The rectangle's horizontal centre.
@param centerY - The rectangle's vertical centre.
@param width - The rectangle's width.
@param height - The rectangle's height.
@return true - If the point is inside the rectangle, otherwise false.
*//*________________________________________________________________________________*/
static bool IsPointInRect(float px, float py, float centerX, float centerY, float width, float height)
{
    return px >= centerX - width * 0.5f && px <= centerX + width * 0.5f &&
        py >= centerY - height * 0.5f && py <= centerY + height * 0.5f;
}

/*! Specification for function UI_IsMouseOver:
@brief Checks whether the mouse is inside a centre-anchored rectangle.

@param centerX - The rectangle's horizontal centre.
@param centerY - The rectangle's vertical centre.
@param width - The rectangle's width.
@param height - The rectangle's height.
@return true - If the mouse is inside the rectangle, otherwise false.
*//*________________________________________________________________________________*/
bool UI_IsMouseOver(float centerX, float centerY, float width, float height)
{
    float mouseX;
    float mouseY;

    UI_GetMouse(&mouseX, &mouseY);
    return IsPointInRect(mouseX, mouseY, centerX, centerY, width, height);
}

/*! Specification for function UI_TrackPress:
@brief Records where the current left mouse press started.

Runs at most once per frame. Buttons use this so they only activate when the
press also started on them.

@param none.
@return none.
*//*________________________________________________________________________________*/
void UI_TrackPress(void)
{
    /* Only record once per frame, however many buttons are drawn. */
    unsigned frame = CP_System_GetFrameCount();

    if (frame == pressFrame)
    {
        return;
    }

    pressFrame = frame;

    if (CP_Input_MouseTriggered(MOUSE_BUTTON_LEFT))
    {
        UI_GetMouse(&pressX, &pressY);
    }
}

/*! Specification for function UI_PlayClick:
@brief Plays the shared button click sound on the SFX group.

@param none.
@return none.
*//*________________________________________________________________________________*/
void UI_PlayClick(void)
{
    if (clickSound != NULL)
    {
        CP_Sound_PlayAdvanced(clickSound, UI_CLICK_VOLUME, 1.0f, false, CP_SOUND_GROUP_SFX);
    }
}

/*! Specification for function UI_DrawPillButton:
@brief Draws a rounded pill button and its label without handling input.

@param x - The horizontal centre of the button.
@param y - The vertical centre of the button.
@param w - The button width.
@param h - The button height.
@param label - The text on the button.
@param hovered - true to draw the hover colour.
@param pressed - true to draw the pressed-in look.
@return none.
*//*________________________________________________________________________________*/
void UI_DrawPillButton(float x, float y, float w, float h, const char* label, bool hovered, bool pressed)
{
    float scale = UI_GetScale();

    /* Pressed buttons look pushed in: smaller, lower and darker. */
    if (pressed)
    {
        w *= UI_PRESS_SHRINK;
        h *= UI_PRESS_SHRINK;
        y += UI_PRESS_DROP * scale;
    }

    CP_Settings_RectMode(CP_POSITION_CENTER);
    CP_Settings_Stroke(CP_Color_Create(20, 20, 20, 255));
    CP_Settings_StrokeWeight(5.0f * scale);

    if (pressed)
    {
        CP_Settings_Fill(CP_Color_Create(150, 45, 60, 255));
    }
    else if (hovered)
    {
        CP_Settings_Fill(CP_Color_Create(225, 75, 95, 255));
    }
    else
    {
        CP_Settings_Fill(CP_Color_Create(195, 65, 80, 255));
    }

    CP_Graphics_DrawRectAdvanced(x, y, w, h, 0.0f, h / 2.0f);

    /* Glossy highlight strip on the top half (hidden while pressed). */
    CP_Settings_NoStroke();

    if (!pressed)
    {
        CP_Settings_Fill(CP_Color_Create(255, 255, 255, 45));
        CP_Graphics_DrawRectAdvanced(x, y - h * 0.18f, w * 0.85f, h * 0.35f, 0.0f, h * 0.18f);
    }

    if (titleFont != NULL)
    {
        CP_Font_Set(titleFont);
    }

    CP_Settings_TextAlignment(CP_TEXT_ALIGN_H_CENTER, CP_TEXT_ALIGN_V_MIDDLE);
    CP_Settings_TextSize(h * UI_BUTTON_TEXT_RATIO);
    CP_Settings_Fill(hovered ? CP_Color_Create(255, 255, 255, 255) : CP_Color_Create(235, 235, 235, 255));
    CP_Font_DrawText(label, x, y);
}

/*! Specification for function UI_PillButton:
@brief Draws a pill button and handles its hover, hold and release states.

Plays the click sound when the button activates.

@param x - The horizontal centre of the button.
@param y - The vertical centre of the button.
@param w - The button width.
@param h - The button height.
@param label - The text on the button.
@return true - On the frame the left button is released over the button (and the press
         started on it), otherwise false.
*//*________________________________________________________________________________*/
bool UI_PillButton(float x, float y, float w, float h, const char* label)
{
    bool hovered;
    bool pressStartedHere;
    bool pressed;
    bool released;

    UI_TrackPress();

    hovered = UI_IsMouseOver(x, y, w, h);
    pressStartedHere = IsPointInRect(pressX, pressY, x, y, w, h);
    pressed = hovered && pressStartedHere && CP_Input_MouseDown(MOUSE_BUTTON_LEFT);
    released = hovered && pressStartedHere && CP_Input_MouseReleased(MOUSE_BUTTON_LEFT);

    UI_DrawPillButton(x, y, w, h, label, hovered, pressed);

    if (released)
    {
        UI_PlayClick();
    }

    return released;
}

/*! Specification for function UI_DrawTextWithOutline:
@brief Draws text with an outline around it.

The outline is made by drawing copies of the text around its position.

@param text - The text to draw.
@param x - The horizontal position of the text.
@param y - The vertical position of the text.
@param size - The text size.
@param fillColor - The colour of the main text.
@param outlineColor - The colour of the outline.
@param outlineWidth - The distance between the text and its outline copies.
@return none.
*//*________________________________________________________________________________*/
void UI_DrawTextWithOutline(const char* text, float x, float y, float size,
    CP_Color fillColor, CP_Color outlineColor, float outlineWidth)
{
    CP_Settings_TextSize(size);
    CP_Settings_Fill(outlineColor);

    /* Eight neighbours plus four slightly further copies make a round outline. */
    CP_Font_DrawText(text, x - outlineWidth, y - outlineWidth);
    CP_Font_DrawText(text, x, y - outlineWidth);
    CP_Font_DrawText(text, x + outlineWidth, y - outlineWidth);
    CP_Font_DrawText(text, x - outlineWidth, y);
    CP_Font_DrawText(text, x + outlineWidth, y);
    CP_Font_DrawText(text, x - outlineWidth, y + outlineWidth);
    CP_Font_DrawText(text, x, y + outlineWidth);
    CP_Font_DrawText(text, x + outlineWidth, y + outlineWidth);
    CP_Font_DrawText(text, x - outlineWidth * 1.4f, y);
    CP_Font_DrawText(text, x + outlineWidth * 1.4f, y);
    CP_Font_DrawText(text, x, y - outlineWidth * 1.4f);
    CP_Font_DrawText(text, x, y + outlineWidth * 1.4f);

    CP_Settings_Fill(fillColor);
    CP_Font_DrawText(text, x, y);
}

/*! Specification for function UI_DrawHudPanel:
@brief Draws a dark eight-sided HUD panel with a light border.

@param x - The horizontal centre of the panel.
@param y - The vertical centre of the panel.
@param w - The panel width.
@param h - The panel height.
@param hovered - true to draw the highlighted (hover) colours.
@return none.
*//*________________________________________________________________________________*/
void UI_DrawHudPanel(float x, float y, float w, float h, bool hovered)
{
    float scale = UI_GetScale();
    float cut = 7.0f * scale;   /* size of the clipped corners */

    float left = x - w * 0.5f;
    float right = x + w * 0.5f;
    float top = y - h * 0.5f;
    float bottom = y + h * 0.5f;

    CP_Settings_RectMode(CP_POSITION_CORNER);
    CP_Settings_NoStroke();
    CP_Settings_Fill(hovered ? CP_Color_Create(35, 35, 40, 235) : CP_Color_Create(5, 5, 10, 220));

    /* Fill: two overlapping rectangles plus four corner triangles. */
    CP_Graphics_DrawRect(left + cut, top, w - 2.0f * cut, h);
    CP_Graphics_DrawRect(left, top + cut, w, h - 2.0f * cut);
    CP_Graphics_DrawTriangle(left + cut, top, left + cut, top + cut, left, top + cut);
    CP_Graphics_DrawTriangle(right - cut, top, right, top + cut, right - cut, top + cut);
    CP_Graphics_DrawTriangle(left, bottom - cut, left + cut, bottom - cut, left + cut, bottom);
    CP_Graphics_DrawTriangle(right - cut, bottom - cut, right, bottom - cut, right - cut, bottom);

    /* Border following the same eight-sided outline. */
    CP_Settings_Stroke(hovered ? CP_Color_Create(255, 225, 100, 255) : CP_Color_Create(205, 205, 215, 255));
    CP_Settings_StrokeWeight(2.0f * scale);

    CP_Graphics_DrawLine(left + cut, top, right - cut, top);
    CP_Graphics_DrawLine(right - cut, top, right, top + cut);
    CP_Graphics_DrawLine(right, top + cut, right, bottom - cut);
    CP_Graphics_DrawLine(right, bottom - cut, right - cut, bottom);
    CP_Graphics_DrawLine(right - cut, bottom, left + cut, bottom);
    CP_Graphics_DrawLine(left + cut, bottom, left, bottom - cut);
    CP_Graphics_DrawLine(left, bottom - cut, left, top + cut);
    CP_Graphics_DrawLine(left, top + cut, left + cut, top);

    CP_Settings_NoStroke();
}   