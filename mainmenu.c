/*!
@file       : mainmenu.c
@author     : Ggwendolynn Lee Rasni (ggwendolynnlee.r@digipen.edu)
@co-author  : Isaac Tan (isaacd.tan@digipen.edu) - shared UI/transition hookup,
              keyboard shortcuts, credits fixes
@team       : IsaacACat
@module     : CSD1401 Software Engineering Project 1
@section    : B
@project    : Alley Runner
@brief      : Main menu state: title, animated chase in the background, and the
              Play / How To Play / Settings / Credits / Quit buttons with their
              pop-up screens.

All content © 2026 DigiPen Institute of Technology Singapore. All Rights Reserved.
*//*________________________________________________________________________________*/
#ifndef WIN32_LEAN_AND_MEAN // keep windows.h small to avoid clashes with other libraries
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX // stop windows.h defining min / max macros
#define NOMINMAX
#endif

#include <Windows.h> // monitor information, window styles and native window positioning

#include "mainmenu.h"
#include "game.h"
#include "transition.h"
#include "ui.h"

#include <stdio.h>
#include <stdbool.h>
#include <math.h>

/* ============================================================
   Settings
   ============================================================ */

// delay before quitting so the click sound is not cut off.
#define QUIT_SOUND_DELAY 0.25f

// default volume for both groups (0.0 to 1.0).
#define DEFAULT_VOLUME 0.2f

// main menu buttons, on the 1280 x 720 design canvas.
#define MENU_BUTTON_W        320.0f
#define MENU_BUTTON_H        64.0f
#define MENU_BUTTON_SPACING  78.0f
#define MENU_BUTTONS_TOP     0.43f   // first button's y as a fraction of the window height
#define WINDOWED_CONTENT_SCALE 0.72f // buttons are smaller in windowed mode

// background chase animation.
#define MENU_CHASE_SPEED 160.0f
#define MENU_CHASE_GAP   500.0f   // distance between the ball and the pin

// sprite sheets for the background chase.
// the main menu has its own costumed pin; gameplay uses bowling_pin_running.png.
#define MENU_PIN_PATH        "Assets/running_pin_spiderman.PNG"
#define MENU_PIN_FRAME_COUNT 4       // 2000 x 500 sheet = 4 frames
#define MENU_PIN_FRAME_W     500.0f
#define MENU_PIN_FRAME_H     500.0f
#define MENU_PIN_FRAME_TIME  0.10f   // smaller = faster animation
#define MENU_PIN_SIZE        250.0f  // drawn size

#define MENU_BALL_PATH        "Assets/venom_bowling_ball_chasing.png"
#define MENU_BALL_FRAME_COUNT 8
#define MENU_BALL_FRAME_W     500.0f
#define MENU_BALL_FRAME_H     500.0f
#define MENU_BALL_FRAME_TIME  0.08f
#define MENU_BALL_SIZE        500.0f

/* ============================================================
   State
   ============================================================ */

// a menu button: centre position, size and text.
typedef struct
{
    float x;
    float y;
    float width;
    float height;
    const char* label;
} Button;

// which pop-up (if any) covers the menu.
typedef enum
{
    MENU_POPUP_NONE,
    MENU_POPUP_HOW_TO_PLAY,
    MENU_POPUP_SETTINGS,
    MENU_POPUP_CREDITS,
    MENU_POPUP_QUIT_CONFIRM
} MenuPopup;

// display modes the settings pop-up can switch between.
typedef enum
{
    DISPLAY_WINDOWED,
    DISPLAY_BORDERLESS,
    DISPLAY_FULLSCREEN,
    DISPLAY_MODE_COUNT
} DisplayMode;

// sounds and images, loaded in Init and freed in Exit.
static CP_Sound menuMusic = NULL;
static CP_Image bgImage = NULL;
static CP_Image teamLogo = NULL;
static CP_Image creditsLogo = NULL;
static CP_Image menuPin = NULL;
static CP_Image menuBall = NULL;

static Button btnPlay;
static Button btnHowTo;
static Button btnSettings;
static Button btnCredits;
static Button btnQuit;

static MenuPopup popup = MENU_POPUP_NONE;
static bool quitting = false;       // "Yes" was chosen: waiting for the click sound
static float quitTimer = 0.0f;

// audio settings persist for the whole program.
static bool audioSettingsInitialized = false;
static float musicVolume = DEFAULT_VOLUME;
static float sfxVolume = DEFAULT_VOLUME;
static int activeVolumeSlider = 0;  // 0 = none, 1 = music, 2 = SFX

// display settings persist for the whole program.
static DisplayMode displayMode = DISPLAY_WINDOWED;
static int windowedWidth = 1600;
static int windowedHeight = 900;
static bool displaySettingsInitialized = false;

// title bobbing and background chase.
static float floatTimer = 0.0f;
static float menuChaseX = 0.0f;
static float menuPinFrameTimer = 0.0f;
static float menuBallFrameTimer = 0.0f;
static int menuPinFrame = 0;
static int menuBallFrame = 0;

/* ============================================================
   Helpers
   ============================================================ */

/*! Specification for function MenuContentScale:
@brief Gets the size multiplier for the menu buttons and sliders.

@param none.
@return The UI scale, reduced in windowed mode so the menu fits the background art.
*//*________________________________________________________________________________*/
static float MenuContentScale(void)
{
    float scale = UI_GetScale();

    if (displayMode == DISPLAY_WINDOWED)
    {
        scale *= WINDOWED_CONTENT_SCALE;
    }

    return scale;
}

/*! Specification for function MenuIsBusy:
@brief Checks whether the menu should ignore input this frame.

@param none.
@return true - If the game is quitting or a screen transition is fading out, otherwise false.
*//*________________________________________________________________________________*/
static bool MenuIsBusy(void)
{
    return quitting || Transition_IsBusy();
}

