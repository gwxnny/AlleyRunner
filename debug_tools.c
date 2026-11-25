/*!
@file       : debug_tools.c
@author     : Isaac Tan (isaacd.tan@digipen.edu)
              (module written with AI assistance and reviewed by the team)
@team       : IsaacACat
@module     : CSD1401 Software Engineering Project 1
@section    : B
@project    : Alley Runner
@brief      : Implementation of the in-game debugging tools (see debug_tools.h).

All content © 2026 DigiPen Institute of Technology Singapore. All Rights Reserved.
*//*________________________________________________________________________________*/
#include "debug_tools.h"
#include "ui.h"

#include <stdio.h>

#define FRAME_HISTORY   120     /* frames shown in the frame-time graph */
#define GRAPH_MAX_MS    33.3f   /* top of the graph (30 FPS) */
#define TARGET_MS       16.7f   /* 60 FPS line */
#define PROFILE_SMOOTH  0.1f    /* how quickly the profiler bars follow new timings */

static bool overlayOn = false;
static bool godMode = false;
static bool frozen = false;
static bool stepFrame = false;
static DebugCommand pendingCommand = DEBUG_COMMAND_NONE;

static LARGE_INTEGER timerFrequency;
static LARGE_INTEGER sectionStart[DEBUG_SECTION_COUNT];
static float sectionMs[DEBUG_SECTION_COUNT];   /* smoothed milliseconds */

static float frameMs[FRAME_HISTORY];
static int frameIndex = 0;

static const char* sectionNames[DEBUG_SECTION_COUNT] =
{
    "Update",
    "Draw world",
    "Draw UI"
};

void Debug_Reset(void)
{
    godMode = false;
    frozen = false;
    stepFrame = false;
    pendingCommand = DEBUG_COMMAND_NONE;
}

void Debug_Update(void)
{
    stepFrame = false;

    /* Record how long the last frame took for the graph. */
    frameMs[frameIndex] = CP_System_GetDt() * 1000.0f;
    frameIndex = (frameIndex + 1) % FRAME_HISTORY;

    if (CP_Input_KeyTriggered(KEY_F1)) overlayOn = !overlayOn;
    if (CP_Input_KeyTriggered(KEY_F2)) godMode = !godMode;
    if (CP_Input_KeyTriggered(KEY_F3)) frozen = !frozen;
    if (CP_Input_KeyTriggered(KEY_F4) && frozen) stepFrame = true;
    if (CP_Input_KeyTriggered(KEY_F5)) pendingCommand = DEBUG_COMMAND_GIVE_POWERUP;
    if (CP_Input_KeyTriggered(KEY_F6)) pendingCommand = DEBUG_COMMAND_ADD_LIFE;
}

bool Debug_IsOverlayOn(void)     { return overlayOn; }
bool Debug_IsGodMode(void)       { return godMode; }
bool Debug_IsFrozen(void)        { return frozen; }
bool Debug_IsSteppingFrame(void) { return stepFrame; }

DebugCommand Debug_TakeCommand(void)
{
    DebugCommand command = pendingCommand;

    pendingCommand = DEBUG_COMMAND_NONE;
    return command;
}

void Debug_ProfileBegin(DebugSection section)
{
    if (timerFrequency.QuadPart == 0)
    {
        QueryPerformanceFrequency(&timerFrequency);
    }

    QueryPerformanceCounter(&sectionStart[section]);
}

void Debug_ProfileEnd(DebugSection section)
{
    LARGE_INTEGER now;
    float elapsedMs;

    if (timerFrequency.QuadPart == 0)
    {
        return;
    }

    QueryPerformanceCounter(&now);
    elapsedMs = (float)(now.QuadPart - sectionStart[section].QuadPart) * 1000.0f /
        (float)timerFrequency.QuadPart;

    /* Smooth the value so the bars are readable. */
    sectionMs[section] += (elapsedMs - sectionMs[section]) * PROFILE_SMOOTH;
}

void Debug_DrawBox(float centerX, float centerY, float w, float h, CP_Color color)
{
    CP_Settings_NoFill();
    CP_Settings_Stroke(color);
    CP_Settings_StrokeWeight(2.0f);
    CP_Settings_RectMode(CP_POSITION_CENTER);
    CP_Graphics_DrawRect(centerX, centerY, w, h);
    CP_Settings_NoStroke();
}

