/*!
@file       : tuning.c
@author     : Isaac Tan (isaacd.tan@digipen.edu)
@team       : IsaacACat
@module     : CSD1401 Software Engineering Project 1
@section    : B
@project    : Alley Runner
@brief      : Implementation of the gameplay tuning file, hot reloading and
              the in-game tuning editor (see tuning.h).

All content © 2026 DigiPen Institute of Technology Singapore. All Rights Reserved.
*//*________________________________________________________________________________*/
#include "tuning.h"
#include "ui.h"
#include "cprocessing.h"

#include <stddef.h>   /* offsetof */
#include <stdio.h>
#include <stdlib.h>   /* strtof */
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>

#define RELOAD_CHECK_INTERVAL 0.5f  /* seconds between file change checks */
#define MESSAGE_TIME          2.0f  /* seconds a "saved / reloaded" message stays */
#define MAX_LINE_LENGTH       256
#define MAX_KEY_LENGTH        64

/* One editable value: where it lives in GameTuning and how it may change. */
typedef struct
{
    const char* key;          /* name used in the file */
    const char* label;        /* name shown in the editor */
    size_t offset;            /* offsetof(GameTuning, field) */
    float minValue;
    float maxValue;
    float step;               /* change per Left/Right press */
} TuningField;

static const GameTuning defaultTuning =
{
    600.0f,   /* startSpeed */
    5.0f,     /* speedRamp */
    450.0f,   /* obstacleSpacing */

    3.0f,  6.0f,    /* coin spawn */
    28.0f, 56.0f,   /* power-up spawn */
    5.0f,  8.0f,    /* heart spawn */
    20.0f, 50.0f,   /* multiplier spawn */

    5.0f,     /* invincibleTime */
    10.0f,    /* shieldTime */
    16.0f,    /* magnetTime */
    450.0f,   /* magnetRange */

    6.0f, 22.0f,    /* rage quiet time */
    6.0f            /* rageDuration */
};

static const TuningField fields[] =
{
    { "start_speed",          "Start speed",          offsetof(GameTuning, startSpeed),         100.0f, 2000.0f, 25.0f },
    { "speed_ramp",           "Speed gain / sec",     offsetof(GameTuning, speedRamp),            0.0f,   50.0f,  0.5f },
    { "obstacle_spacing",     "Obstacle spacing",     offsetof(GameTuning, obstacleSpacing),    200.0f, 1500.0f, 10.0f },
    { "coin_spawn_min",       "Coin spawn min (s)",   offsetof(GameTuning, coinSpawnMin),         0.5f,   30.0f,  0.5f },
    { "coin_spawn_max",       "Coin spawn max (s)",   offsetof(GameTuning, coinSpawnMax),         0.5f,   30.0f,  0.5f },
    { "powerup_spawn_min",    "Mystery box min (s)",  offsetof(GameTuning, powerUpSpawnMin),      1.0f,  120.0f,  1.0f },
    { "powerup_spawn_max",    "Mystery box max (s)",  offsetof(GameTuning, powerUpSpawnMax),      1.0f,  120.0f,  1.0f },
    { "heart_spawn_min",      "Heart spawn min (s)",  offsetof(GameTuning, heartSpawnMin),        1.0f,   60.0f,  0.5f },
    { "heart_spawn_max",      "Heart spawn max (s)",  offsetof(GameTuning, heartSpawnMax),        1.0f,   60.0f,  0.5f },
    { "multiplier_spawn_min", "Multiplier min (s)",   offsetof(GameTuning, multiplierSpawnMin),   1.0f,  120.0f,  1.0f },
    { "multiplier_spawn_max", "Multiplier max (s)",   offsetof(GameTuning, multiplierSpawnMax),   1.0f,  120.0f,  1.0f },
    { "invincible_time",      "Invincible time (s)",  offsetof(GameTuning, invincibleTime),       0.5f,   30.0f,  0.5f },
    { "shield_time",          "Shield time (s)",      offsetof(GameTuning, shieldTime),           0.5f,   30.0f,  0.5f },
    { "magnet_time",          "Magnet time (s)",      offsetof(GameTuning, magnetTime),           0.5f,   30.0f,  0.5f },
    { "magnet_range",         "Magnet range",         offsetof(GameTuning, magnetRange),         50.0f, 1500.0f, 25.0f },
    { "rage_quiet_min",       "Rage quiet min (s)",   offsetof(GameTuning, rageQuietMin),         1.0f,  120.0f,  1.0f },
    { "rage_quiet_max",       "Rage quiet max (s)",   offsetof(GameTuning, rageQuietMax),         1.0f,  120.0f,  1.0f },
    { "rage_duration",        "Rage duration (s)",    offsetof(GameTuning, rageDuration),         0.5f,   20.0f,  0.5f }
};