/*! Specification for function MenuButton:
@brief Draws a main menu button and handles its input.

While the menu is busy the button is drawn but cannot be clicked.

@param btn - The button to draw (centre position, size and label).
@return true - If the button was clicked this frame, otherwise false.
*//*________________________________________________________________________________*/
static bool MenuButton(const Button* btn)
{
    if (MenuIsBusy())
    {
        UI_DrawPillButton(btn->x, btn->y, btn->width, btn->height, btn->label, false, false);
        return false;
    }

    return UI_PillButton(btn->x, btn->y, btn->width, btn->height, btn->label);
}

/*! Specification for function UpdateMenuButtonLayout:
@brief Sets the positions and sizes of the five main menu buttons.

The buttons are arranged in a column in the centre of the window.

@param windowW - The window width.
@param windowH - The window height.
@return none.
*//*________________________________________________________________________________*/
static void UpdateMenuButtonLayout(float windowW, float windowH)
{
    float centerX = windowW / 2.0f;
    float startY = windowH * MENU_BUTTONS_TOP;
    float scale = MenuContentScale();
    float w = MENU_BUTTON_W * scale;
    float h = MENU_BUTTON_H * scale;
    float spacing = MENU_BUTTON_SPACING * scale;

    btnPlay = (Button){ centerX, startY,                  w, h, "PLAY" };
    btnHowTo = (Button){ centerX, startY + spacing,        w, h, "HOW TO PLAY" };
    btnSettings = (Button){ centerX, startY + spacing * 2.0f, w, h, "SETTINGS" };
    btnCredits = (Button){ centerX, startY + spacing * 3.0f, w, h, "CREDITS" };
    btnQuit = (Button){ centerX, startY + spacing * 4.0f, w, h, "QUIT" };
}

/*! Specification for function ApplyAudioVolumes:
@brief Applies the current music and sound effect volume settings to the sound groups.

@param none.
@return none.
*//*________________________________________________________________________________*/
static void ApplyAudioVolumes(void)
{
    CP_Sound_SetGroupVolume(CP_SOUND_GROUP_MUSIC, musicVolume);
    CP_Sound_SetGroupVolume(CP_SOUND_GROUP_SFX, sfxVolume);
}

/*! Specification for function DrawDimmer:
@brief Darkens everything behind a pop-up.

@param windowW - The window width.
@param windowH - The window height.
@param alpha - How dark the overlay is, from 0 (clear) to 255 (black).
@return none.
*//*________________________________________________________________________________*/
static void DrawDimmer(float windowW, float windowH, int alpha)
{
    CP_Settings_RectMode(CP_POSITION_CENTER);
    CP_Settings_NoStroke();
    CP_Settings_Fill(CP_Color_Create(0, 0, 0, alpha));
    CP_Graphics_DrawRect(windowW * 0.5f, windowH * 0.5f, windowW, windowH);
}

/*! Specification for function DrawPopupPanel:
@brief Draws a dark rounded pop-up panel with a pink border.

@param x - The horizontal centre of the panel.
@param y - The vertical centre of the panel.
@param w - The panel width.
@param h - The panel height.
@return none.
*//*________________________________________________________________________________*/
static void DrawPopupPanel(float x, float y, float w, float h)
{
    float scale = UI_GetScale();

    CP_Settings_RectMode(CP_POSITION_CENTER);
    CP_Settings_Fill(CP_Color_Create(20, 20, 25, 250));
    CP_Settings_Stroke(CP_Color_Create(225, 75, 95, 255));
    CP_Settings_StrokeWeight(4.0f * scale);
    CP_Graphics_DrawRectAdvanced(x, y, w, h, 0.0f, 15.0f * scale);
    CP_Settings_NoStroke();
}

/*! Specification for function UseBodyFont:
@brief Sets the body font (VT323) for the next text drawn, if it loaded.

@param none.
@return none.
*//*________________________________________________________________________________*/
static void UseBodyFont(void)
{
    if (UI_GetBodyFont() != NULL)
    {
        CP_Font_Set(UI_GetBodyFont());
    }
}

/*! Specification for function UseTitleFont:
@brief Sets the title font (Press Start 2P) for the next text drawn, if it loaded.

@param none.
@return none.
*//*________________________________________________________________________________*/
static void UseTitleFont(void)
{
    if (UI_GetTitleFont() != NULL)
    {
        CP_Font_Set(UI_GetTitleFont());
    }
}

/* ============================================================
   Settings pop-up
   ============================================================ */

/*! Specification for function DrawVolumeSlider:
@brief Draws a volume slider with a label and percentage.

This function only draws the slider. It does not handle mouse input.

@param label - The name displayed above the slider.
@param left - The horizontal position of the slider's left edge.
@param y - The vertical position of the slider.
@param width - The width of the slider.
@param value - The volume level, from 0.0 to 1.0.
@return none.
*//*________________________________________________________________________________*/
static void DrawVolumeSlider(const char* label, float left, float y, float width, float value)
{
    float scale = MenuContentScale();
    char text[80];

    snprintf(text, sizeof(text), "%s: %d%%", label, (int)(value * 100.0f + 0.5f));

    CP_Settings_NoStroke();
    CP_Settings_TextAlignment(CP_TEXT_ALIGN_H_LEFT, CP_TEXT_ALIGN_V_MIDDLE);
    CP_Settings_TextSize(28.0f * scale);
    CP_Settings_Fill(CP_Color_Create(255, 255, 255, 255));
    CP_Font_DrawText(text, left, y - 32.0f * scale);

    // track
    CP_Settings_RectMode(CP_POSITION_CENTER);
    CP_Settings_Fill(CP_Color_Create(75, 75, 85, 255));
    CP_Graphics_DrawRectAdvanced(left + width / 2.0f, y, width, 12.0f * scale, 0.0f, 6.0f * scale);

    // filled part
    if (value > 0.0f)
    {
        CP_Settings_Fill(CP_Color_Create(225, 75, 95, 255));
        CP_Graphics_DrawRect(left + width * value / 2.0f, y, width * value, 12.0f * scale);
    }

    // knob
    CP_Settings_Fill(CP_Color_Create(255, 255, 255, 255));
    CP_Graphics_DrawRectAdvanced(left + width * value, y, 18.0f * scale, 30.0f * scale, 0.0f, 7.0f * scale);
}