void Debug_DrawCircle(float centerX, float centerY, float radius, CP_Color color)
{
    CP_Settings_NoFill();
    CP_Settings_Stroke(color);
    CP_Settings_StrokeWeight(2.0f);
    CP_Graphics_DrawCircle(centerX, centerY, radius * 2.0f);
    CP_Settings_NoStroke();
}

void Debug_DrawOverlay(const char* const* lines, int lineCount)
{
    float scale = UI_GetScale();
    float rowH = 22.0f * scale;
    float left = 12.0f * scale;
    float top = 100.0f * scale;
    float panelW = 380.0f * scale;
    float graphH = 70.0f * scale;
    float panelH = rowH * (float)(lineCount + DEBUG_SECTION_COUNT + 3) + graphH + 20.0f * scale;
    float y;
    char text[96];
    CP_Font font = UI_GetBodyFont();

    CP_Settings_ResetMatrix();

    if (font != NULL)
    {
        CP_Font_Set(font);
    }

    /* Background panel. */
    CP_Settings_NoStroke();
    CP_Settings_RectMode(CP_POSITION_CORNER);
    CP_Settings_Fill(CP_Color_Create(0, 0, 0, 200));
    CP_Graphics_DrawRect(left, top, panelW, panelH);

    CP_Settings_TextAlignment(CP_TEXT_ALIGN_H_LEFT, CP_TEXT_ALIGN_V_MIDDLE);
    CP_Settings_TextSize(21.0f * scale);

    /* Header with FPS and active toggles. */
    y = top + rowH * 0.7f;
    snprintf(text, sizeof(text), "DEBUG  FPS %.0f  %s%s",
        (double)CP_System_GetFrameRate(),
        godMode ? "[GOD] " : "",
        frozen ? "[FROZEN - F4 step]" : "");
    CP_Settings_Fill(CP_Color_Create(120, 255, 120, 255));
    CP_Font_DrawText(text, left + 8.0f * scale, y);

    /* Live game values supplied by the caller. */
    CP_Settings_Fill(CP_Color_Create(235, 235, 235, 255));

    for (int i = 0; i < lineCount; i++)
    {
        y += rowH;
        CP_Font_DrawText(lines[i], left + 8.0f * scale, y);
    }

    /* Profiler bars: one per section, full bar = 16.7 ms (one 60 FPS frame). */
    y += rowH * 1.2f;
    CP_Settings_Fill(CP_Color_Create(255, 210, 80, 255));
    CP_Font_DrawText("Profiler (ms per frame)", left + 8.0f * scale, y);

    for (int s = 0; s < DEBUG_SECTION_COUNT; s++)
    {
        float barMax = 180.0f * scale;
        float barW = barMax * sectionMs[s] / TARGET_MS;

        if (barW > barMax)
        {
            barW = barMax;
        }

        y += rowH;
        snprintf(text, sizeof(text), "%-10s %5.2f", sectionNames[s], (double)sectionMs[s]);
        CP_Settings_Fill(CP_Color_Create(235, 235, 235, 255));
        CP_Font_DrawText(text, left + 8.0f * scale, y);

        CP_Settings_Fill(CP_Color_Create(80, 180, 255, 255));
        CP_Graphics_DrawRect(left + 180.0f * scale, y - 6.0f * scale, barW, 12.0f * scale);
    }

    /* Frame-time graph: each bar is one frame, green under 60 FPS budget, red over. */
    {
        float graphTop = y + rowH;
        float graphW = panelW - 16.0f * scale;
        float barW = graphW / (float)FRAME_HISTORY;
        float targetY = graphTop + graphH - graphH * (TARGET_MS / GRAPH_MAX_MS);

        for (int i = 0; i < FRAME_HISTORY; i++)
        {
            float ms = frameMs[(frameIndex + i) % FRAME_HISTORY];
            float barH = graphH * (ms > GRAPH_MAX_MS ? 1.0f : ms / GRAPH_MAX_MS);

            CP_Settings_Fill(ms > TARGET_MS + 1.0f ? CP_Color_Create(255, 80, 80, 255) : CP_Color_Create(120, 255, 120, 255));
            CP_Graphics_DrawRect(left + 8.0f * scale + (float)i * barW, graphTop + graphH - barH, barW, barH);
        }

        CP_Settings_Stroke(CP_Color_Create(255, 255, 255, 160));
        CP_Settings_StrokeWeight(1.0f);
        CP_Graphics_DrawLine(left + 8.0f * scale, targetY, left + 8.0f * scale + graphW, targetY);
        CP_Settings_NoStroke();
    }
}
