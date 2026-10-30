/*!
@file       : mainmenu.h
@team       : IsaacACat
@module     : CSD1401 Software Engineering Project 1
@section    : section b
@project    : Team Project 1 - Alley Runner
@brief      : This file contains the interface for the main menu state for the Alley Runner.
*//*________________________________________________________________________________*/
#ifndef MAINMENU_H
#define MAINMENU_H

#include "cprocessing.h"

// screen/states in Alley Runner.
typedef enum {
    STATE_MAIN_MENU,
    STATE_GAMEPLAY,
    STATE_PAUSE,
    STATE_DEATH,
    STATE_EXIT
} GameState;

// main menu state functions.
void Main_Menu_Init(void);
void Main_Menu_Update(void);
void Main_Menu_Exit(void);

// get and change the current state of the game.
GameState Main_Menu_GetNextState(void);
void Main_Menu_SetState(GameState state);

#endif // MAINMENU_H