/*! Specification for function GetDisplayModeLabel:
@brief Gets the text label for the current display mode.

@param none.
@return The name of the current display mode.
*//*________________________________________________________________________________*/
static const char* GetDisplayModeLabel(void)
{
    switch (displayMode)
    {
    case DISPLAY_BORDERLESS: return "BORDERLESS WINDOWED";
    case DISPLAY_FULLSCREEN: return "FULLSCREEN";
    default:                 return "WINDOWED";
    }
}

/*! Specification for function ApplyDisplayMode:
@brief Changes the window to the selected display mode.

Supports windowed, borderless windowed, and fullscreen modes.
Remembers the windowed size before leaving windowed mode.

@param nextMode - The display mode to switch to.
@return none.
*//*________________________________________________________________________________*/
static void ApplyDisplayMode(DisplayMode nextMode)
{
    HWND window = CP_System_GetWindowHandle();
    MONITORINFO monitorInfo = { 0 };
    HMONITOR monitor;
    LONG_PTR style;

    if (window == NULL || nextMode == displayMode)
    {
        return;
    }

    if (displayMode == DISPLAY_WINDOWED)
    {
        windowedWidth = CP_System_GetWindowWidth();
        windowedHeight = CP_System_GetWindowHeight();
    }

    if (nextMode == DISPLAY_FULLSCREEN)
    {
        CP_System_Fullscreen();
        displayMode = nextMode;
        return;
    }

    // leave fullscreen first, then change the native window frame.
    CP_System_SetWindowSize(windowedWidth, windowedHeight);
    window = CP_System_GetWindowHandle();

    if (window == NULL)
    {
        return;
    }

    monitorInfo.cbSize = sizeof(monitorInfo);
    monitor = MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST);

    if (!GetMonitorInfo(monitor, &monitorInfo))
    {
        return;
    }

    if (nextMode == DISPLAY_BORDERLESS)
    {
        int monitorW = monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left;
        int monitorH = monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top;

        CP_System_SetWindowSize(monitorW, monitorH);
        window = CP_System_GetWindowHandle();

        if (window == NULL)
        {
            return;
        }

        // remove the title bar and borders, then cover the whole monitor.
        style = GetWindowLongPtr(window, GWL_STYLE);
        style &= ~((LONG_PTR)WS_OVERLAPPEDWINDOW);
        style |= WS_POPUP;
        SetWindowLongPtr(window, GWL_STYLE, style);

        SetWindowPos(window, HWND_NOTOPMOST,
            monitorInfo.rcMonitor.left, monitorInfo.rcMonitor.top, monitorW, monitorH,
            SWP_FRAMECHANGED | SWP_SHOWWINDOW);
    }
    else
    {
        RECT rect = { 0, 0, windowedWidth, windowedHeight };
        DWORD exStyle;
        int outerW;
        int outerH;
        int workW;
        int workH;
        int x;
        int y;

        // restore the normal title bar and borders.
        style = GetWindowLongPtr(window, GWL_STYLE);
        style &= ~((LONG_PTR)WS_POPUP);
        style |= WS_OVERLAPPEDWINDOW;
        SetWindowLongPtr(window, GWL_STYLE, style);

        // outer window size needed for the requested client area, centred on the work area.
        exStyle = (DWORD)GetWindowLongPtr(window, GWL_EXSTYLE);
        AdjustWindowRectEx(&rect, (DWORD)style, GetMenu(window) != NULL, exStyle);

        outerW = rect.right - rect.left;
        outerH = rect.bottom - rect.top;
        workW = monitorInfo.rcWork.right - monitorInfo.rcWork.left;
        workH = monitorInfo.rcWork.bottom - monitorInfo.rcWork.top;
        x = monitorInfo.rcWork.left + (workW > outerW ? (workW - outerW) / 2 : 0);
        y = monitorInfo.rcWork.top + (workH > outerH ? (workH - outerH) / 2 : 0);

        SetWindowPos(window, HWND_NOTOPMOST, x, y, outerW, outerH,
            SWP_FRAMECHANGED | SWP_SHOWWINDOW);
    }

    displayMode = nextMode;
}

