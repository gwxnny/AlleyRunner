#include "cprocessing.h"
#include "game.h"
#include "mainmenu.h"
#include "utils.h"

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/* ============================================================
   Gameplay audio
   ============================================================ */

static CP_Sound sndJump = NULL;
static CP_Sound gameMusic = NULL;
static CP_Sound sndDeath = NULL;

#define JUMP_VOL  1.0f
#define DEATH_VOL 1.0f

/* ============================================================
   Gameplay settings
   ============================================================ */

#define NUM_LANES 3

#define MAX_OBS       4
#define NUM_OBS_TYPES 3
#define OBS_SPACING   450.0f

/* Obstacle source-image dimensions. */
#define OBS_SRC_W 700.0f
#define OBS_SRC_H 500.0f

/* Obstacle display dimensions. */
#define OBS_DRAW_W 192.0f
#define OBS_DRAW_H 192.0f

/* Sprite-sheet frame dimensions. */
#define PLAYER_W 120.0f
#define PLAYER_H 120.0f

#define ENEMY_W 500.0f
#define ENEMY_H 500.0f

#define GRAB_W 288.0f
#define GRAB_H 192.0f

#define ENEMY_BODY_X 96.0f
#define GRAB_REACH   130.0f

#define ENEMY_WALK_SPEED 250.0f

#define START_SPEED 800.0f
#define SPEED_RAMP  20.0f

#define FEET_OFFSET 80.0f

/* Death-screen layout. */
#define BTN_W     300.0f
#define BTN_H     200.0f
#define RESTART_X 400.0f
#define EXIT_X    1200.0f
#define BTN_Y     400.0f

/* ============================================================
   Rage overlay
   ============================================================ */

static CP_Font rageFont = NULL;

#define RAGE_QUIET_MIN_MS 6000
#define RAGE_QUIET_MAX_MS 22000

#define RAGE_EVENT_DURATION 6.0f
#define RAGE_DRAW_SIZE 46.0f
#define RAGE_ROWS 20
#define RAGE_PHRASES_PER_ROW 8
#define RAGE_PHRASE_GAP 0.0f

static float rageSpawnTimer = 0.0f;
static bool rageEventActive = false;
static float rageEventTimer = 0.0f;

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

#define RAGE_WORD_COUNT \
    ((int)(sizeof(rageWords) / sizeof(rageWords[0])))

/* ============================================================
   Types
   ============================================================ */

typedef enum
{
    STATE_PLAYING,
    STATE_ENEMY_WALK,
    STATE_GRAB
} GameplayPhase;

typedef struct
{
    CP_Image sheet;

    int frameCount;

    /* Frame size inside the sprite sheet. */
    float frameW;
    float frameH;

    /* Size drawn on the screen. */
    float drawW;
    float drawH;

    int currentFrame;

    float timer;
    float frameTime;

    int loop;
} Animation;

/* ============================================================
   Gameplay data
   ============================================================ */

int width;
int height;

float speed;

int lane;
float lane_gap;
float top_lane_y;
float player_x;

float enemyX;
float enemyGround;

Animation runAnim;
Animation chaseAnim;
Animation grabAnim;

static GameplayPhase state;

/* Background and scoreboard. */
static CP_Image bgImage = NULL;

static CP_Font scoreFont = NULL;
static CP_Font deathFont = NULL;

static int distance = 0;
static float score = 0.0f;
static float distanceTimer = 0.0f;

static const float pinYOffset[NUM_LANES] =
{
    50.0f,
    0.0f,
    -50.0f
};

static const float obsYOffset[NUM_LANES] =
{
    120.0f,
    65.0f,
    10.0f
};

/* Obstacles. */
float obs_x[MAX_OBS];
int obs_lane[MAX_OBS];

static int obs_type[MAX_OBS];
static Animation obsAnim[NUM_OBS_TYPES];

static const char* obsPaths[NUM_OBS_TYPES] =
{
    "../Assets/barrier.png",
    "../Assets/Oil_Spill.png",
    "../Assets/hole.png"
};

/* ============================================================
   Animation helpers
   ============================================================ */