#define FIELD_COUNT ((int)(sizeof(fields) / sizeof(fields[0])))

static GameTuning tuning;
static bool initialized = false;

static bool editorOpen = false;
static int selectedField = 0;

static float reloadCheckTimer = 0.0f;
static time_t lastFileTime = 0;

static char message[96] = "";
static float messageTimer = 0.0f;

/* ---------------------------------------------------------------- helpers */

static float* FieldValue(int index)
{
    return (float*)((char*)&tuning + fields[index].offset);
}

static void ShowMessage(const char* text)
{
    snprintf(message, sizeof(message), "%s", text);
    messageTimer = MESSAGE_TIME;
}

/* fopen is flagged as unsafe by MSVC's SDL checks, so use fopen_s there. */
static FILE* OpenTuningFile(const char* path, const char* mode)
{
#ifdef _MSC_VER
    FILE* file = NULL;

    if (fopen_s(&file, path, mode) != 0)
    {
        return NULL;
    }

    return file;
#else
    return fopen(path, mode);
#endif
}

/* Last modified time of the tuning file, or 0 if it does not exist. */
static time_t GetTuningFileTime(void)
{
#ifdef _MSC_VER
    struct _stat info;

    if (_stat(TUNING_FILE_PATH, &info) != 0)
    {
        return 0;
    }
#else
    struct stat info;

    if (stat(TUNING_FILE_PATH, &info) != 0)
    {
        return 0;
    }
#endif

    return info.st_mtime;
}

/* Keeps every value in range and every min below its max. */
static void Validate(void)
{
    for (int i = 0; i < FIELD_COUNT; i++)
    {
        float* value = FieldValue(i);

        if (*value < fields[i].minValue)
        {
            *value = fields[i].minValue;
        }
        else if (*value > fields[i].maxValue)
        {
            *value = fields[i].maxValue;
        }
    }

    if (tuning.coinSpawnMax < tuning.coinSpawnMin)             tuning.coinSpawnMax = tuning.coinSpawnMin;
    if (tuning.powerUpSpawnMax < tuning.powerUpSpawnMin)       tuning.powerUpSpawnMax = tuning.powerUpSpawnMin;
    if (tuning.heartSpawnMax < tuning.heartSpawnMin)           tuning.heartSpawnMax = tuning.heartSpawnMin;
    if (tuning.multiplierSpawnMax < tuning.multiplierSpawnMin) tuning.multiplierSpawnMax = tuning.multiplierSpawnMin;
    if (tuning.rageQuietMax < tuning.rageQuietMin)             tuning.rageQuietMax = tuning.rageQuietMin;
}

/* Removes spaces from both ends of a string, in place. */
static char* Trim(char* text)
{
    char* end;

    while (*text != '\0' && isspace((unsigned char)*text))
    {
        text++;
    }

    end = text + strlen(text);

    while (end > text && isspace((unsigned char)end[-1]))
    {
        end--;
    }

    *end = '\0';
    return text;
}

