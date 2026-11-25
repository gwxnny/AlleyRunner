/*!
@file       : animation.h
@author     : Ang Xiao Hui (a.xiaohui@digipen.edu) - sprite sheets and frame timing
@co-author  : Lek Kar Lim (lekkar.lim@digipen.edu) - alpha drawing
              Isaac Tan (isaacd.tan@digipen.edu) - moved into its own module
@team       : IsaacACat
@module     : CSD1401 Software Engineering Project 1
@section    : B
@project    : Alley Runner
@brief      : Sprite-sheet animation system. A sheet is a single row of
              equally sized frames; an Animation steps through them over time.

All content © 2026 DigiPen Institute of Technology Singapore. All Rights Reserved.
*//*________________________________________________________________________________*/
#ifndef ANIMATION_H
#define ANIMATION_H

#include "cprocessing.h"
#include <stdbool.h>

typedef struct
{
    CP_Image sheet;     /* the whole sprite sheet (NULL if it failed to load) */

    int frameCount;     /* number of frames in the row */

    float frameW;       /* size of one frame inside the sheet, in pixels */
    float frameH;

    float drawW;        /* size drawn on screen */
    float drawH;

    int currentFrame;
    float timer;        /* time spent on the current frame */
    float frameTime;    /* seconds per frame */

    bool loop;          /* false: stop on the last frame */
} Animation;

/* Loads the sheet and sets up the frame layout. Draw size starts at frame size. */
void Anim_Init(Animation* anim, const char* path, int frameCount,
               float frameW, float frameH, float frameTime, bool loop);

/* Frees the sheet (safe to call on an animation that never loaded). */
void Anim_Free(Animation* anim);

void Anim_SetDrawSize(Animation* anim, float w, float h);

/* Works out the frame size from the image itself and keeps its aspect ratio
   at the given draw height. Useful when the sheet's exact size is unknown. */
void Anim_FitHorizontalSheet(Animation* anim, float drawHeight);

void Anim_Reset(Animation* anim);

/* Advances the animation by dt seconds. */
void Anim_Update(Animation* anim, float dt);

/* True when a non-looping animation is showing its last frame. */
bool Anim_IsFinished(const Animation* anim);

/* Draws the current frame centred at (x, y). */
void Anim_Draw(const Animation* anim, float x, float y);
void Anim_DrawAlpha(const Animation* anim, float x, float y, int alpha);

/* Draws a chosen frame. Used when many objects share one sheet but each
   keeps its own frame counter (e.g. every oil spill). */
void Anim_DrawFrame(const Animation* anim, int frame, float x, float y, int alpha);

#endif /* ANIMATION_H */
