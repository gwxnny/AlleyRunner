/*!
@file       : transition.h
@author     : Isaac Tan (isaacd.tan@digipen.edu)
@team       : IsaacACat
@module     : CSD1401 Software Engineering Project 1
@section    : B
@project    : Alley Runner
@brief      : Screen transition system. Every change of game state goes
              through Transition_GoTo, which fades the screen to black,
              switches state, then fades the new screen in.

All content © 2026 DigiPen Institute of Technology Singapore. All Rights Reserved.
*//*________________________________________________________________________________*/
#ifndef TRANSITION_H
#define TRANSITION_H

#include "cprocessing.h"
#include <stdbool.h>

// registers the fade overlay as CProcessing's post-update function. Call once in main() before CP_Engine_Run.
void Transition_Init(void);

// fades out, then switches to the given state and fades it in. Ignored if a fade-out is already running (prevents double clicks).
void Transition_GoTo(FunctionPtr init, FunctionPtr update, FunctionPtr exit);

// true while a fade-out or fade-in is in progress (the game state is not yet fully visible).
bool Transition_IsBusy(void);

#endif /* TRANSITION_H */