static int FindField(const char* key)
{
    for (int i = 0; i < FIELD_COUNT; i++)
    {
        if (strcmp(fields[i].key, key) == 0)
        {
            return i;
        }
    }

    return -1;
}

/* Reads "key = value" lines. Unknown keys and bad lines are skipped,
   so a typo in the file never crashes the game. */
static bool LoadFile(void)
{
    char line[MAX_LINE_LENGTH];
    FILE* file = OpenTuningFile(TUNING_FILE_PATH, "r");

    if (file == NULL)
    {
        return false;
    }

    while (fgets(line, (int)sizeof(line), file) != NULL)
    {
        char* comment = strchr(line, '#');
        char* equals;
        char* key;
        char* valueText;
        char* parseEnd;
        float value;
        int index;

        if (comment != NULL)
        {
            *comment = '\0';
        }

        equals = strchr(line, '=');

        if (equals == NULL)
        {
            continue;
        }

        *equals = '\0';
        key = Trim(line);
        valueText = Trim(equals + 1);
        index = FindField(key);

        if (index < 0)
        {
            continue;
        }

        value = strtof(valueText, &parseEnd);

        if (parseEnd != valueText)
        {
            *FieldValue(index) = value;
        }
    }

    fclose(file);
    Validate();
    return true;
}

static bool SaveFile(void)
{
    FILE* file = OpenTuningFile(TUNING_FILE_PATH, "w");

    if (file == NULL)
    {
        return false;
    }

    fprintf(file, "# Alley Runner gameplay tuning.\n");
    fprintf(file, "# Edit and save this file while the game is running: changes load automatically.\n");
    fprintf(file, "# Or press F7 in a run to use the in-game editor (F8 saves here).\n\n");

    for (int i = 0; i < FIELD_COUNT; i++)
    {
        fprintf(file, "%-22s = %g    # %s (%g to %g)\n",
            fields[i].key, (double)*FieldValue(i), fields[i].label,
            (double)fields[i].minValue, (double)fields[i].maxValue);
    }

    fclose(file);

    /* Our own save should not count as an outside edit. */
    lastFileTime = GetTuningFileTime();
    return true;
}

/* ---------------------------------------------------------------- public */

void Tuning_Init(void)
{
    if (initialized)
    {
        return;
    }

    initialized = true;
    tuning = defaultTuning;

    if (LoadFile())
    {
        lastFileTime = GetTuningFileTime();
    }
    else
    {
        /* No file yet: write one so designers have something to edit. */
        SaveFile();
    }
}

const GameTuning* Tuning_Get(void)
{
    return &tuning;
}

bool Tuning_IsEditorOpen(void)
{
    return editorOpen;
}

void Tuning_Update(float dt)
{
    /* Hot reload: check the file's modified time a few times per second. */
    reloadCheckTimer -= dt;

    if (reloadCheckTimer <= 0.0f)
    {
        time_t fileTime = GetTuningFileTime();

        reloadCheckTimer = RELOAD_CHECK_INTERVAL;

        if (fileTime != 0 && fileTime != lastFileTime)
        {
            lastFileTime = fileTime;

            if (LoadFile())
            {
                ShowMessage("Tuning file reloaded");
            }
        }
    }

    if (messageTimer > 0.0f)
    {
        messageTimer -= dt;
    }

    if (CP_Input_KeyTriggered(KEY_F7))
    {
        editorOpen = !editorOpen;
    }

    if (!editorOpen)
    {
        return;
    }

    if (CP_Input_KeyTriggered(KEY_UP))
    {
        selectedField = (selectedField + FIELD_COUNT - 1) % FIELD_COUNT;
    }

    if (CP_Input_KeyTriggered(KEY_DOWN))
    {
        selectedField = (selectedField + 1) % FIELD_COUNT;
    }

    {
        float direction = 0.0f;
        bool fast = CP_Input_KeyDown(KEY_LEFT_SHIFT) || CP_Input_KeyDown(KEY_RIGHT_SHIFT);

        if (CP_Input_KeyTriggered(KEY_LEFT))
        {
            direction = -1.0f;
        }
        else if (CP_Input_KeyTriggered(KEY_RIGHT))
        {
            direction = 1.0f;
        }

        if (direction != 0.0f)
        {
            *FieldValue(selectedField) += direction * fields[selectedField].step * (fast ? 10.0f : 1.0f);
            Validate();
        }
    }

    if (CP_Input_KeyTriggered(KEY_F8))
    {
        ShowMessage(SaveFile() ? "Saved to " TUNING_FILE_PATH : "Could not save tuning file");
    }

    if (CP_Input_KeyTriggered(KEY_F9))
    {
        tuning = defaultTuning;
        ShowMessage("Defaults restored (F8 to save)");
    }
}