/*! Specification for function UpdateSettingsPopup:
@brief Draws the settings pop-up and handles its controls.

Allows the player to change the display mode, drag the music and SFX volume sliders,
and close the pop-up using the Back button or Escape key.

@param centerX - The horizontal centre of the pop-up.
@param centerY - The vertical centre of the pop-up.
@param windowW - The window width.
@param windowH - The window height.
@return none.
*//*________________________________________________________________________________*/
static void UpdateSettingsPopup(float centerX, float centerY, float windowW, float windowH)
{
    float scale = UI_GetScale();
    float sliderWidth = 400.0f * scale;
    float sliderLeft = centerX - sliderWidth / 2.0f;
    float musicY = centerY - 10.0f * scale;
    float sfxY = centerY + 85.0f * scale;
    float mouseX;
    float mouseY;

    UI_GetMouse(&mouseX, &mouseY);

    // start dragging a slider when it is pressed.
    if (CP_Input_MouseTriggered(MOUSE_BUTTON_LEFT))
    {
        bool insideSliderWidth =
            mouseX >= sliderLeft - 12.0f * scale &&
            mouseX <= sliderLeft + sliderWidth + 12.0f * scale;

        if (insideSliderWidth && fabsf(mouseY - musicY) <= 22.0f * scale)
        {
            activeVolumeSlider = 1;
        }
        else if (insideSliderWidth && fabsf(mouseY - sfxY) <= 22.0f * scale)
        {
            activeVolumeSlider = 2;
        }
    }

    // follow the mouse while held; play a sample click when an SFX drag ends.
    if (CP_Input_MouseDown(MOUSE_BUTTON_LEFT))
    {
        if (activeVolumeSlider != 0)
        {
            float value = CP_Math_ClampFloat((mouseX - sliderLeft) / sliderWidth, 0.0f, 1.0f);

            if (activeVolumeSlider == 1)
            {
                musicVolume = value;
            }
            else
            {
                sfxVolume = value;
            }

            ApplyAudioVolumes();
        }
    }
    else if (activeVolumeSlider != 0)
    {
        if (activeVolumeSlider == 2)
        {
            UI_PlayClick();
        }

        activeVolumeSlider = 0;
    }

    DrawDimmer(windowW, windowH, 150);
    DrawPopupPanel(centerX, centerY, 580.0f * scale, 520.0f * scale);

    CP_Settings_TextAlignment(CP_TEXT_ALIGN_H_CENTER, CP_TEXT_ALIGN_V_MIDDLE);
    UseTitleFont();
    CP_Settings_TextSize(22.0f * scale);
    CP_Settings_Fill(CP_Color_Create(255, 255, 255, 255));
    CP_Font_DrawText("SETTINGS", centerX, centerY - 210.0f * scale);

    UseBodyFont();
    CP_Settings_TextSize(26.0f * scale);
    CP_Font_DrawText("Display mode - click to change", centerX, centerY - 165.0f * scale);

    if (UI_PillButton(centerX, centerY - 120.0f * scale, 460.0f * scale, 52.0f * scale, GetDisplayModeLabel()))
    {
        activeVolumeSlider = 0;
        ApplyDisplayMode((DisplayMode)(((int)displayMode + 1) % DISPLAY_MODE_COUNT));
        return;  // the window changed size: redraw next frame with the new layout
    }

    UseBodyFont();
    DrawVolumeSlider("Music", sliderLeft, musicY, sliderWidth, musicVolume);
    DrawVolumeSlider("SFX", sliderLeft, sfxY, sliderWidth, sfxVolume);

    if (UI_PillButton(centerX, centerY + 190.0f * scale, 230.0f * scale, 52.0f * scale, "BACK (ESC)") ||
        CP_Input_KeyTriggered(KEY_ESCAPE))
    {
        if (CP_Input_KeyTriggered(KEY_ESCAPE))
        {
            UI_PlayClick();
        }

        popup = MENU_POPUP_NONE;
        activeVolumeSlider = 0;
    }
}

/* ============================================================
   Quit confirmation pop-up
   ============================================================ */

/*! Specification for function UpdateQuitPopup:
@brief Draws the quit confirmation pop-up and handles its controls.

Yes (or Enter) starts quitting after the click sound; No (or Escape) closes the pop-up.

@param centerX - The horizontal centre of the pop-up.
@param centerY - The vertical centre of the pop-up.
@param windowW - The window width.
@param windowH - The window height.
@return none.
*//*________________________________________________________________________________*/
static void UpdateQuitPopup(float centerX, float centerY, float windowW, float windowH)
{
    float scale = UI_GetScale();
    float buttonY = centerY + 48.0f * scale;
    bool yes = false;
    bool no = false;

    DrawDimmer(windowW, windowH, 180);
    DrawPopupPanel(centerX, centerY, 500.0f * scale, 200.0f * scale);

    UseBodyFont();
    CP_Settings_TextAlignment(CP_TEXT_ALIGN_H_CENTER, CP_TEXT_ALIGN_V_MIDDLE);
    CP_Settings_TextSize(32.0f * scale);
    CP_Settings_Fill(CP_Color_Create(255, 255, 255, 255));
    CP_Font_DrawText("Quit Alley Runner?", centerX, centerY - 38.0f * scale);

    if (quitting)
    {
        UI_DrawPillButton(centerX - 100.0f * scale, buttonY, 160.0f * scale, 52.0f * scale, "YES", false, true);
        UI_DrawPillButton(centerX + 100.0f * scale, buttonY, 160.0f * scale, 52.0f * scale, "NO", false, false);
        return;
    }

    yes = UI_PillButton(centerX - 100.0f * scale, buttonY, 160.0f * scale, 52.0f * scale, "YES");
    no = UI_PillButton(centerX + 100.0f * scale, buttonY, 160.0f * scale, 52.0f * scale, "NO");

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

    if (yes)
    {
        quitting = true;
        quitTimer = QUIT_SOUND_DELAY;
    }
    else if (no)
    {
        popup = MENU_POPUP_NONE;
    }
}

/* ============================================================
   How To Play pop-up
   ============================================================ */

/*! Specification for function DrawHowToHeading:
@brief Draws a coloured heading in the How To Play pop-up.

@param text - The heading text.
@param x - The horizontal position of the text.
@param y - The vertical position of the text.
@param scale - The UI scale.
@param color - The colour of the heading.
@return none.
*//*________________________________________________________________________________*/
static void DrawHowToHeading(const char* text, float x, float y, float scale, CP_Color color)
{
    CP_Settings_TextSize(29.0f * scale);
    CP_Settings_Fill(color);
    CP_Font_DrawText(text, x, y);
}

/*! Specification for function DrawHowToLine:
@brief Draws one line of white body text in the How To Play pop-up.

@param text - The line of text.
@param x - The horizontal position of the text.
@param y - The vertical position of the text.
@param scale - The UI scale.
@return none.
*//*________________________________________________________________________________*/
static void DrawHowToLine(const char* text, float x, float y, float scale)
{
    CP_Settings_TextSize(25.0f * scale);
    CP_Settings_Fill(CP_Color_Create(245, 245, 245, 255));
    CP_Font_DrawText(text, x, y);
}

