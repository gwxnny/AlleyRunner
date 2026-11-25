/*!
@file       : rage_overlay.c
@author     : Ggwendolynn Lee Rasni (ggwendolynnlee.r@digipen.edu)
@co-author  : Isaac Tan (isaacd.tan@digipen.edu) - moved into its own module, tuning values
@team       : IsaacACat
@module     : CSD1401 Software Engineering Project 1
@section    : B
@project    : Alley Runner
@brief      : Implementation of the ragebait distraction overlay.

All content © 2026 DigiPen Institute of Technology Singapore. All Rights Reserved.
*//*________________________________________________________________________________*/
#include "rage_overlay.h"
#include "tuning.h"
#include "ui.h"
#include "cprocessing.h"

#include <math.h>
#include <string.h>

#define RAGE_DRAW_SIZE        64.0f  /* text size on the 1280 x 720 design canvas */
#define RAGE_ROWS             20     /* rows of text covering the screen */
#define RAGE_PHRASES_PER_ROW  8
#define RAGE_PHRASE_GAP       0.0f   /* horizontal gap between phrases */
#define RAGE_ROW_STAGGER      120.0f /* rows start at different x so they do not line up */
#define RAGE_MAX_DT           0.05f  /* ignore long hitches so a burst is never skipped */

static bool  eventActive = false;  /* true while the words are on screen */
static float quietTimer = 0.0f;    /* time until the next burst */
static float eventTimer = 0.0f;    /* time left in the current burst */

static const char* rageWords[] =
{
    "SKILL ISSUE",
    "TOO SLOW",
    "NICE TRY",
    "JUST DODGE",
    "YOU MISSED",
    "TRY HARDER",
    "NOT EVEN CLOSE",
    "STILL RUNNING?",
    "PRASANNA GHALI",
    "VADIM SUROV",
    "SOROOR",
    "TAN CHEE WEI TOMMY",
    "RICHARD TSAO",
    "FONG FOO HOONG",
    "GERALD WONG HAN FENG",
    "ROSA ANAJAO",
    "TANG LIANG",
    "TRALALERO TRALALA",
    "TUNG TUNG TUNG SAHUR",
    "BRR BRR PATAPIM",
    "CAPPUCINO ASSASSINO",
    "TRIPPI TROPPI",
    "BOMBARDIRO CROCODILO",
    "BALLERINA CAPPUCINA"
};

/* number of phrases: size of the whole array / size of one entry. */
#define RAGE_WORD_COUNT ((int)(sizeof(rageWords) / sizeof(rageWords[0])))

/*! Specification for function PhraseIndex:
@brief Picks which phrase is shown in a row and slot.

The pattern looks random but never changes, so a row keeps its words while it scrolls.

@param row - The row of text.
@param phrase - The slot within the row.
@return The index into the rage words array.
*//*________________________________________________________________________________*/
static int PhraseIndex(int row, int phrase)
{
    return (row * 5 + phrase * 7) % RAGE_WORD_COUNT;
}

/*! Specification for function RandomQuietTime:
@brief Picks a random wait before the next rage event.

@param none.
@return Seconds to wait, between the tuning file's rage_quiet_min and rage_quiet_max.
*//*________________________________________________________________________________*/
static float RandomQuietTime(void)
{
    const GameTuning* tuning = Tuning_Get();
    return CP_Random_RangeFloat(tuning->rageQuietMin, tuning->rageQuietMax);
}

/*! Specification for function RageOverlay_Reset:
@brief Resets the rage words overlay and sets a random waiting time.

Hides the rage words and picks the waiting time before the next rage event.

@param none.
@return none.
*//*________________________________________________________________________________*/
void RageOverlay_Reset(void)
{
    eventActive = false;
    eventTimer = 0.0f;
    quietTimer = RandomQuietTime();
}

/*! Specification for function RageOverlay_IsActive:
@brief Checks whether the rage words are on screen.

@param none.
@return true - If a rage event is active, otherwise false.
*//*________________________________________________________________________________*/
bool RageOverlay_IsActive(void)
{
    return eventActive;
}