void Tuning_Draw(void)
{
    float scale = UI_GetScale();
    CP_Font font = UI_GetBodyFont();

    if (font != NULL)
    {
        CP_Font_Set(font);
    }

    CP_Settings_ResetMatrix();
    CP_Settings_NoStroke();

    if (editorOpen)
    {
        float rowH = 24.0f * scale;
        float panelW = 420.0f * scale;
        float panelH = (float)(FIELD_COUNT + 4) * rowH;
        float left = (float)CP_System_GetWindowWidth() - panelW - 16.0f * scale;
        float top = 100.0f * scale;
        char text[96];

        CP_Settings_RectMode(CP_POSITION_CORNER);
        CP_Settings_Fill(CP_Color_Create(10, 10, 20, 225));
        CP_Graphics_DrawRect(left, top, panelW, panelH);

        CP_Settings_TextAlignment(CP_TEXT_ALIGN_H_LEFT, CP_TEXT_ALIGN_V_MIDDLE);
        CP_Settings_TextSize(22.0f * scale);
        CP_Settings_Fill(CP_Color_Create(255, 210, 80, 255));
        CP_Font_DrawText("TUNING EDITOR (F7 close)", left + 10.0f * scale, top + rowH * 0.6f);

        CP_Settings_TextSize(19.0f * scale);
        CP_Settings_Fill(CP_Color_Create(190, 190, 200, 255));
        CP_Font_DrawText("Up/Down pick  Left/Right change  Shift x10  F8 save  F9 defaults",
            left + 10.0f * scale, top + rowH * 1.5f);

        for (int i = 0; i < FIELD_COUNT; i++)
        {
            float rowY = top + rowH * (2.6f + (float)i);

            if (i == selectedField)
            {
                CP_Settings_Fill(CP_Color_Create(225, 75, 95, 160));
                CP_Graphics_DrawRect(left + 4.0f * scale, rowY - rowH * 0.5f, panelW - 8.0f * scale, rowH);
            }

            CP_Settings_TextSize(21.0f * scale);
            CP_Settings_Fill(CP_Color_Create(245, 245, 245, 255));
            CP_Font_DrawText(fields[i].label, left + 12.0f * scale, rowY);

            snprintf(text, sizeof(text), "%g", (double)*FieldValue(i));
            CP_Settings_TextAlignment(CP_TEXT_ALIGN_H_RIGHT, CP_TEXT_ALIGN_V_MIDDLE);
            CP_Font_DrawText(text, left + panelW - 12.0f * scale, rowY);
            CP_Settings_TextAlignment(CP_TEXT_ALIGN_H_LEFT, CP_TEXT_ALIGN_V_MIDDLE);
        }
    }

    if (messageTimer > 0.0f)
    {
        CP_Settings_TextAlignment(CP_TEXT_ALIGN_H_CENTER, CP_TEXT_ALIGN_V_MIDDLE);
        CP_Settings_TextSize(26.0f * scale);
        CP_Settings_Fill(CP_Color_Create(255, 210, 80, 255));
        CP_Font_DrawText(message, (float)CP_System_GetWindowWidth() * 0.5f,
            (float)CP_System_GetWindowHeight() - 30.0f * scale);
    }
}