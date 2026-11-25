/*!
@file       : rage_overlay.h
@author     : Ggwendolynn Lee Rasni (ggwendolynnlee.r@digipen.edu)
@team       : IsaacACat
@module     : CSD1401 Software Engineering Project 1
@section    : B
@project    : Alley Runner
@brief      : "Ragebait" distraction system. At random moments, rows of
              coloured taunts sweep across the screen to distract the player.
              The words never collide with anything. Timings come from the
              tuning file (rage_quiet_min / rage_quiet_max / rage_duration).

All content © 2026 DigiPen Institute of Technology Singapore. All Rights Reserved.
*//*________________________________________________________________________________*/
#ifndef RAGE_OVERLAY_H
#define RAGE_OVERLAY_H

#include <stdbool.h>

/* hides the overlay and picks a random quiet period before the next burst. */
void RageOverlay_Reset(void);

/* counts down the quiet period / the active burst. */
void RageOverlay_Update(float dt);

/* draws the moving words while a burst is active. */
void RageOverlay_Draw(void);

/* true while the words are on screen. */
bool RageOverlay_IsActive(void);

#endif /* RAGE_OVERLAY_H */