void Anim_Init(
    Animation* a,
    const char* path,
    int frameCount,
    float frameW,
    float frameH,
    float frameTime,
    int loop)
{
    a->sheet = CP_Image_Load(path);

    a->frameCount = frameCount;

    a->frameW = frameW;
    a->frameH = frameH;

    a->drawW = frameW;
    a->drawH = frameH;

    a->currentFrame = 0;

    a->timer = 0.0f;
    a->frameTime = frameTime;

    a->loop = loop;
}

static void Anim_SetDrawSize(Animation* a, float w, float h)
{
    a->drawW = w;
    a->drawH = h;
}

void Anim_Reset(Animation* a)
{
    a->currentFrame = 0;
    a->timer = 0.0f;
}

void Anim_Update(Animation* a)
{
    a->timer += CP_System_GetDt();

    if (a->timer >= a->frameTime)
    {
        a->timer -= a->frameTime;
        a->currentFrame++;

        if (a->currentFrame >= a->frameCount)
        {
            if (a->loop)
            {
                a->currentFrame = 0;
            }
            else
            {
                a->currentFrame = a->frameCount - 1;
            }
        }
    }
}

// x and y are the centre of the sprite.
void Anim_Draw(Animation* a, float x, float y)
{
    if (a->sheet == NULL)
    {
        return;
    }

    float left = (float)a->currentFrame * a->frameW;

    CP_Image_DrawSubImage(
        a->sheet,
        x,
        y,
        a->drawW,
        a->drawH,
        left,
        0.0f,
        left + a->frameW,
        a->frameH,
        255
    );
}

/* ============================================================
   Game helpers
   ============================================================ */

float LaneGround(int l)
{
    return top_lane_y + (float)l * lane_gap + FEET_OFFSET;
}

// x is the obstacle's left edge; ground is its bottom edge.
static void DrawObstacle(int type, float x, float ground)
{
    Animation* a = &obsAnim[type];

    Anim_Draw(
        a,
        x + a->drawW / 2.0f,
        ground - a->drawH / 2.0f
    );
}

static void RandomiseObstacle(int i)
{
    obs_type[i] = CP_Random_RangeInt(0, NUM_OBS_TYPES - 1);
    obs_lane[i] = CP_Random_RangeInt(0, NUM_LANES - 1);
}

void StartRun(void)
{
    state = STATE_PLAYING;

    lane = 1;
    speed = START_SPEED;

    score = 0.0f;
    distance = 0;
    distanceTimer = 0.0f;

    enemyX = (float)width * 0.12f;
    enemyGround = LaneGround(lane);

    Anim_Reset(&runAnim);
    Anim_Reset(&chaseAnim);
    Anim_Reset(&grabAnim);
}

static void KillPlayer(void)
{
    state = STATE_ENEMY_WALK;

    if (sndDeath != NULL)
    {
        CP_Sound_PlayAdvanced(
            sndDeath,
            DEATH_VOL,
            1.0f,
            false,
            CP_SOUND_GROUP_SFX
        );
    }

    CP_Sound_StopGroup(CP_SOUND_GROUP_MUSIC);
}

/* ============================================================
   Rage overlay helpers
   ============================================================ */

static void RageOverlay_Reset(void)
{
    rageEventActive = false;
    rageEventTimer = 0.0f;

    rageSpawnTimer =
        (float)CP_Random_RangeInt(
            RAGE_QUIET_MIN_MS,
            RAGE_QUIET_MAX_MS
        ) / 1000.0f;
}

static void RageOverlay_Update(float dt)
{
    if (dt < 0.0f)
    {
        dt = 0.0f;
    }
    else if (dt > 0.05f)
    {
        dt = 0.05f;
    }

    if (!rageEventActive)
    {
        rageSpawnTimer -= dt;

        if (rageSpawnTimer <= 0.0f)
        {
            rageEventActive = true;
            rageEventTimer = RAGE_EVENT_DURATION;
        }

        return;
    }

    rageEventTimer -= dt;

    if (rageEventTimer <= 0.0f)
    {
        rageEventActive = false;
        rageEventTimer = 0.0f;

        rageSpawnTimer =
            (float)CP_Random_RangeInt(
                RAGE_QUIET_MIN_MS,
                RAGE_QUIET_MAX_MS
            ) / 1000.0f;
    }
}