/*! Specification for function UpdateHowToPopup:
@brief Draws the How to Play pop-up.

Shows the goal, controls, hazards, hearts and power-ups, and closes the pop-up
when the player clicks anywhere or presses Escape.

@param windowW - The window width.
@param windowH - The window height.
@return none.
*//*________________________________________________________________________________*/
static void UpdateHowToPopup(float windowW, float windowH)
{
    float scale = UI_GetScale();
    float panelW = 1120.0f * scale;
    float panelH = 610.0f * scale;
    float panelX = windowW * 0.5f;
    float panelY = windowH * 0.5f;
    float panelLeft = panelX - panelW * 0.5f;
    float panelTop = panelY - panelH * 0.5f;
    float leftX = panelLeft + 45.0f * scale;
    float rightX = panelX + 35.0f * scale;

    CP_Color muted = CP_Color_Create(195, 195, 205, 255);
    CP_Color pink = CP_Color_Create(225, 75, 95, 255);
    CP_Color gold = CP_Color_Create(255, 210, 80, 255);
    CP_Color blue = CP_Color_Create(100, 195, 255, 255);
    CP_Color purple = CP_Color_Create(195, 135, 255, 255);

    DrawDimmer(windowW, windowH, 190);
    DrawPopupPanel(panelX, panelY, panelW, panelH);

    // title and tagline
    CP_Settings_TextAlignment(CP_TEXT_ALIGN_H_CENTER, CP_TEXT_ALIGN_V_MIDDLE);
    UseTitleFont();
    CP_Settings_Fill(CP_Color_Create(245, 245, 245, 255));
    CP_Settings_TextSize(26.0f * scale);
    CP_Font_DrawText("HOW TO PLAY", panelX, panelTop + 45.0f * scale);

    UseBodyFont();
    CP_Settings_TextSize(26.0f * scale);
    CP_Settings_Fill(muted);
    CP_Font_DrawText("You are the pin. The giant bowling ball is right behind you!", panelX, panelTop + 87.0f * scale);

    // dividers
    CP_Settings_Fill(CP_Color_Create(225, 75, 95, 110));
    CP_Graphics_DrawRect(panelX, panelTop + 115.0f * scale, panelW - 70.0f * scale, 2.0f * scale);
    CP_Graphics_DrawRect(panelX, panelTop + 303.0f * scale, 2.0f * scale, 335.0f * scale);

    CP_Settings_TextAlignment(CP_TEXT_ALIGN_H_LEFT, CP_TEXT_ALIGN_V_MIDDLE);

    // left column
    DrawHowToHeading("YOUR GOAL", leftX, panelTop + 148.0f * scale, scale, pink);
    DrawHowToLine("Survive as long as possible.", leftX, panelTop + 183.0f * scale, scale);
    DrawHowToLine("Multiplier balls boost your final score.", leftX, panelTop + 213.0f * scale, scale);
    DrawHowToLine("The game gets faster over time!", leftX, panelTop + 243.0f * scale, scale);

    DrawHowToHeading("CONTROLS", leftX, panelTop + 290.0f * scale, scale, pink);
    DrawHowToLine("W / S  -  Move up / down one lane", leftX, panelTop + 325.0f * scale, scale);
    DrawHowToLine("ESC / SPACE  -  Pause / Resume", leftX, panelTop + 355.0f * scale, scale);

    DrawHowToHeading("HAZARDS & HEARTS", leftX, panelTop + 402.0f * scale, scale, pink);
    DrawHowToLine("Avoid barriers, oil slicks, and holes.", leftX, panelTop + 437.0f * scale, scale);
    DrawHowToLine("3 hearts. Each unprotected hit costs 1.", leftX, panelTop + 467.0f * scale, scale);
    DrawHowToLine("Lose all 3 hearts and the ball gets you.", leftX, panelTop + 497.0f * scale, scale);

    // right column
    DrawHowToHeading("MYSTERY BOXES", rightX, panelTop + 148.0f * scale, scale, pink);
    DrawHowToLine("Collect a box and wait for the roll.", rightX, panelTop + 183.0f * scale, scale);
    DrawHowToLine("Receive one random power-up:", rightX, panelTop + 213.0f * scale, scale);

    DrawHowToHeading("INVINCIBILITY", rightX, panelTop + 260.0f * scale, scale, gold);
    DrawHowToLine("Smash through barriers and oil.", rightX, panelTop + 293.0f * scale, scale);

    DrawHowToHeading("SHIELD", rightX, panelTop + 340.0f * scale, scale, blue);
    DrawHowToLine("Blocks one barrier or oil hit.", rightX, panelTop + 373.0f * scale, scale);

    DrawHowToHeading("MAGNET", rightX, panelTop + 420.0f * scale, scale, purple);
    DrawHowToLine("Pulls nearby coins towards you.", rightX, panelTop + 453.0f * scale, scale);

    CP_Settings_TextSize(25.0f * scale);
    CP_Settings_Fill(gold);
    CP_Font_DrawText("Holes bypass shields & invincibility!", rightX, panelTop + 497.0f * scale);

    // footer
    CP_Settings_TextAlignment(CP_TEXT_ALIGN_H_CENTER, CP_TEXT_ALIGN_V_MIDDLE);
    CP_Settings_TextSize(23.0f * scale);
    CP_Settings_Fill(muted);
    CP_Font_DrawText("Grab floating hearts to heal. Stay alert when words fly across the screen!",
        panelX, panelTop + 545.0f * scale);

    CP_Settings_TextSize(24.0f * scale);
    CP_Settings_Fill(pink);
    CP_Font_DrawText("[ Click anywhere or press ESC to return ]", panelX, panelTop + 580.0f * scale);

    if (CP_Input_MouseReleased(MOUSE_BUTTON_LEFT) || CP_Input_KeyTriggered(KEY_ESCAPE))
    {
        UI_PlayClick();
        popup = MENU_POPUP_NONE;
    }
}

/* ============================================================
   Credits screen
   ============================================================ */

/*! Specification for function DrawCreditsList:
@brief Draws a heading followed by a list of names on the credits screen.

@param heading - The heading text (e.g. "Instructors").
@param names - The names to list under the heading.
@param count - The number of names.
@param x - The horizontal centre of the list.
@param y - The vertical position to start at; moved down past the list when done.
@param scale - The UI scale.
@return none.
*//*________________________________________________________________________________*/
static void DrawCreditsList(const char* heading, const char* const* names, int count,
    float x, float* y, float scale)
{
    CP_Settings_Fill(CP_Color_Create(255, 125, 145, 255));
    CP_Settings_TextSize(30.0f * scale);
    CP_Font_DrawText(heading, x, *y);
    *y += 35.0f * scale;

    CP_Settings_Fill(CP_Color_Create(245, 245, 245, 255));
    CP_Settings_TextSize(26.0f * scale);

    for (int i = 0; i < count; i++)
    {
        CP_Font_DrawText(names[i], x, *y);
        *y += 29.0f * scale;
    }

    *y += 18.0f * scale;
}

