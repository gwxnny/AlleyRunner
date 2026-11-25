/*!
@file       : game_menus.h
@author     : Lek Kar Lim (lekkar.lim@digipen.edu) - pause menu, confirmation, death screen
@co-author  : Isaac Tan (isaacd.tan@digipen.edu) - restart option, keyboard controls, own module
              Ggwendolynn Lee Rasni (ggwendolynnlee.r@digipen.edu) - death screen revamp
@team       : IsaacACat
@module     : CSD1401 Software Engineering Project 1
@section    : B
@project    : Alley Runner
@brief      : Menus shown on top of gameplay: the pause button, the pause menu,
              the "are you sure?" confirmation and the results (death) screen.
              Each function draws its menu and returns what the player chose.

All content © 2026 DigiPen Institute of Technology Singapore. All Rights Reserved.
*//*________________________________________________________________________________*/
#ifndef GAME_MENUS_H
#define GAME_MENUS_H

#include <stdbool.h>

typedef enum
{
    MENU_ACTION_NONE,
    MENU_ACTION_RESUME,
    MENU_ACTION_RESTART,
    MENU_ACTION_MAIN_MENU
} MenuAction;

/* Final numbers for the results screen. */
typedef struct
{
    int baseScore;
    int multiplier;
    int finalScore;
    int coins;
    bool isNewHighScore;
    const char* causeText;   /* e.g. "YOU FELL INTO A HOLE!" */
} RunResults;

/* Closes any open confirmation (call when a run starts or the game unpauses). */
void GameMenus_Reset(void);

/* Draws the pause button (top right). Returns true when it is clicked. */
bool GameMenus_PauseButton(void);

/* Draws the pause menu or its confirmation and handles mouse + keyboard.
   acceptKeys is false on the frame the game was paused, so the same key
   press does not also close the menu.
   Returns RESUME, or RESTART / MAIN_MENU only after the player confirms. */
MenuAction GameMenus_PauseMenu(bool acceptKeys);

/* Draws the results screen. Returns RESTART or MAIN_MENU when chosen. */
MenuAction GameMenus_DeathScreen(const RunResults* results);

#endif /* GAME_MENUS_H */
