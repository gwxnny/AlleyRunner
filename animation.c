/*!
@file       : animation.c
@author     : Ang Xiao Hui (a.xiaohui@digipen.edu) - sprite sheets and frame timing
@co-author  : Lek Kar Lim (lekkar.lim@digipen.edu) - alpha drawing
              Isaac Tan (isaacd.tan@digipen.edu) - moved into its own module
@team       : IsaacACat
@module     : CSD1401 Software Engineering Project 1
@section    : B
@project    : Alley Runner
@brief      : Implementation of the sprite-sheet animation system.

All content © 2026 DigiPen Institute of Technology Singapore. All Rights Reserved.
*//*________________________________________________________________________________*/
#include "animation.h"

#include <stddef.h>   /* NULL */

void Anim_Init(Animation* anim, const char* path, int frameCount,
               float frameW, float frameH, float frameTime, bool loop)
{
    anim->sheet = CP_Image_Load(path);

    anim->frameCount = frameCount;
    anim->frameW = frameW;
    anim->frameH = frameH;
    anim->drawW = frameW;
    anim->drawH = frameH;

    anim->currentFrame = 0;
    anim->timer = 0.0f;
    anim->frameTime = frameTime;
    anim->loop = loop;
}

void Anim_Free(Animation* anim)
{
    if (anim->sheet != NULL)
    {
        CP_Image_Free(anim->sheet);
        anim->sheet = NULL;
    }
}

void Anim_SetDrawSize(Animation* anim, float w, float h)
{
    anim->drawW = w;
    anim->drawH = h;
}

void Anim_FitHorizontalSheet(Animation* anim, float drawHeight)
{
    int sheetWidth;
    int sheetHeight;

    if (anim == NULL || anim->sheet == NULL || anim->frameCount <= 0)
    {
        return;
    }

    sheetWidth = CP_Image_GetWidth(anim->sheet);
    sheetHeight = CP_Image_GetHeight(anim->sheet);

    if (sheetWidth <= 0 || sheetHeight <= 0)
    {
        return;
    }

    anim->frameW = (float)sheetWidth / (float)anim->frameCount;
    anim->frameH = (float)sheetHeight;
    anim->drawH = drawHeight;
    anim->drawW = drawHeight * (anim->frameW / anim->frameH);
}

void Anim_Reset(Animation* anim)
{
    anim->currentFrame = 0;
    anim->timer = 0.0f;
}

bool Anim_IsFinished(const Animation* anim)
{
    return !anim->loop && anim->currentFrame >= anim->frameCount - 1;
}

void Anim_Update(Animation* anim, float dt)
{
    if (anim->frameTime <= 0.0f || anim->frameCount <= 0 || Anim_IsFinished(anim))
    {
        return;
    }

    anim->timer += dt;

    /* A long frame may skip several animation frames at once. */
    while (anim->timer >= anim->frameTime)
    {
        anim->timer -= anim->frameTime;
        anim->currentFrame++;

        if (anim->currentFrame >= anim->frameCount)
        {
            if (anim->loop)
            {
                anim->currentFrame = 0;
            }
            else
            {
                anim->currentFrame = anim->frameCount - 1;
                anim->timer = 0.0f;
                break;
            }
        }
    }
}

void Anim_DrawFrame(const Animation* anim, int frame, float x, float y, int alpha)
{
    float left;

    if (anim->sheet == NULL)
    {
        return;
    }

    left = (float)frame * anim->frameW;

    CP_Image_DrawSubImage(anim->sheet, x, y, anim->drawW, anim->drawH,
        left, 0.0f, left + anim->frameW, anim->frameH, alpha);
}

void Anim_DrawAlpha(const Animation* anim, float x, float y, int alpha)
{
    Anim_DrawFrame(anim, anim->currentFrame, x, y, alpha);
}

void Anim_Draw(const Animation* anim, float x, float y)
{
    Anim_DrawFrame(anim, anim->currentFrame, x, y, 255);
}
