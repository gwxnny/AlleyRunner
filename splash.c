/*!
@file       : splash.c
@author     : Isaac Tan (isaacd.tan@digipen.edu)
@team       : IsaacACat
@module     : CSD1401 Software Engineering Project 1
@section    : B
@project    : Alley Runner
@brief      : Splash screen state. Shows the DigiPen Singapore logo with its
              copyright line (part of the image), then the team logo. Each
              fades in, holds and fades out over 3 seconds. A single click,
              Space, Enter or Escape skips ALL remaining splash screens.

All content © 2026 DigiPen Institute of Technology Singapore. All Rights Reserved.
*//*________________________________________________________________________________*/
#include "splash.h"
#include "mainmenu.h"
#include "transition.h"
#include "cprocessing.h"

#define SPLASH_COUNT 2

#define SPLASH_HOLD_TIME 3.0f    /* total time per logo, including both fades */
#define SPLASH_FADE_TIME 0.75f   /* fade-in and fade-out length */

#define DIGIPEN_LOGO_WIDTH 1.0f  /* fraction of the window width each logo may use */
#define TEAM_LOGO_WIDTH    0.65f
#define LOGO_MAX_HEIGHT    0.85f /* fraction of the window height */

static const char* splashPaths[SPLASH_COUNT] =
{
    "Assets/DigiPen Singapore Splash Screen.png",
    "Assets/team_logo.png"
};

static const float splashWidths[SPLASH_COUNT] =
{
    DIGIPEN_LOGO_WIDTH,
    TEAM_LOGO_WIDTH
};

static CP_Image logos[SPLASH_COUNT] = { NULL };
static int current = 0;
static float timer = 0.0f;
static int leaving = 0;   /* 1 once the switch to the main menu has started */

/* Any of the inputs the TCR lists for bypassing intro screens. */
static CP_BOOL SkipPressed(void)
{
    return CP_Input_MouseTriggered(MOUSE_BUTTON_LEFT) ||
        CP_Input_MouseTriggered(MOUSE_BUTTON_RIGHT) ||
        CP_Input_KeyTriggered(KEY_SPACE) ||
        CP_Input_KeyTriggered(KEY_ENTER) ||
        CP_Input_KeyTriggered(KEY_ESCAPE) ||
        CP_Input_GamepadTriggered(GAMEPAD_START) ||
        CP_Input_GamepadTriggered(GAMEPAD_A);
}

static void GoToMainMenu(void)
{
    leaving = 1;
    Transition_GoTo(MainMenu_Init, MainMenu_Update, MainMenu_Exit);
}

void Splash_Init(void)
{
    CP_System_SetWindowTitle("Alley Runner");
    CP_Settings_ImageMode(CP_POSITION_CENTER);

    current = 0;
    timer = 0.0f;
    leaving = 0;

    for (int i = 0; i < SPLASH_COUNT; i++)
    {
        logos[i] = CP_Image_Load(splashPaths[i]);
    }
}

void Splash_Update(void)
{
    float opacity = 1.0f;

    CP_Settings_ResetMatrix();
    CP_Graphics_ClearBackground(CP_Color_Create(0, 0, 0, 255));

    if (!leaving)
    {
        if (SkipPressed())
        {
            GoToMainMenu();
        }
        else if (timer >= SPLASH_HOLD_TIME)
        {
            timer = 0.0f;
            current++;

            if (current >= SPLASH_COUNT)
            {
                current = SPLASH_COUNT - 1;
                GoToMainMenu();
            }
        }
    }

    /* Fade in, hold, fade out. */
    if (timer < SPLASH_FADE_TIME)
    {
        opacity = timer / SPLASH_FADE_TIME;
    }
    else if (timer > SPLASH_HOLD_TIME - SPLASH_FADE_TIME)
    {
        opacity = (SPLASH_HOLD_TIME - timer) / SPLASH_FADE_TIME;
    }

    opacity = CP_Math_ClampFloat(opacity, 0.0f, 1.0f);
    opacity = opacity * opacity * (3.0f - 2.0f * opacity);   /* smoothstep */

    if (logos[current] != NULL)
    {
        float imageW = (float)CP_Image_GetWidth(logos[current]);
        float imageH = (float)CP_Image_GetHeight(logos[current]);

        if (imageW > 0.0f && imageH > 0.0f)
        {
            float windowW = (float)CP_System_GetWindowWidth();
            float windowH = (float)CP_System_GetWindowHeight();
            float scaleX = windowW * splashWidths[current] / imageW;
            float scaleY = windowH * LOGO_MAX_HEIGHT / imageH;
            float scale = scaleX < scaleY ? scaleX : scaleY;

            CP_Settings_ImageMode(CP_POSITION_CENTER);
            CP_Image_Draw(logos[current], windowW * 0.5f, windowH * 0.5f,
                imageW * scale, imageH * scale, (int)(opacity * 255.0f));
        }
    }

    if (!leaving)
    {
        timer += CP_System_GetDt();
    }
}

void Splash_Exit(void)
{
    for (int i = 0; i < SPLASH_COUNT; i++)
    {
        if (logos[i] != NULL)
        {
            CP_Image_Free(logos[i]);
            logos[i] = NULL;
        }
    }
}