/*! Specification for function UpdateCreditsScreen:
@brief Draws the full-screen credits and handles the Back button.

Shows the DigiPen Singapore logo and copyright at the top, then the team members,
instructors, president, executives and font licences. Back or Escape returns to the menu.

@param windowW - The window width.
@param windowH - The window height.
@return none.
*//*________________________________________________________________________________*/
static void UpdateCreditsScreen(float windowW, float windowH)
{
    static const char* const teamMembers[] =
    {
        "Ggwendolynn Lee Rasni (IMGD)",
        "Isaac Tan (IMGD)",
        "Ang Xiao Hui (IMGD)",
        "Lek Kar Lim (RTIS)"
    };

    static const char* const instructors[] =
    {
        "Gerald WONG Han Feng",
        "TAN Chee Wei Tommy",
        "Soroor MALEKMOHAMMADI FARADOUNBEH"
    };

    static const char* const president[] =
    {
        "Claude Comair"
    };

    static const char* const executives[] =
    {
        "CHU Jason Yeu Tat",
        "Michael GATS",
        "TAN Chek Ming",
        "Prasanna Kumar GHALI",
        "Mandy WONG",
        "Johnny DEEK"
    };

    float scale = UI_GetScale();
    float centerX = windowW * 0.5f;
    float top = (windowH - 720.0f * scale) * 0.5f;
    float leftX = centerX - 260.0f * scale;
    float rightX = centerX + 260.0f * scale;
    float leftY = top + 265.0f * scale;
    float rightY = top + 265.0f * scale;

    CP_Graphics_ClearBackground(CP_Color_Create(20, 20, 25, 255));
    CP_Settings_NoStroke();
    CP_Settings_TextAlignment(CP_TEXT_ALIGN_H_CENTER, CP_TEXT_ALIGN_V_MIDDLE);
    UseBodyFont();

    // DigiPen logo and copyright first, as required.
    if (creditsLogo != NULL)
    {
        float logoW = (float)CP_Image_GetWidth(creditsLogo);
        float logoH = (float)CP_Image_GetHeight(creditsLogo);
        float fit = fminf(360.0f * scale / logoW, 100.0f * scale / logoH);

        CP_Settings_ImageMode(CP_POSITION_CENTER);
        CP_Image_Draw(creditsLogo, centerX, top + 78.0f * scale, logoW * fit, logoH * fit, 255);
    }

    CP_Settings_Fill(CP_Color_Create(245, 245, 245, 255));
    CP_Settings_TextSize(23.0f * scale);
    CP_Font_DrawText("All content \xC2\xA9 2026 DigiPen Institute of Technology Singapore. All Rights Reserved",
        centerX, top + 151.0f * scale);

    CP_Settings_Fill(CP_Color_Create(255, 125, 145, 255));
    CP_Settings_TextSize(42.0f * scale);
    CP_Font_DrawText("CREDITS", centerX, top + 202.0f * scale);

    DrawCreditsList("Team Members", teamMembers, 4, leftX, &leftY, scale);
    DrawCreditsList("Instructors", instructors, 3, leftX, &leftY, scale);
    DrawCreditsList("President", president, 1, rightX, &rightY, scale);
    DrawCreditsList("Executives", executives, 6, rightX, &rightY, scale);

    // third-party content
    CP_Settings_Fill(CP_Color_Create(195, 195, 205, 255));
    CP_Settings_TextSize(20.0f * scale);
    CP_Font_DrawText("Fonts: Press Start 2P (CodeMan38) and VT323 (Peter Hull), SIL Open Font License",
        centerX, top + 612.0f * scale);

    Button backButton = { centerX, top + 659.0f * scale, 200.0f * scale, 52.0f * scale, "BACK (ESC)" };

    if (MenuButton(&backButton) ||
        CP_Input_KeyTriggered(KEY_ESCAPE))
    {
        if (CP_Input_KeyTriggered(KEY_ESCAPE))
        {
            UI_PlayClick();
        }

        popup = MENU_POPUP_NONE;
    }
}

/* ============================================================
   Background chase and title
   ============================================================ */

/*! Specification for function AdvanceFrame:
@brief Advances a looping sprite-sheet frame counter.

@param dt - The time passed since the previous frame, in seconds.
@param frameTime - Seconds each frame is shown.
@param frameCount - The number of frames in the sheet.
@param timer - Time spent on the current frame (updated).
@param frame - The current frame index (updated).
@return none.
*//*________________________________________________________________________________*/
static void AdvanceFrame(float dt, float frameTime, int frameCount, float* timer, int* frame)
{
    *timer += dt;

    while (*timer >= frameTime)
    {
        *timer -= frameTime;
        *frame = (*frame + 1) % frameCount;
    }
}

