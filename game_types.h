/*!
@file       : game_types.h
@author     : Isaac Tan (isaacd.tan@digipen.edu)
@team       : IsaacACat
@module     : CSD1401 Software Engineering Project 1
@section    : B
@project    : Alley Runner
@brief      : Small shared types used by more than one gameplay module.

All content © 2026 DigiPen Institute of Technology Singapore. All Rights Reserved.
*//*________________________________________________________________________________*/
#ifndef GAME_TYPES_H
#define GAME_TYPES_H

/* Power-ups the mystery box can give. */
typedef enum
{
    PU_INVINCIBLE,
    PU_SHIELD,
    PU_MAGNET,
    NUM_POWERUPS
} PowerType;

#define MAX_LIVES 3   /* hearts at the start of a run, and the most you can hold */

#endif /* GAME_TYPES_H */
