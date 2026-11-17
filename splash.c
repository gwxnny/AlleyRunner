#include "splash.h"
#include "cprocessing.h"
#include "mainmenu.h"
#include <stdio.h>

#define SPLASH_COUNT 1

/* Total duration per logo, including both fades. */
#define SPLASH_HOLD_TIME  3.0f
#define SPLASH_FADE_TIME  0.75f

static const char* splashPaths[SPLASH_COUNT] =
{
    "Assets/DigiPen_Singapore_WEB_WHITE.png"
};

static CP_Image logos[SPLASH_COUNT] = { NULL };

static int current = 0;
static float timer = 0.0f;


static CP_BOOL Splash_Skip_Pressed(void)
{
    return CP_Input_MouseTriggered(MOUSE_BUTTON_LEFT);
}


void Splash_Init(void)
{
    CP_System_SetWindowTitle("ALLEY RUNNER");
    CP_Settings_ImageMode(CP_POSITION_CENTER);

    current = 0;
    timer = 0.0f;

    for (int i = 0; i < SPLASH_COUNT; i++)
    {
        logos[i] = CP_Image_Load(splashPaths[i]);

        if (logos[i] == NULL)
        {
            printf(
                "Could not load splash image: %s\n",
                splashPaths[i]
            );
        }
    }
}


void Splash_Update(void)
{
    CP_Settings_ResetMatrix();

    CP_Graphics_ClearBackground(
        CP_Color_Create(0, 0, 0, 255)
    );

    /* Advance to the next logo after its duration or on click. */
    if (Splash_Skip_Pressed() || timer >= SPLASH_HOLD_TIME)
    {
        current++;
        timer = 0.0f;

        if (current >= SPLASH_COUNT)
        {
            CP_Engine_SetNextGameState(
                Main_Menu_Init,
                Main_Menu_Update,
                Main_Menu_Exit
            );

            return;
        }
    }

    float opacity = 1.0f;

    if (timer < SPLASH_FADE_TIME)
    {
        opacity = timer / SPLASH_FADE_TIME;
    }
    else if (timer > SPLASH_HOLD_TIME - SPLASH_FADE_TIME)
    {
        opacity =
            (SPLASH_HOLD_TIME - timer) / SPLASH_FADE_TIME;
    }

    opacity = CP_Math_ClampFloat(opacity, 0.0f, 1.0f);

    /* Smooth the beginning and end of each fade. */
    opacity = opacity * opacity * (3.0f - 2.0f * opacity);

    if (logos[current] != NULL)
    {
        float imageW =
            (float)CP_Image_GetWidth(logos[current]);

        float imageH =
            (float)CP_Image_GetHeight(logos[current]);

        if (imageW > 0.0f && imageH > 0.0f)
        {
            float windowW =
                (float)CP_System_GetWindowWidth();

            float windowH =
                (float)CP_System_GetWindowHeight();

            float maxW = windowW * 0.70f;
            float maxH = windowH * 0.60f;

            float scaleX = maxW / imageW;
            float scaleY = maxH / imageH;

            float scale =
                scaleX < scaleY ? scaleX : scaleY;

            CP_Settings_ImageMode(CP_POSITION_CENTER);

            CP_Image_Draw(
                logos[current],
                windowW * 0.5f,
                windowH * 0.5f,
                imageW * scale,
                imageH * scale,
                (int)(opacity * 255.0f)
            );
        }
    }

    timer += CP_System_GetDt();
}


void Splash_Exit(void)
{
    for (int i = 0; i < SPLASH_COUNT; i++)
    {
        if (logos[i] != NULL)
        {
            CP_Image_Free(&logos[i]);
            logos[i] = NULL;
        }
    }
}