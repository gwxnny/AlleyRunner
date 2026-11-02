/*!
@file       : mainmenu.c
@team       : IsaacACat
@module     : CSD1401 Software Engineering Project 1
@section    : section b
@project    : Team Project 1 - Alley Runner
@brief      : This file contains the implementation of the main menu state for the Alley Runner.
*//*________________________________________________________________________________*/
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>

#include "mainmenu.h"
#include "game.h"
#include "utils.h"

#include <stdio.h>
#include <stdbool.h>
#include <math.h>

CP_Font Font;

// sounds
static CP_Sound menuMusic = NULL;
static CP_Sound buttonSound = NULL;

// prevent the sound from being cut off by a state change.
#define BUTTON_SOUND_DELAY 0.25f

static float buttonActionTimer = 0.0f;
static int pendingButtonAction = 0; // 0 = none, 1 = play, 2 = exit

// audio settings are initialized once when the main menu is first loaded.
static bool audioSettingsInitialized = false;
static float musicVolume = 0.2f;
static float sfxVolume = 1.0f;

// 0 = not dragging, 1 = music, 2 = SFX.
static int activeVolumeSlider = 0;

// button ui component.
typedef struct {
    float x;
    float y;
    float width;
    float height;
    const char* label;
    bool isHovered;
} Button;

// font and backgroundimage resources.
static CP_Font fontTitle = NULL;
static CP_Font fontBody = NULL;
static CP_Image bgImage = NULL;

// background animation resources.
static CP_Image menuPin = NULL;
static CP_Image menuBall = NULL;

static float menuChaseX = 0.0f;
static float menuChaseTimer = 0.0f;

// frame animation timers and frame indices for the pin and ball.
static float menuPinFrameTimer = 0.0f;
static float menuBallFrameTimer = 0.0f;

static int menuPinFrame = 0;
static int menuBallFrame = 0;

#define MENU_CHASE_SPEED 160.0f
#define MENU_CHASE_GAP 500.0f // original: 230

// sprite sheet frame sizes and counts for the pin and ball.
// pin sheet: 480 x 120, containing 4 frames.
#define MENU_PIN_FRAME_COUNT 4
#define MENU_PIN_FRAME_W 120.0f
#define MENU_PIN_FRAME_H 120.0f
// ball sheet: 2000 x 250, containing 8 frames.
#define MENU_BALL_FRAME_COUNT 8
#define MENU_BALL_FRAME_W 250.0f
#define MENU_BALL_FRAME_H 250.0f

// smaller frame times make the animation faster. Adjust these values to change the speed of the pin and ball animations.
#define MENU_PIN_FRAME_TIME 0.10f
#define MENU_BALL_FRAME_TIME 0.08f

// display frame size
#define MENU_PIN_SIZE 240.0f // original: 120
#define MENU_BALL_SIZE 750.0f // original: 250

// menu states.
static GameState currentRequestedState = STATE_MAIN_MENU;

static Button btnPlay;
static Button btnHowTo;
static Button btnOptions;
static Button btnExit;

static bool showHowToModal = false;
static bool showOptionsModal = false;

static float floatTimer = 0.0f;

/* // ☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆ //

    Button helpers

*/ // ☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆ //

static bool IsMouseOverButton(
    Button btn,
    float mouseX,
    float mouseY)
{
    float halfWidth = btn.width / 2.0f;
    float halfHeight = btn.height / 2.0f;

    return
        mouseX >= btn.x - halfWidth &&
        mouseX <= btn.x + halfWidth &&
        mouseY >= btn.y - halfHeight &&
        mouseY <= btn.y + halfHeight;
}

static void PlayMenuClick(void)
{
    if (buttonSound != NULL) {
        CP_Sound_Play(buttonSound);
    }
}