/*! Specification for function DrawMenuChase:
@brief Updates and draws the menu's bowling pin and ball animation.

Moves both characters across the screen and updates their animation
frames. Restarts the chase after both characters leave the screen.

@param dt - The time passed since the previous frame, in seconds.
@param windowW - The window width.
@param windowH - The window height.
@return none.
*//*________________________________________________________________________________*/
static void DrawMenuChase(float dt, float windowW, float windowH)
{
    float menuScale = windowH / UI_DESIGN_HEIGHT;
    float chaseGap = MENU_CHASE_GAP * menuScale;
    float groundY = windowH * 0.90f;
    float pinX;
    float ballX;

    AdvanceFrame(dt, MENU_PIN_FRAME_TIME, MENU_PIN_FRAME_COUNT, &menuPinFrameTimer, &menuPinFrame);
    AdvanceFrame(dt, MENU_BALL_FRAME_TIME, MENU_BALL_FRAME_COUNT, &menuBallFrameTimer, &menuBallFrame);

    menuChaseX += MENU_CHASE_SPEED * menuScale * dt;
    pinX = menuChaseX;
    ballX = menuChaseX - chaseGap;

    if (ballX - MENU_BALL_SIZE / 2.0f > windowW)
    {
        menuChaseX = -MENU_PIN_SIZE / 2.0f;
        pinX = menuChaseX;
        ballX = menuChaseX - chaseGap;
    }

    CP_Settings_ImageMode(CP_POSITION_CENTER);

    if (menuBall != NULL)
    {
        float sourceLeft = (float)menuBallFrame * MENU_BALL_FRAME_W;

        CP_Image_DrawSubImage(menuBall, ballX, groundY - MENU_BALL_SIZE / 2.0f,
            MENU_BALL_SIZE, MENU_BALL_SIZE,
            sourceLeft, 0.0f, sourceLeft + MENU_BALL_FRAME_W, MENU_BALL_FRAME_H, 255);
    }

    if (menuPin != NULL)
    {
        float sourceLeft = (float)menuPinFrame * MENU_PIN_FRAME_W;

        CP_Image_DrawSubImage(menuPin, pinX, groundY - MENU_PIN_SIZE / 2.0f,
            MENU_PIN_SIZE, MENU_PIN_SIZE,
            sourceLeft, 0.0f, sourceLeft + MENU_PIN_FRAME_W, MENU_PIN_FRAME_H, 255);
    }
}

/*! Specification for function DrawTitle:
@brief Draws the bobbing "ALLEY RUNNER" title and its tagline.

Both are drawn with a drop shadow and an outline so they stay readable on the background.

@param centerX - The horizontal centre of the window.
@param windowH - The window height.
@return none.
*//*________________________________________________________________________________*/
static void DrawTitle(float centerX, float windowH)
{
    float headingScale = UI_GetScale() * 0.65f;
    float titleSize = 90.0f * headingScale;
    float subtitleSize = 40.0f * headingScale;
    float titleY = windowH * 0.20f + sinf(floatTimer) * 8.0f * headingScale;
    float taglineY = titleY + 80.0f * headingScale;
    CP_Color white = CP_Color_Create(255, 255, 255, 255);
    CP_Color outline = CP_Color_Create(15, 15, 15, 255);
    CP_Color shadow = CP_Color_Create(0, 0, 0, 210);

    CP_Settings_NoStroke();
    CP_Settings_TextAlignment(CP_TEXT_ALIGN_H_CENTER, CP_TEXT_ALIGN_V_MIDDLE);

    UseTitleFont();
    UI_DrawTextWithOutline("ALLEY RUNNER", centerX + 5.0f * headingScale, titleY + 7.0f * headingScale,
        titleSize, shadow, shadow, 4.0f * headingScale);
    UI_DrawTextWithOutline("ALLEY RUNNER", centerX, titleY, titleSize, white, outline, 4.0f * headingScale);

    UseBodyFont();
    CP_Settings_TextSize(subtitleSize);
    CP_Settings_Fill(CP_Color_Create(0, 0, 0, 230));
    CP_Font_DrawText("Keep running. Don't get bowled over!",
        centerX + 2.0f * headingScale, taglineY + 3.0f * headingScale);
    UI_DrawTextWithOutline("Keep running. Don't get bowled over!", centerX, taglineY,
        subtitleSize, white, outline, 1.0f * headingScale);
}

/*! Specification for function DrawFooter:
@brief Draws the high score (bottom left) and the team logo (bottom right).

@param windowW - The window width.
@param windowH - The window height.
@return none.
*//*________________________________________________________________________________*/
static void DrawFooter(float windowW, float windowH)
{
    float scale = UI_GetScale();
    float footerSize = 36.0f * scale;
    float margin = 25.0f * scale;
    float footerY = windowH - 20.0f * scale;
    char highScoreText[64];

    snprintf(highScoreText, sizeof(highScoreText), "HIGH SCORE: %d", Game_GetHighScore());

    UseBodyFont();
    CP_Settings_NoStroke();
    CP_Settings_TextAlignment(CP_TEXT_ALIGN_H_LEFT, CP_TEXT_ALIGN_V_BOTTOM);
    CP_Settings_TextSize(footerSize);
    CP_Settings_Fill(CP_Color_Create(0, 0, 0, 230));
    CP_Font_DrawText(highScoreText, margin + 2.0f * scale, footerY + 2.0f * scale);
    UI_DrawTextWithOutline(highScoreText, margin, footerY, footerSize,
        CP_Color_Create(255, 255, 255, 255), CP_Color_Create(15, 15, 15, 255), 1.0f * scale);

    if (teamLogo != NULL)
    {
        float imageW = (float)CP_Image_GetWidth(teamLogo);
        float imageH = (float)CP_Image_GetHeight(teamLogo);

        if (imageW > 0.0f && imageH > 0.0f)
        {
            // fit inside 280 x 90, keeping the image's proportions.
            float fit = fminf(280.0f * scale / imageW, 90.0f * scale / imageH);
            float logoW = imageW * fit;
            float logoH = imageH * fit;

            CP_Settings_ImageMode(CP_POSITION_CENTER);
            CP_Image_Draw(teamLogo, windowW - margin - logoW * 0.5f, footerY - logoH * 0.5f, logoW, logoH, 255);
        }
    }
}

/* ============================================================
   State functions
   ============================================================ */

