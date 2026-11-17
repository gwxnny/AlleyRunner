//---------------------------------------------------------
// file:	main.c
// author:	IsaacACat
// email:	isaacd.tan@digipen.edu
//          ggwendolynnlee.r@digipen.edu
//          lekkar.lim@digipen.edu
//          a.xiaohui@digipen.edu
//
// brief:	Main entry point for the sample project
//			of the CProcessing library
//
// Copyright © 2026 DigiPen, All rights reserved.
//---------------------------------------------------------
#include "cprocessing.h"
#include "mainmenu.h"
#include "splash.h"
#include <stdio.h>

int main(void) {
	CP_Engine_SetNextGameState(Splash_Init, Splash_Update, Splash_Exit); // set the initial game state to the splash screen
    // main() the starting point for the program
    // CP_Engine_SetNextGameState() tells CProcessing which functions to use for init, update and exit
    //// CP_Engine_Run() is the core function that starts the simulation
    //CP_Engine_SetNextGameState(Main_Menu_Init, Main_Menu_Update, Main_Menu_Exit);
    CP_System_SetWindowSize(1600, 900);
    CP_Engine_Run(0);

    return 0;
}