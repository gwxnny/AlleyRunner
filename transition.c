/*!
@file       : transition.c
@author     : Isaac Tan (isaacd.tan@digipen.edu)
@team       : IsaacACat
@module     : CSD1401 Software Engineering Project 1
@section    : B
@project    : Alley Runner
@brief      : Implementation of the fade-to-black screen transitions.

All content © 2026 DigiPen Institute of Technology Singapore. All Rights Reserved.
*//*________________________________________________________________________________*/
#include "transition.h"

#define FADE_OUT_TIME 0.30f   /* seconds to fade the old screen to black */
#define FADE_IN_TIME  0.40f   /* seconds to fade the new screen in */
#define MAX_FADE_DT   (1.0f / 30.0f) /* loading a state can produce one very long frame;
                                         clamp it so the fade-in is still visible */

typedef enum
{
    FADE_IDLE,
    FADE_OUT,
    FADE_IN
} FadePhase;

static FadePhase phase = FADE_IN;   /* the first screen also fades in */
static float blackAlpha = 1.0f;     /* 0 = fully visible, 1 = fully black */

static FunctionPtr nextInit = NULL;
static FunctionPtr nextUpdate = NULL;
static FunctionPtr nextExit = NULL;

/* Runs after every state's update: advances the fade and draws the overlay. */
static void Transition_PostUpdate(void)
{
    float dt = CP_System_GetDt();

    if (dt > MAX_FADE_DT)
    {
        dt = MAX_FADE_DT;
    }

    if (phase == FADE_OUT)
    {
        blackAlpha += dt / FADE_OUT_TIME;

        if (blackAlpha >= 1.0f)
        {
            /* Screen is black: switch state now, the new state fades in. */
            blackAlpha = 1.0f;
            phase = FADE_IN;
            CP_Engine_SetNextGameStateForced(nextInit, nextUpdate, nextExit);
        }
    }
    else if (phase == FADE_IN)
    {
        blackAlpha -= dt / FADE_IN_TIME;

        if (blackAlpha <= 0.0f)
        {
            blackAlpha = 0.0f;
            phase = FADE_IDLE;
        }
    }

    if (blackAlpha > 0.0f)
    {
        CP_Settings_ResetMatrix();
        CP_Settings_RectMode(CP_POSITION_CORNER);
        CP_Settings_NoStroke();
        CP_Settings_Fill(CP_Color_Create(0, 0, 0, (int)(blackAlpha * 255.0f)));
        CP_Graphics_DrawRect(0.0f, 0.0f,
            (float)CP_System_GetWindowWidth(), (float)CP_System_GetWindowHeight());
    }
}

void Transition_Init(void)
{
    CP_Engine_SetPostUpdateFunction(Transition_PostUpdate);
}

void Transition_GoTo(FunctionPtr init, FunctionPtr update, FunctionPtr exit)
{
    if (phase == FADE_OUT)
    {
        return;
    }

    nextInit = init;
    nextUpdate = update;
    nextExit = exit;
    phase = FADE_OUT;
}

bool Transition_IsBusy(void)
{
    return phase == FADE_OUT;
}
