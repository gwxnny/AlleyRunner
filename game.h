/*!
@file       : game.h
@author     : Isaac Tan (isaacd.tan@digipen.edu)
@team       : IsaacACat
@module     : CSD1401 Software Engineering Project 1
@section    : B
@project    : Alley Runner
@brief      : Gameplay state (the run itself).

All content © 2026 DigiPen Institute of Technology Singapore. All Rights Reserved.
*//*________________________________________________________________________________*/
#ifndef GAME_H
#define GAME_H

void Game_Init(void);
void Game_Update(void);
void Game_Exit(void);

/* Best final score this session (shown on the main menu). */
int Game_GetHighScore(void);

#endif /* GAME_H */