static void RageOverlay_Draw(void)
{
    if (!rageEventActive || rageFont == NULL)
    {
        return;
    }

    float screenW = (float)CP_System_GetWindowWidth();
    float screenH = (float)CP_System_GetWindowHeight();

    float elapsed = RAGE_EVENT_DURATION - rageEventTimer;
    float progress = elapsed / RAGE_EVENT_DURATION;

    if (progress < 0.0f)
    {
        progress = 0.0f;
    }
    else if (progress > 1.0f)
    {
        progress = 1.0f;
    }

    float rowHeight = screenH / (float)RAGE_ROWS;

    CP_Font_Set(rageFont);
    CP_Settings_NoStroke();

    CP_Settings_TextAlignment(
        CP_TEXT_ALIGN_H_LEFT,
        CP_TEXT_ALIGN_V_MIDDLE
    );

    CP_Settings_TextSize(RAGE_DRAW_SIZE);

    for (int row = 0; row < RAGE_ROWS; row++)
    {
        float y = ((float)row + 0.5f) * rowHeight;
        float rowWidth = 0.0f;

        for (int phrase = 0;
            phrase < RAGE_PHRASES_PER_ROW;
            phrase++)
        {
            int index =
                (row * 5 + phrase * 7) % RAGE_WORD_COUNT;

            rowWidth +=
                (float)strlen(rageWords[index]) * RAGE_DRAW_SIZE;

            rowWidth += RAGE_PHRASE_GAP;
        }

        float stagger = (float)(row % 3) * 120.0f;

        float startX = screenW + stagger;
        float endX = -rowWidth - 50.0f;

        float x = startX + (endX - startX) * progress;

        switch (row % 3)
        {
        case 0:
            CP_Settings_Fill(
                CP_Color_Create(255, 225, 80, 255)
            );
            break;

        case 1:
            CP_Settings_Fill(
                CP_Color_Create(100, 225, 255, 255)
            );
            break;

        default:
            CP_Settings_Fill(
                CP_Color_Create(255, 145, 185, 255)
            );
            break;
        }

        for (int phrase = 0;
            phrase < RAGE_PHRASES_PER_ROW;
            phrase++)
        {
            int index =
                (row * 5 + phrase * 7) % RAGE_WORD_COUNT;

            const char* text = rageWords[index];

            float phraseWidth =
                (float)strlen(text) * RAGE_DRAW_SIZE;

            if (x < screenW && x + phraseWidth >= 0.0f)
            {
                CP_Font_DrawText(text, x, y);
            }

            x += phraseWidth + RAGE_PHRASE_GAP;
        }
    }

    CP_Settings_TextAlignment(
        CP_TEXT_ALIGN_H_CENTER,
        CP_TEXT_ALIGN_V_MIDDLE
    );
}

/* ============================================================
   Initialization
   ============================================================ */