static void DrawPillButton(Button btn)
{
    CP_Settings_RectMode(CP_POSITION_CENTER);
    CP_Settings_Stroke(CP_Color_Create(20, 20, 20, 255));
    CP_Settings_StrokeWeight(5.0f);

    if (btn.isHovered) {
        CP_Settings_Fill(CP_Color_Create(225, 75, 95, 255));
    }
    else {
        CP_Settings_Fill(CP_Color_Create(195, 65, 80, 255));
    }

    CP_Graphics_DrawRectAdvanced(
        btn.x,
        btn.y,
        btn.width,
        btn.height,
        0.0f,
        btn.height / 2.0f
    );
    // button highlights.
    CP_Settings_NoStroke();
    CP_Settings_Fill(CP_Color_Create(255, 255, 255, 45));

    CP_Graphics_DrawRectAdvanced(
        btn.x,
        btn.y - btn.height * 0.18f,
        btn.width * 0.85f,
        btn.height * 0.35f,
        0.0f,
        btn.height * 0.18f
    );

	// button label.
    CP_Settings_TextAlignment(
        CP_TEXT_ALIGN_H_CENTER,
        CP_TEXT_ALIGN_V_MIDDLE
    );

    if (fontTitle != NULL) {
        CP_Font_Set(fontTitle);
        CP_Settings_TextSize(20.0f);
    }
    else {
        CP_Settings_TextSize(22.0f);
    }

    if (btn.isHovered) {
        CP_Settings_Fill(CP_Color_Create(255, 255, 255, 255));
    }
    else {
        CP_Settings_Fill(CP_Color_Create(245, 245, 245, 255));
    }

    CP_Font_DrawText(btn.label, btn.x, btn.y);
}


/* // ☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆ //

    Text helpers

*/ // ☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆ //

static void DrawTextWithOutline(
    const char* text,
    float x,
    float y,
    float size,
    CP_Color fillColor,
    CP_Color strokeColor,
    float outlineWidth)
{
    CP_Settings_TextSize(size);
    CP_Settings_Fill(strokeColor);

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

/* // ☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆ //

    Layout

*/ // ☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆ //

static void UpdateMenuButtonLayout(float windowW, float windowH)
{
    float centerX = windowW / 2.0f;
    float startY = windowH * 0.48f;

	// change values to resize the menu buttons.
    float btnWidth = 270.0f;
    float btnHeight = 58.0f;
    float spacing = 72.0f;

    btnPlay = (Button){
        centerX,
        startY,
        btnWidth,
        btnHeight,
        "PLAY",
        false
    };

    btnHowTo = (Button){
        centerX,
        startY + spacing,
        btnWidth,
        btnHeight,
        "HOW TO PLAY",
        false
    };

    btnOptions = (Button){
        centerX,
        startY + spacing * 2.0f,
        btnWidth,
        btnHeight,
        "OPTIONS",
        false
    };

    btnExit = (Button){
        centerX,
        startY + spacing * 3.0f,
        btnWidth,
        btnHeight,
        "EXIT",
        false
    };
}


/* // ☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆ //

	Audio options

*/ // ☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆ //

static void ApplyAudioVolumes(void)
{
    CP_Sound_SetGroupVolume(
        CP_SOUND_GROUP_MUSIC,
        musicVolume
    );

    CP_Sound_SetGroupVolume(
        CP_SOUND_GROUP_SFX,
        sfxVolume
    );
}

static void DrawVolumeSlider(
    const char* label,
    float left,
    float y,
    float width,
    float value)
{
    char text[80];

    snprintf(
        text,
        sizeof(text),
        "%s: %d%%",
        label,
        (int)(value * 100.0f + 0.5f)
    );

	// slider label and percentage.
    CP_Settings_NoStroke();
    CP_Settings_TextAlignment(
        CP_TEXT_ALIGN_H_LEFT,
        CP_TEXT_ALIGN_V_MIDDLE
    );
    CP_Settings_TextSize(28.0f);
    CP_Settings_Fill(CP_Color_Create(255, 255, 255, 255));
    CP_Font_DrawText(text, left, y - 32.0f);
    CP_Settings_RectMode(CP_POSITION_CENTER);
    CP_Settings_Fill(CP_Color_Create(75, 75, 85, 255));
    CP_Graphics_DrawRectAdvanced(
        left + width / 2.0f,
        y,
        width,
        12.0f,
        0.0f,
        6.0f
    );

	// filled portion of the slider.
    if (value > 0.0f) {
        CP_Settings_Fill(CP_Color_Create(225, 75, 95, 255));

        CP_Graphics_DrawRect(
            left + width * value / 2.0f,
            y,
            width * value,
            12.0f
        );
    }

	// slider knob.
    CP_Settings_Fill(CP_Color_Create(255, 255, 255, 255));

    CP_Graphics_DrawRectAdvanced(
        left + width * value,
        y,
        18.0f,
        30.0f,
        0.0f,
        7.0f
    );
}

/* Display settings persist between menu visits in the same run. */
typedef enum
{
    DISPLAY_WINDOWED,
    DISPLAY_BORDERLESS,
    DISPLAY_FULLSCREEN
} DisplayMode;

static DisplayMode displayMode = DISPLAY_WINDOWED;

/* Size used when returning to bordered windowed mode. */
static int windowedWidth = 1280;
static int windowedHeight = 720;

static bool displaySettingsInitialized = false;

static const char* GetDisplayModeLabel(void)
{
    switch (displayMode)
    {
    case DISPLAY_BORDERLESS:
        return "BORDERLESS WINDOWED";

    case DISPLAY_FULLSCREEN:
        return "FULLSCREEN";

    default:
        return "WINDOWED";
    }
}

static void ApplyDisplayMode(DisplayMode nextMode)
{
    HWND window = CP_System_GetWindowHandle();

    if (window == NULL || nextMode == displayMode)
    {
        return;
    }

    /* Remember the current windowed canvas size. */
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

	// request windowed mode to ensure the window is not in fullscreen before changing the native frame.
    CP_System_SetWindowSize(windowedWidth, windowedHeight);

	// retrieve the native window handle and monitor information to adjust the window style and position for borderless or windowed mode.
    window = CP_System_GetWindowHandle();

    if (window == NULL)
    {
        return;
    }

    MONITORINFO monitorInfo = { 0 };
    monitorInfo.cbSize = sizeof(monitorInfo);

    HMONITOR monitor = MonitorFromWindow(
        window,
        MONITOR_DEFAULTTONEAREST
    );

    if (!GetMonitorInfo(monitor, &monitorInfo))
    {
        return;
    }

    LONG_PTR style = GetWindowLongPtr(window, GWL_STYLE);

    if (nextMode == DISPLAY_BORDERLESS)
    {
        int monitorW =
            monitorInfo.rcMonitor.right -
            monitorInfo.rcMonitor.left;

        int monitorH =
            monitorInfo.rcMonitor.bottom -
            monitorInfo.rcMonitor.top;

        CP_System_SetWindowSize(monitorW, monitorH);

        window = CP_System_GetWindowHandle();

        if (window == NULL)
        {
            return;
        }

        style = GetWindowLongPtr(window, GWL_STYLE);

        style &= ~((LONG_PTR)WS_OVERLAPPEDWINDOW);
        style |= WS_POPUP;

        SetWindowLongPtr(window, GWL_STYLE, style);

        SetWindowPos(
            window,
            HWND_NOTOPMOST,
            monitorInfo.rcMonitor.left,
            monitorInfo.rcMonitor.top,
            monitorW,
            monitorH,
            SWP_FRAMECHANGED | SWP_SHOWWINDOW
        );
    }
    else
    {
        style &= ~((LONG_PTR)WS_POPUP);
        style |= WS_OVERLAPPEDWINDOW;

        SetWindowLongPtr(window, GWL_STYLE, style);

		// calculate the outer window size needed for the requested client area.
        RECT rect = {
            0,
            0,
            windowedWidth,
            windowedHeight
        };

        DWORD exStyle = (DWORD)GetWindowLongPtr(
            window,
            GWL_EXSTYLE
        );

        AdjustWindowRectEx(
            &rect,
            (DWORD)style,
            GetMenu(window) != NULL,
            exStyle
        );

        int outerW = rect.right - rect.left;
        int outerH = rect.bottom - rect.top;

        int workW =
            monitorInfo.rcWork.right -
            monitorInfo.rcWork.left;

        int workH =
            monitorInfo.rcWork.bottom -
            monitorInfo.rcWork.top;

        int x = monitorInfo.rcWork.left;
        int y = monitorInfo.rcWork.top;

        if (workW > outerW)
        {
            x += (workW - outerW) / 2;
        }

        if (workH > outerH)
        {
            y += (workH - outerH) / 2;
        }

        SetWindowPos(
            window,
            HWND_NOTOPMOST,
            x,
            y,
            outerW,
            outerH,
            SWP_FRAMECHANGED | SWP_SHOWWINDOW
        );
    }

    displayMode = nextMode;
}

static void DrawOptionsModal(
    float centerX,
    float centerY,
    float windowW,
    float windowH)
{
    float mouseX = CP_Input_GetMouseX();
    float mouseY = CP_Input_GetMouseY();

    float sliderWidth = 400.0f;
    float sliderLeft = centerX - sliderWidth / 2.0f;

    float musicY = centerY - 10.0f;
    float sfxY = centerY + 85.0f;

    Button backButton = {
        centerX,
        centerY + 190.0f,
        230.0f,
        52.0f,
        "BACK",
        false
    };

    backButton.isHovered =
        IsMouseOverButton(backButton, mouseX, mouseY);

	// handle mouse input for the volume sliders.
    if (CP_Input_MouseTriggered(MOUSE_BUTTON_LEFT)) {
        bool insideSliderWidth =
            mouseX >= sliderLeft - 12.0f &&
            mouseX <= sliderLeft + sliderWidth + 12.0f;

        if (insideSliderWidth &&
            fabsf(mouseY - musicY) <= 22.0f) {
            activeVolumeSlider = 1;
        }
        else if (
            insideSliderWidth &&
            fabsf(mouseY - sfxY) <= 22.0f) {
            activeVolumeSlider = 2;
        }
    }

	// update slider values if the mouse is being dragged.
    if (CP_Input_MouseDown(MOUSE_BUTTON_LEFT)) {
        if (activeVolumeSlider != 0) {
            float value = ClampFloat(
                (mouseX - sliderLeft) / sliderWidth,
                0.0f,
                1.0f
            );

            if (activeVolumeSlider == 1) {
                musicVolume = value;
            }
            else {
                sfxVolume = value;
            }

            ApplyAudioVolumes();
        }
    }
    else if (activeVolumeSlider != 0) {
		// mouse button released, play a click sound if the user was dragging a slider.
        if (activeVolumeSlider == 2) {
            PlayMenuClick();
        }

        activeVolumeSlider = 0;
    }

	// dim screen behind the popup.
    CP_Settings_RectMode(CP_POSITION_CENTER);
    CP_Settings_NoStroke();
    CP_Settings_Fill(CP_Color_Create(0, 0, 0, 150));

    CP_Graphics_DrawRect(
        centerX,
        centerY,
        windowW,
        windowH
    );

	// popup panel.
    CP_Settings_Fill(CP_Color_Create(20, 20, 25, 250));
    CP_Settings_Stroke(CP_Color_Create(225, 75, 95, 255));
    CP_Settings_StrokeWeight(4.0f);

    CP_Graphics_DrawRectAdvanced(
        centerX,
        centerY,
        580.0f,
        520.0f,
        0.0f,
        15.0f
    );

	// header text.
    CP_Settings_NoStroke();
    CP_Settings_TextAlignment(
        CP_TEXT_ALIGN_H_CENTER,
        CP_TEXT_ALIGN_V_MIDDLE
    );

    if (fontTitle != NULL) {
        CP_Font_Set(fontTitle);
    }

    CP_Settings_TextSize(22.0f);
    CP_Settings_Fill(CP_Color_Create(255, 255, 255, 255));

    CP_Font_DrawText(
        "OPTIONS",
        centerX,
        centerY - 210.0f
    );

	// display mode button.
    if (fontBody != NULL)
    {
        CP_Font_Set(fontBody);
    }

    CP_Settings_TextAlignment(
        CP_TEXT_ALIGN_H_CENTER,
        CP_TEXT_ALIGN_V_MIDDLE
    );

    CP_Settings_TextSize(26.0f);
    CP_Settings_Fill(CP_Color_Create(255, 255, 255, 255));

    CP_Font_DrawText(
        "Display mode - click to change",
        centerX,
        centerY - 165.0f
    );

    Button displayButton = {
        centerX,
        centerY - 120.0f,
        460.0f,
        52.0f,
        GetDisplayModeLabel(),
        false
    };

    displayButton.isHovered =
        IsMouseOverButton(displayButton, mouseX, mouseY);

    DrawPillButton(displayButton);

    if (CP_Input_MouseTriggered(MOUSE_BUTTON_LEFT) &&
        displayButton.isHovered)
    {
        PlayMenuClick();

        DisplayMode nextMode =
            (DisplayMode)(((int)displayMode + 1) % 3);

        activeVolumeSlider = 0;
        ApplyDisplayMode(nextMode);

		// stop drawing the options modal and return to the main menu.
        return;
    }

    // sliders.
    if (fontBody != NULL) {
        CP_Font_Set(fontBody);
    }

    DrawVolumeSlider(
        "Music",
        sliderLeft,
        musicY,
        sliderWidth,
        musicVolume
    );

    DrawVolumeSlider(
        "SFX",
        sliderLeft,
        sfxY,
        sliderWidth,
        sfxVolume
    );

    DrawPillButton(backButton);

	// close popup if the user clicks the back button or presses ESC.
    if (
        (CP_Input_MouseTriggered(MOUSE_BUTTON_LEFT) &&
            backButton.isHovered) ||
        CP_Input_KeyTriggered(KEY_ESCAPE)
        ) {
        PlayMenuClick();
        showOptionsModal = false;
        activeVolumeSlider = 0;
    }
}


/* // ☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆ //

	How to play 

*/ // ☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆ //

static void DrawHowToModal(
    float centerX,
    float centerY,
    float windowW,
    float windowH)
{
    CP_Settings_RectMode(CP_POSITION_CENTER);

	// dim screen behind the popup.
    CP_Settings_NoStroke();
    CP_Settings_Fill(CP_Color_Create(0, 0, 0, 150));

    CP_Graphics_DrawRect(
        centerX,
        centerY,
        windowW,
        windowH
    );

	// popup panel.
    float panelY = windowH * 0.55f;

    CP_Settings_Fill(CP_Color_Create(20, 20, 25, 250));
    CP_Settings_Stroke(CP_Color_Create(225, 75, 95, 255));
    CP_Settings_StrokeWeight(4.0f);

    CP_Graphics_DrawRectAdvanced(
        centerX,
        panelY,
        620.0f,
        310.0f,
        0.0f,
        15.0f
    );

    CP_Settings_NoStroke();
    CP_Settings_TextAlignment(
        CP_TEXT_ALIGN_H_CENTER,
        CP_TEXT_ALIGN_V_MIDDLE
    );

    if (fontTitle != NULL) {
        CP_Font_Set(fontTitle);
    }

    CP_Settings_Fill(CP_Color_Create(255, 255, 255, 255));
    CP_Settings_TextSize(18.0f);

    CP_Font_DrawText(
        "HOW TO PLAY",
        centerX,
        panelY - 110.0f
    );

    if (fontBody != NULL) {
        CP_Font_Set(fontBody);
    }

    CP_Settings_TextSize(26.0f);

    CP_Font_DrawText(
        "- Use [W / S] to switch lanes",
        centerX,
        panelY - 55.0f
    );

    CP_Font_DrawText(
        "- Dodge oil slicks and fatal pit holes",
        centerX,
        panelY - 15.0f
    );

    CP_Font_DrawText(
        "- Collect multiplier stars & speed boosts",
        centerX,
        panelY + 25.0f
    );

    CP_Font_DrawText(
        "- Don't let the chasing bowling ball catch you!",
        centerX,
        panelY + 65.0f
    );

    CP_Settings_Fill(CP_Color_Create(225, 75, 95, 255));
    CP_Settings_TextSize(22.0f);

    CP_Font_DrawText(
        "[ Click anywhere or press ESC to return ]",
        centerX,
        panelY + 120.0f
    );

    if (
        CP_Input_MouseTriggered(MOUSE_BUTTON_LEFT) ||
        CP_Input_KeyTriggered(KEY_ESCAPE)
        ) {
        PlayMenuClick();
        showHowToModal = false;
    }
}

// characters chasing each other across the bottom of the menu.
// animate the sprite frames and move the chase across the menu.
static void DrawMenuChase(
    float dt,
    float windowW,
    float windowH)
{
    float menuScale = windowH / 720.0f;

    float pinSize = MENU_PIN_SIZE * menuScale;
    float ballSize = MENU_BALL_SIZE * menuScale;
    float chaseGap = MENU_CHASE_GAP * menuScale;

    // pin animation.
    menuPinFrameTimer += dt;

    while (menuPinFrameTimer >= MENU_PIN_FRAME_TIME)
    {
        menuPinFrameTimer -= MENU_PIN_FRAME_TIME;

        menuPinFrame =
            (menuPinFrame + 1) % MENU_PIN_FRAME_COUNT;
    }

	// ball animation.
    menuBallFrameTimer += dt;

    while (menuBallFrameTimer >= MENU_BALL_FRAME_TIME)
    {
        menuBallFrameTimer -= MENU_BALL_FRAME_TIME;

        menuBallFrame =
            (menuBallFrame + 1) % MENU_BALL_FRAME_COUNT;
    }

    menuChaseTimer += dt;

    menuChaseX += MENU_CHASE_SPEED * menuScale * dt;

    float pinX = menuChaseX;
    float ballX = menuChaseX - chaseGap;

	// restart the chase when both characters haveleft the screen.
    if (ballX - ballSize / 2.0f > windowW)
    {
        menuChaseX = -pinSize / 2.0f;

        pinX = menuChaseX;
        ballX = menuChaseX - chaseGap;
    }

    float groundY = windowH * 0.90f;
    float ballYOffset = 190.0f * menuScale;
    float pinYOffset = 0.0f;

    CP_Settings_ImageMode(CP_POSITION_CENTER);

    if (menuBall != NULL)
    {
        float sourceLeft =
            (float)menuBallFrame * MENU_BALL_FRAME_W;

        CP_Image_DrawSubImage(
            menuBall,
            ballX,
            groundY - ballSize / 2.0f + ballYOffset,
            ballSize,
            ballSize,
            sourceLeft,
            0.0f,
            sourceLeft + MENU_BALL_FRAME_W,
            MENU_BALL_FRAME_H,
            255
        );
    }

    if (menuPin != NULL)
    {
        float sourceLeft =
            (float)menuPinFrame * MENU_PIN_FRAME_W;

        CP_Image_DrawSubImage(
            menuPin,
            pinX,
            groundY - pinSize / 2.0f + pinYOffset,
            pinSize,
            pinSize,
            sourceLeft,
            0.0f,
            sourceLeft + MENU_PIN_FRAME_W,
            MENU_PIN_FRAME_H,
            255
        );
    }
}

/* // ☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆ //

	Main menu state functions

*/ // ☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆ //

void Main_Menu_Init(void)
{
    buttonActionTimer = 0.0f;
    pendingButtonAction = 0;

    currentRequestedState = STATE_MAIN_MENU;

    showHowToModal = false;
    showOptionsModal = false;
    activeVolumeSlider = 0;

    floatTimer = 0.0f;

    // apply the last display mode used when returning to the menu.
    if (!displaySettingsInitialized)
    {
        windowedWidth = CP_System_GetWindowWidth();
        windowedHeight = CP_System_GetWindowHeight();

        displayMode = DISPLAY_WINDOWED;
        displaySettingsInitialized = true;
    }

	// initialize audio settings only once when the main menu is first loaded.
    if (!audioSettingsInitialized) {
        musicVolume = 0.2f;
        sfxVolume = 1.0f;
        audioSettingsInitialized = true;
    }

    ApplyAudioVolumes();

	// button click sound effect.
    buttonSound = CP_Sound_Load(
        "../Assets/main_menu_btn_sound.mp3"
    );

    // main menu music.
    menuMusic = CP_Sound_Load(
        "../Assets/main_menu_bg_music.mp3"
    );

    if (menuMusic != NULL) {
        CP_Sound_PlayAdvanced(
            menuMusic,
            1.0f,
            1.0f,
            true,
            CP_SOUND_GROUP_MUSIC
        );
    }

    // title font.
    fontTitle = CP_Font_Load(
        "Assets/PressStart2P-Regular.ttf"
    );

    if (!fontTitle) {
        fontTitle = CP_Font_Load(
            "./Assets/PressStart2P-Regular.ttf"
        );
    }

    if (!fontTitle) {
        fontTitle = CP_Font_Load(
            "../Assets/PressStart2P-Regular.ttf"
        );
    }

    // body font.
    fontBody = CP_Font_Load("Assets/VT323-Regular.ttf");

    if (!fontBody) {
        fontBody = CP_Font_Load("./Assets/VT323-Regular.ttf");
    }

    if (!fontBody) {
        fontBody = CP_Font_Load("../Assets/VT323-Regular.ttf");
    }

    // background.
    bgImage = CP_Image_Load(
        "Assets/mainmenu_background.png"
    );

    if (!bgImage) {
        bgImage = CP_Image_Load(
            "./Assets/mainmenu_background.png"
        );
    }

    if (!bgImage) {
        bgImage = CP_Image_Load(
            "../Assets/mainmenu_background.png"
        );
    }

    UpdateMenuButtonLayout(
        (float)CP_System_GetWindowWidth(),
        (float)CP_System_GetWindowHeight()
    );

	// chase animation resources.
    menuPin = CP_Image_Load(
        "../Assets/bowling_pin_running.png"
    );

    menuBall = CP_Image_Load(
        "../Assets/bowling_ball_chasing.png"
    );

    // restart movement.
    float menuScale =
    (float)CP_System_GetWindowHeight() / 720.0f;

menuChaseX =
    -(MENU_PIN_SIZE * menuScale) / 2.0f;
    menuChaseTimer = 0.0f;

    // restart both animations.
    menuPinFrame = 0;
    menuBallFrame = 0;

    menuPinFrameTimer = 0.0f;
    menuBallFrameTimer = 0.0f;

    // print message if an image cannot be loaded.
    if (menuPin == NULL) {
        printf("Could not load the menu pin sprite sheet.\n");
    }

    if (menuBall == NULL) {
        printf("Could not load the menu ball sprite sheet.\n");
    }
}


/* // ☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆ //

	Main menu update function

*/ // ☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆ //

void Main_Menu_Update(void)
{
    float dt = CP_System_GetDt();

    // complete a delayed Play or Exit action.
    if (pendingButtonAction != 0) {
        buttonActionTimer -= dt;

        if (buttonActionTimer <= 0.0f) {
            int action = pendingButtonAction;
            pendingButtonAction = 0;

            if (action == 1) {
                CP_Engine_SetNextGameStateForced(
                    Game_Init,
                    Game_Update,
                    Game_Exit
                );
            }
            else if (action == 2) {
                currentRequestedState = STATE_EXIT;
                CP_Engine_Terminate();
            }

            return;
        }
    }

    // title animation.
    floatTimer += dt * 2.5f;
    float floatOffset = sinf(floatTimer) * 8.0f;

    float mouseX = CP_Input_GetMouseX();
    float mouseY = CP_Input_GetMouseY();

    float windowW = (float)CP_System_GetWindowWidth();
    float windowH = (float)CP_System_GetWindowHeight();

    float centerX = windowW / 2.0f;
    float centerY = windowH / 2.0f;

    UpdateMenuButtonLayout(windowW, windowH);

    CP_Color whiteFill = CP_Color_Create(255, 255, 255, 255);
    CP_Color blackStroke = CP_Color_Create(15, 15, 15, 255);
    CP_Color shadowColor = CP_Color_Create(0, 0, 0, 210);

    // background.
    CP_Graphics_ClearBackground(
        CP_Color_Create(35, 25, 25, 255)
    );

    if (bgImage != NULL) {
        CP_Image_Draw(
            bgImage,
            centerX,
            centerY,
            windowW,
            windowH,
            255
        );
    }

    // background chase, behind the title, buttons, and popups.
    DrawMenuChase(dt, windowW, windowH);

    // game title.
    CP_Settings_NoStroke();
    CP_Settings_TextAlignment(
        CP_TEXT_ALIGN_H_CENTER,
        CP_TEXT_ALIGN_V_MIDDLE
    );

    if (fontTitle != NULL) {
        CP_Font_Set(fontTitle);
    }

    float titleY = windowH * 0.20f + floatOffset;

    DrawTextWithOutline(
        "ALLEY RUNNER",
        centerX + 5.0f,
        titleY + 7.0f,
        90.0f,
        shadowColor,
        shadowColor,
        4.0f
    );

    DrawTextWithOutline(
        "ALLEY RUNNER",
        centerX,
        titleY,
        90.0f,
        whiteFill,
        blackStroke,
        4.0f
    );

    // tagline.
    if (fontBody != NULL) {
        CP_Font_Set(fontBody);
    }

    // adjust tagline's y level.
    float taglineY = titleY + 80.0f;

    CP_Settings_TextSize(40.0f);
    CP_Settings_Fill(CP_Color_Create(0, 0, 0, 230));

    CP_Font_DrawText(
        "Keep running. Don't get bowled over!",
        centerX + 2.0f,
        taglineY + 3.0f
    );

    DrawTextWithOutline(
        "Keep running. Don't get bowled over!",
        centerX,
        taglineY,
        40.0f,
        whiteFill,
        blackStroke,
        1.0f
    );

    // draw and handle normal buttons only when no popup is open.
    if (!showHowToModal && !showOptionsModal) {
        btnPlay.isHovered =
            IsMouseOverButton(btnPlay, mouseX, mouseY);

        btnHowTo.isHovered =
            IsMouseOverButton(btnHowTo, mouseX, mouseY);

        btnOptions.isHovered =
            IsMouseOverButton(btnOptions, mouseX, mouseY);

        btnExit.isHovered =
            IsMouseOverButton(btnExit, mouseX, mouseY);

        DrawPillButton(btnPlay);
        DrawPillButton(btnHowTo);
        DrawPillButton(btnOptions);
        DrawPillButton(btnExit);

        if (
            pendingButtonAction == 0 &&
            CP_Input_MouseTriggered(MOUSE_BUTTON_LEFT)
            ) {
            if (btnPlay.isHovered) {
                PlayMenuClick();

                pendingButtonAction = 1;
                buttonActionTimer = BUTTON_SOUND_DELAY;
            }
            else if (btnHowTo.isHovered) {
                PlayMenuClick();
                showHowToModal = true;
            }
            else if (btnOptions.isHovered) {
                PlayMenuClick();
                showOptionsModal = true;
                activeVolumeSlider = 0;
            }
            else if (btnExit.isHovered) {
                PlayMenuClick();

                pendingButtonAction = 2;
                buttonActionTimer = BUTTON_SOUND_DELAY;
            }
        }
    }
    else if (showOptionsModal) {
        DrawOptionsModal(
            centerX,
            centerY,
            windowW,
            windowH
        );
    }
    else if (showHowToModal) {
        DrawHowToModal(
            centerX,
            centerY,
            windowW,
            windowH
        );
    }

    // footer.
    if (fontBody != NULL)
    {
        CP_Font_Set(fontBody);
    }

	// scale the footer text and spacing based on the window size.
    float footerScale = fminf(
        windowW / 1280.0f,
        windowH / 720.0f
    );

    float footerSize = 36.0f * footerScale;
    float footerMargin = 25.0f * footerScale;
    float footerBottom = 20.0f * footerScale;

    float footerY = windowH - footerBottom;

    float shadowOffset = 2.0f * footerScale;
    float outlineWidth = 1.0f * footerScale;

    CP_Settings_NoStroke();
    CP_Settings_TextSize(footerSize);

	// high score text.
    char highScoreText[64];

    snprintf(
        highScoreText,
        sizeof(highScoreText),
        "HIGH SCORE: %d",
        Game_GetHighScore()
    );

    // high score: bottom left.
    CP_Settings_TextAlignment(
        CP_TEXT_ALIGN_H_LEFT,
        CP_TEXT_ALIGN_V_BOTTOM
    );

    CP_Settings_Fill(CP_Color_Create(0, 0, 0, 230));

    CP_Font_DrawText(
        highScoreText,
        footerMargin + shadowOffset,
        footerY + shadowOffset
    );

    DrawTextWithOutline(
        highScoreText,
        footerMargin,
        footerY,
        footerSize,
        whiteFill,
        blackStroke,
        outlineWidth
    );

	// team name: bottom right.
    CP_Settings_TextAlignment(
        CP_TEXT_ALIGN_H_RIGHT,
        CP_TEXT_ALIGN_V_BOTTOM
    );

    CP_Settings_Fill(CP_Color_Create(0, 0, 0, 230));

    CP_Font_DrawText(
        "Team IsaacACat - CSD1401",
        windowW - footerMargin + shadowOffset,
        footerY + shadowOffset
    );

    DrawTextWithOutline(
        "Team IsaacACat - CSD1401",
        windowW - footerMargin,
        footerY,
        footerSize,
        whiteFill,
        blackStroke,
        outlineWidth
    );
}


/* // ☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆ //

	Cleanup

*/ // ☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆ //

void Main_Menu_Exit(void)
{
    CP_Sound_StopGroup(CP_SOUND_GROUP_MUSIC);

    if (buttonSound != NULL) {
        CP_Sound_Free(buttonSound);
        buttonSound = NULL;
    }

    if (menuMusic != NULL) {
        CP_Sound_Free(menuMusic);
        menuMusic = NULL;
    }

    if (bgImage != NULL) {
        CP_Image_Free(bgImage);
        bgImage = NULL;
    }

    if (menuPin != NULL) {
        CP_Image_Free(menuPin);
        menuPin = NULL;
    }

    if (menuBall != NULL) {
        CP_Image_Free(menuBall);
        menuBall = NULL;
    }

    fontTitle = NULL;
    fontBody = NULL;

    activeVolumeSlider = 0;
    pendingButtonAction = 0;
    buttonActionTimer = 0.0f;
}


/* // ☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆ //

	State management

*/ // ☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆ //

GameState Main_Menu_GetNextState(void)
{
    return currentRequestedState;
}

void Main_Menu_SetState(GameState state)
{
    currentRequestedState = state;
}

// returns true if the current display mode is fullscreen or borderless windowed.
int Main_Menu_IsFullscreenMode(void)
{
    return displayMode == DISPLAY_FULLSCREEN ||
        displayMode == DISPLAY_BORDERLESS;
}