/*!
@file       : main.c
@author     : Isaac Tan (isaacd.tan@digipen.edu)
@team       : IsaacACat
              Isaac Tan (isaacd.tan@digipen.edu)
              Ggwendolynn Lee Rasni (ggwendolynnlee.r@digipen.edu)
              Lek Kar Lim (lekkar.lim@digipen.edu)
              Ang Xiao Hui (a.xiaohui@digipen.edu)
@module     : CSD1401 Software Engineering Project 1
@section    : B
@project    : Alley Runner
@brief      : Program entry point. Opens a 1600 x 900 window, registers the
              screen transitions and starts on the splash screen.

All content © 2026 DigiPen Institute of Technology Singapore. All Rights Reserved.
*//*________________________________________________________________________________*/

// memory leak detection in debug builds
#ifdef _DEBUG
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#endif

#include "cprocessing.h"
#include "splash.h"
#include "transition.h"

#define WINDOW_WIDTH  1600
#define WINDOW_HEIGHT 900

/*! Specification for function main:
@brief Program entry point.

Turns on memory leak checking in Debug builds, opens a 1600 x 900 window,
registers the screen transitions and starts on the splash screen.

@param none.
@return 0 when the program closes.
*//*________________________________________________________________________________*/
int main(void)
{
#ifdef _DEBUG
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    CP_System_SetWindowSize(WINDOW_WIDTH, WINDOW_HEIGHT);
    CP_System_SetWindowTitle("Alley Runner");

    /* Every state change fades through black (see transition.c). */
    Transition_Init();

    CP_Engine_SetNextGameState(Splash_Init, Splash_Update, Splash_Exit);
    CP_Engine_Run(0);   /* 0 = no console window */

    return 0;
}