void Game_Init(void)
{
    CP_Settings_NoStroke();

    CP_Settings_ImageMode(CP_POSITION_CENTER);
    CP_Settings_RectMode(CP_POSITION_CENTER);

    width = CP_System_GetWindowWidth();
    height = CP_System_GetWindowHeight();

    lane_gap = (float)height / 3.0f;
    top_lane_y = (float)height / 6.0f;

    player_x = (float)width * 0.4f;

    /* Rage overlay font. */
    rageFont = CP_Font_Load("../Assets/VT323-Regular.ttf");

    if (rageFont == NULL)
    {
        rageFont = CP_Font_Load("Assets/VT323-Regular.ttf");
    }

    /* Background. */
    bgImage = CP_Image_Load("../Assets/background.png");

    /* Gameplay audio. */
    sndJump = CP_Sound_Load("../Assets/jump.mp3");
    sndDeath = CP_Sound_Load("../Assets/game_over.mp3");
    gameMusic = CP_Sound_Load("../Assets/bg_music.mp3");

    if (gameMusic != NULL)
    {
        CP_Sound_PlayAdvanced(
            gameMusic,
            1.0f,
            1.0f,
            true,
            CP_SOUND_GROUP_MUSIC
        );
    }

    // Updated character sprite sheets.
    Anim_Init(
        &runAnim,
        "../Assets/pixil-layer-Pin Running.png",
        4,
        PLAYER_W,
        PLAYER_H,
        0.15f,
        1
    );

    Anim_Init(
        &chaseAnim,
        "../Assets/pixil-layer-Ball Rolling (500).png",
        8,
        ENEMY_W,
        ENEMY_H,
        0.15f,
        1
    );

    Anim_Init(
        &grabAnim,
        "../Assets/enemy_grab.png",
        6,
        GRAB_W,
        GRAB_H,
        0.20f,
        0
    );

    /* Obstacle images. */
    for (int t = 0; t < NUM_OBS_TYPES; t++)
    {
        Anim_Init(
            &obsAnim[t],
            obsPaths[t],
            1,
            OBS_SRC_W,
            OBS_SRC_H,
            0.15f,
            1
        );

        Anim_SetDrawSize(
            &obsAnim[t],
            OBS_DRAW_W,
            OBS_DRAW_H
        );
    }

    /* Place obstacles beyond the right edge. */
    for (int i = 0; i < MAX_OBS; i++)
    {
        obs_x[i] =
            (float)width + (float)i * OBS_SPACING;

        RandomiseObstacle(i);
    }

    /* Score and death-screen fonts. */
    scoreFont =
        CP_Font_Load("../Assets/PressStart2P-Regular.ttf");

    if (scoreFont == NULL)
    {
        scoreFont =
            CP_Font_Load("Assets/PressStart2P-Regular.ttf");
    }

    deathFont = rageFont;

    StartRun();
    RageOverlay_Reset();
}

/* ============================================================
   World drawing
   ============================================================ */

static void Draw_World(void)
{
    CP_Graphics_ClearBackground(
        CP_Color_Create(255, 255, 255, 255)
    );

    CP_Settings_ImageMode(CP_POSITION_CENTER);

    if (bgImage != NULL)
    {
        CP_Image_Draw(
            bgImage,
            (float)width / 2.0f,
            (float)height / 2.0f,
            (float)width,
            (float)height,
            255
        );
    }

    for (int i = 0; i < MAX_OBS; i++)
    {
        DrawObstacle(
            obs_type[i],
            obs_x[i],
            LaneGround(obs_lane[i]) + obsYOffset[obs_lane[i]]
        );
    }

    if (state == STATE_GRAB)
    {
        Anim_Draw(
            &grabAnim,
            enemyX + (GRAB_W / 2.0f - ENEMY_BODY_X),
            enemyGround - GRAB_H / 2.0f
        );
    }
    else
    {
        Anim_Draw(
            &runAnim,
            player_x,
            LaneGround(lane) +
            pinYOffset[lane] -
            PLAYER_H / 2.0f
        );

        Anim_Draw(
            &chaseAnim,
            enemyX,
            enemyGround - ENEMY_H / 2.0f + 150.0f
        );
    }

    /* Scoreboard. */
    if (scoreFont != NULL)
    {
        CP_Font_Set(scoreFont);
    }

    CP_Settings_NoStroke();

    CP_Settings_Fill(
        CP_Color_Create(255, 255, 255, 255)
    );

    CP_Settings_TextSize(30.0f);

    CP_Settings_TextAlignment(
        CP_TEXT_ALIGN_H_LEFT,
        CP_TEXT_ALIGN_V_TOP
    );

    CP_Font_DrawText("SCORE", 20.0f, 20.0f);

    char scoreText[32];

    snprintf(
        scoreText,
        sizeof(scoreText),
        "%05d",
        (int)score
    );

    CP_Font_DrawText(scoreText, 20.0f, 65.0f);

    snprintf(
        scoreText,
        sizeof(scoreText),
        "%04dm",
        distance
    );

    CP_Settings_TextAlignment(
        CP_TEXT_ALIGN_H_RIGHT,
        CP_TEXT_ALIGN_V_TOP
    );

    CP_Font_DrawText(
        scoreText,
        (float)width - 20.0f,
        50.0f
    );
}