/*! Specification for function RageOverlay_Update:
@brief Updates the timers that show and hide the rage words.

Starts a rage event when the waiting time ends. When the event ends,
hides the words and sets another random waiting time.

@param dt - The time passed since the previous frame, in seconds.
@return none.
*//*________________________________________________________________________________*/
void RageOverlay_Update(float dt)
{
    if (dt < 0.0f)
    {
        dt = 0.0f;
    }
    else if (dt > RAGE_MAX_DT)
    {
        dt = RAGE_MAX_DT;
    }

    if (!eventActive)
    {
        quietTimer -= dt;

        if (quietTimer <= 0.0f)
        {
            eventActive = true;
            eventTimer = Tuning_Get()->rageDuration;
        }

        return;
    }

    eventTimer -= dt;

    if (eventTimer <= 0.0f)
    {
        eventActive = false;
        eventTimer = 0.0f;
        quietTimer = RandomQuietTime();
    }
}

/*! Specification for function RageOverlay_Draw:
@brief Draws the rage words moving across the screen.

During an active rage event, displays rows of coloured phrases
moving from right to left. Scales the text to fit the window size.

@param none.
@return none.
*//*________________________________________________________________________________*/
void RageOverlay_Draw(void)
{
    CP_Font font = UI_GetBodyFont();
    float screenW;
    float screenH;
    float drawSize;
    float duration;
    float progress;
    float rowHeight;

    if (!eventActive || font == NULL)
    {
        return;
    }

    screenW = (float)CP_System_GetWindowWidth();
    screenH = (float)CP_System_GetWindowHeight();
    drawSize = RAGE_DRAW_SIZE * UI_GetScale();
    duration = Tuning_Get()->rageDuration;

    /* 0 when the burst starts, 1 when it ends. */
    progress = (duration > 0.0f) ? (duration - eventTimer) / duration : 1.0f;
    progress = CP_Math_ClampFloat(progress, 0.0f, 1.0f);

    rowHeight = screenH / (float)RAGE_ROWS;

    CP_Font_Set(font);
    CP_Settings_NoStroke();
    CP_Settings_TextAlignment(CP_TEXT_ALIGN_H_LEFT, CP_TEXT_ALIGN_V_MIDDLE);
    CP_Settings_TextSize(drawSize);

    for (int row = 0; row < RAGE_ROWS; row++)
    {
        float y = ((float)row + 0.5f) * rowHeight;
        float rowWidth = 0.0f;
        float startX;
        float endX;
        float x;

        /* width of the whole row (approximate: one text size per letter). */
        for (int phrase = 0; phrase < RAGE_PHRASES_PER_ROW; phrase++)
        {
            rowWidth += (float)strlen(rageWords[PhraseIndex(row, phrase)]) * drawSize + RAGE_PHRASE_GAP;
        }

        /* each row slides from just off the right edge to fully off the left. */
        startX = screenW + (float)(row % 3) * RAGE_ROW_STAGGER;
        endX = -rowWidth - 50.0f;
        x = startX + (endX - startX) * progress;

        switch (row % 3)
        {
        case 0:  CP_Settings_Fill(CP_Color_Create(255, 225, 80, 255));  break;
        case 1:  CP_Settings_Fill(CP_Color_Create(100, 225, 255, 255)); break;
        default: CP_Settings_Fill(CP_Color_Create(255, 145, 185, 255)); break;
        }

        for (int phrase = 0; phrase < RAGE_PHRASES_PER_ROW; phrase++)
        {
            const char* text = rageWords[PhraseIndex(row, phrase)];
            float phraseWidth = (float)strlen(text) * drawSize;

            /* skip phrases that are fully off screen. */
            if (x < screenW && x + phraseWidth >= 0.0f)
            {
                CP_Font_DrawText(text, x, y);
            }

            x += phraseWidth + RAGE_PHRASE_GAP;
        }
    }

    CP_Settings_TextAlignment(CP_TEXT_ALIGN_H_CENTER, CP_TEXT_ALIGN_V_MIDDLE);
}