/*! Specification for function MainMenu_Init:
@brief Prepares the main menu when it is entered.

Loads the music and images, starts the menu music, sets up the buttons,
and resets the pop-ups and the background chase animation.

@param none.
@return none.
*//*________________________________________________________________________________*/
void MainMenu_Init(void)
{
    CP_System_SetWindowTitle("Alley Runner");
    CP_System_ShowCursor(true);
    UI_Load();

    popup = MENU_POPUP_NONE;
    quitting = false;
    quitTimer = 0.0f;
    activeVolumeSlider = 0;
    floatTimer = 0.0f;

    if (!displaySettingsInitialized)
    {
        windowedWidth = CP_System_GetWindowWidth();
        windowedHeight = CP_System_GetWindowHeight();
        displayMode = DISPLAY_WINDOWED;
        displaySettingsInitialized = true;
    }

    if (!audioSettingsInitialized)
    {
        musicVolume = DEFAULT_VOLUME;
        sfxVolume = DEFAULT_VOLUME;
        audioSettingsInitialized = true;
    }

    ApplyAudioVolumes();

    menuMusic = CP_Sound_Load("Assets/main_menu_bg_music.mp3");

    if (menuMusic != NULL)
    {
        CP_Sound_PlayAdvanced(menuMusic, 0.2f, 1.0f, true, CP_SOUND_GROUP_MUSIC);
    }

    creditsLogo = CP_Image_Load("Assets/DigiPen_Singapore_WEB_WHITE.png");
    teamLogo = CP_Image_Load("Assets/team_logo.png");
    bgImage = CP_Image_Load("Assets/mainmenu_background.png");
    menuPin = CP_Image_Load(MENU_PIN_PATH);
    menuBall = CP_Image_Load(MENU_BALL_PATH);

    UpdateMenuButtonLayout((float)CP_System_GetWindowWidth(), (float)CP_System_GetWindowHeight());

    // restart the background chase from the left edge.
    menuChaseX = -(MENU_PIN_SIZE * ((float)CP_System_GetWindowHeight() / UI_DESIGN_HEIGHT)) / 2.0f;
    menuPinFrame = 0;
    menuBallFrame = 0;
    menuPinFrameTimer = 0.0f;
    menuBallFrameTimer = 0.0f;
}

/*! Specification for function MainMenu_Update:
@brief Updates and draws the main menu each frame.

Handles the buttons, pop-ups and keyboard shortcuts (Escape opens the quit
confirmation), and closes the program once quitting has finished.

@param none.
@return none.
*//*________________________________________________________________________________*/
void MainMenu_Update(void)
{
    float dt = CP_System_GetDt();
    float windowW = (float)CP_System_GetWindowWidth();
    float windowH = (float)CP_System_GetWindowHeight();
    float centerX = windowW / 2.0f;
    float centerY = windowH / 2.0f;

    CP_Settings_ResetMatrix();
    CP_Settings_ImageMode(CP_POSITION_CENTER);

    // after "Yes" on quit: wait for the click sound, then close the program.
    if (quitting)
    {
        quitTimer -= dt;

        if (quitTimer <= 0.0f)
        {
            UI_Unload();   // shared fonts and click sound are freed once, here
            CP_Engine_Terminate();
            return;
        }
    }

    floatTimer += dt * 2.5f;

    if (popup == MENU_POPUP_CREDITS)
    {
        UpdateCreditsScreen(windowW, windowH);
        return;
    }

    UpdateMenuButtonLayout(windowW, windowH);

    CP_Graphics_ClearBackground(CP_Color_Create(35, 25, 25, 255));

    if (bgImage != NULL)
    {
        CP_Image_Draw(bgImage, centerX, centerY, windowW, windowH, 255);
    }

    DrawMenuChase(dt, windowW, windowH);
    DrawTitle(centerX, windowH);

    switch (popup)
    {
    case MENU_POPUP_NONE:
    {
        // draw every button first so none of them disappears on the frame of a click.
        bool playClicked = MenuButton(&btnPlay);
        bool howToClicked = MenuButton(&btnHowTo);
        bool settingsClicked = MenuButton(&btnSettings);
        bool creditsClicked = MenuButton(&btnCredits);
        bool quitClicked = MenuButton(&btnQuit);

        if (playClicked)
        {
            Transition_GoTo(Game_Init, Game_Update, Game_Exit);
        }
        else if (howToClicked)
        {
            popup = MENU_POPUP_HOW_TO_PLAY;
        }
        else if (settingsClicked)
        {
            popup = MENU_POPUP_SETTINGS;
            activeVolumeSlider = 0;
        }
        else if (creditsClicked)
        {
            popup = MENU_POPUP_CREDITS;
        }
        else if (quitClicked)
        {
            popup = MENU_POPUP_QUIT_CONFIRM;
        }
        else if (!MenuIsBusy() && CP_Input_KeyTriggered(KEY_ESCAPE))
        {
            UI_PlayClick();
            popup = MENU_POPUP_QUIT_CONFIRM;
        }
        break;
    }

    case MENU_POPUP_HOW_TO_PLAY:
        UpdateHowToPopup(windowW, windowH);
        break;

    case MENU_POPUP_SETTINGS:
        UpdateSettingsPopup(centerX, centerY, windowW, windowH);
        break;

    case MENU_POPUP_QUIT_CONFIRM:
        UpdateQuitPopup(centerX, centerY, windowW, windowH);
        break;

    default:
        break;
    }

    DrawFooter(windowW, windowH);
}

/*! Specification for function MainMenu_Exit:
@brief Cleans up the main menu when it is left.

Stops the music and frees every sound and image loaded by MainMenu_Init.

@param none.
@return none.
*//*________________________________________________________________________________*/
void MainMenu_Exit(void)
{
    CP_Sound_StopGroup(CP_SOUND_GROUP_MUSIC);

    if (menuMusic != NULL)
    {
        CP_Sound_Free(menuMusic);
        menuMusic = NULL;
    }

    if (bgImage != NULL)
    {
        CP_Image_Free(bgImage);
        bgImage = NULL;
    }

    if (menuPin != NULL)
    {
        CP_Image_Free(menuPin);
        menuPin = NULL;
    }

    if (menuBall != NULL)
    {
        CP_Image_Free(menuBall);
        menuBall = NULL;
    }

    if (creditsLogo != NULL)
    {
        CP_Image_Free(creditsLogo);
        creditsLogo = NULL;
    }

    if (teamLogo != NULL)
    {
        CP_Image_Free(teamLogo);
        teamLogo = NULL;
    }

    activeVolumeSlider = 0;
}