/* ============================================================
   Death screen
   ============================================================ */

static void Draw_DeathScreen(void)
{
    if (deathFont != NULL)
    {
        CP_Font_Set(deathFont);
    }

    CP_Settings_RectMode(CP_POSITION_CENTER);
    CP_Settings_NoStroke();

    CP_Settings_Fill(
        CP_Color_Create(0, 0, 0, 255)
    );

    CP_Settings_TextSize(100.0f);

    CP_Settings_TextAlignment(
        CP_TEXT_ALIGN_H_CENTER,
        CP_TEXT_ALIGN_V_MIDDLE
    );

    CP_Font_DrawText("YOU DIED", 800.0f, 225.0f);

    /* Restart button. */
    CP_Settings_Fill(
        CP_Color_Create(100, 220, 100, 255)
    );

    CP_Graphics_DrawRect(
        RESTART_X,
        BTN_Y,
        BTN_W,
        BTN_H
    );

    CP_Settings_Fill(
        CP_Color_Create(0, 0, 0, 255)
    );

    CP_Settings_TextSize(80.0f);

    CP_Font_DrawText(
        "RESTART",
        RESTART_X,
        BTN_Y
    );

    /* Exit button. */
    CP_Settings_Fill(
        CP_Color_Create(255, 0, 0, 255)
    );

    CP_Graphics_DrawRect(
        EXIT_X,
        BTN_Y,
        BTN_W,
        BTN_H
    );

    CP_Settings_Fill(
        CP_Color_Create(0, 0, 0, 255)
    );

    CP_Settings_TextSize(100.0f);

    CP_Font_DrawText(
        "EXIT",
        EXIT_X,
        BTN_Y
    );

    if (CP_Input_MouseClicked())
    {
        float mouseX = CP_Input_GetMouseX();
        float mouseY = CP_Input_GetMouseY();

        if (IsAreaClicked(
            RESTART_X,
            BTN_Y,
            BTN_W,
            BTN_H,
            mouseX,
            mouseY))
        {
            CP_Engine_SetNextGameStateForced(
                Game_Init,
                Game_Update,
                Game_Exit
            );
        }
        else if (IsAreaClicked(
            EXIT_X,
            BTN_Y,
            BTN_W,
            BTN_H,
            mouseX,
            mouseY))
        {
            CP_Engine_SetNextGameStateForced(
                Main_Menu_Init,
                Main_Menu_Update,
                Main_Menu_Exit
            );
        }
    }
}

/* ============================================================
   Update
   ============================================================ */

void Game_Update(void)
{
    float dt = CP_System_GetDt();

    if (state == STATE_PLAYING)
    {
        /* Switching lanes. */
        if (CP_Input_KeyTriggered(KEY_W) && lane > 0)
        {
            lane--;

            if (sndJump != NULL)
            {
                CP_Sound_PlayAdvanced(
                    sndJump,
                    JUMP_VOL,
                    1.0f,
                    false,
                    CP_SOUND_GROUP_SFX
                );
            }
        }

        if (CP_Input_KeyTriggered(KEY_S) &&
            lane < NUM_LANES - 1)
        {
            lane++;

            if (sndJump != NULL)
            {
                CP_Sound_PlayAdvanced(
                    sndJump,
                    JUMP_VOL,
                    1.0f,
                    false,
                    CP_SOUND_GROUP_SFX
                );
            }
        }

        score += speed * dt / 100.0f;

        distanceTimer += dt;

        if (distanceTimer >= 1.0f)
        {
            distanceTimer -= 1.0f;
            distance++;
        }

        /* Gradually increase gameplay speed. */
        speed += SPEED_RAMP * dt;

        for (int t = 0; t < NUM_OBS_TYPES; t++)
        {
            Anim_Update(&obsAnim[t]);
        }

        /* Move and respawn obstacles. */
        for (int i = 0; i < MAX_OBS; i++)
        {
            float previousX = obs_x[i];
            float obsW = obsAnim[obs_type[i]].drawW;

            obs_x[i] -= speed * dt;

            /* swept collision check */ 
            float playerHalfWidth = PLAYER_W * 0.30f;
            float playerLeft = player_x - playerHalfWidth;
            float playerRight = player_x + playerHalfWidth;

            bool crossedPlayer =
                obs_x[i] <= playerRight &&
                previousX + obsW >= playerLeft;

            if (obs_lane[i] == lane && crossedPlayer)
            {
                KillPlayer();
                break;
            }

            /* Respawn behind the furthest obstacle. */
            if (obs_x[i] + obsW < 0.0f)
            {
                float furthest = (float)width;

                for (int j = 0; j < MAX_OBS; j++)
                {
                    if (obs_x[j] > furthest)
                    {
                        furthest = obs_x[j];
                    }
                }

                obs_x[i] =
                    furthest +
                    OBS_SPACING * (speed / START_SPEED);

                RandomiseObstacle(i);
            }
        }

        Anim_Update(&runAnim);
        Anim_Update(&chaseAnim);

        // Keep your rage overlay update.
        if (state == STATE_PLAYING)
        {
            RageOverlay_Update(dt);
        }
    }
    else if (state == STATE_ENEMY_WALK)
    {
        Anim_Update(&chaseAnim);

        float targetX = player_x - GRAB_REACH;
        float targetY = LaneGround(lane);

        float dx = targetX - enemyX;
        float dy = targetY - enemyGround;

        float dist = sqrtf(dx * dx + dy * dy);
        float step = ENEMY_WALK_SPEED * dt;

        if (dist <= step)
        {
            enemyX = targetX;
            enemyGround = targetY;

            state = STATE_GRAB;

            Anim_Reset(&grabAnim);
        }
        else
        {
            enemyX += dx / dist * step;
            enemyGround += dy / dist * step;
        }
    }
    else
    {
        Anim_Update(&grabAnim);
    }

    Draw_World();

    // Draw overlay over the gameplay.
    if (state == STATE_PLAYING)
    {
        RageOverlay_Draw();
    }

    bool grabDone =
        state == STATE_GRAB &&
        grabAnim.currentFrame == grabAnim.frameCount - 1;

    if (grabDone)
    {
        Draw_DeathScreen();
    }

    if (CP_Input_KeyDown(KEY_ESCAPE))
    {
        CP_Engine_Terminate();
    }

    if (CP_Input_KeyDown(KEY_Q))
    {
        CP_Engine_SetNextGameStateForced(
            Main_Menu_Init,
            Main_Menu_Update,
            Main_Menu_Exit
        );
    }
}

/* ============================================================
   Cleanup
   ============================================================ */

void Game_Exit(void)
{
    CP_Sound_StopAll();

    if (sndJump != NULL)
    {
        CP_Sound_Free(&sndJump);
        sndJump = NULL;
    }

    if (gameMusic != NULL)
    {
        CP_Sound_Free(&gameMusic);
        gameMusic = NULL;
    }

    if (sndDeath != NULL)
    {
        CP_Sound_Free(&sndDeath);
        sndDeath = NULL;
    }

    if (bgImage != NULL)
    {
        CP_Image_Free(bgImage);
        bgImage = NULL;
    }

    if (runAnim.sheet != NULL)
    {
        CP_Image_Free(runAnim.sheet);
        runAnim.sheet = NULL;
    }

    if (chaseAnim.sheet != NULL)
    {
        CP_Image_Free(chaseAnim.sheet);
        chaseAnim.sheet = NULL;
    }

    if (grabAnim.sheet != NULL)
    {
        CP_Image_Free(grabAnim.sheet);
        grabAnim.sheet = NULL;
    }

    for (int t = 0; t < NUM_OBS_TYPES; t++)
    {
        if (obsAnim[t].sheet != NULL)
        {
            CP_Image_Free(obsAnim[t].sheet);
            obsAnim[t].sheet = NULL;
        }
    }

    rageEventActive = false;
    rageEventTimer = 0.0f;
    rageSpawnTimer = 0.0f;

    rageFont = NULL;
    scoreFont = NULL;
    deathFont = NULL;
}