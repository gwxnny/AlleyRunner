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

/* Original window height used to design the obstacle sizes. */
#define BASE_GAME_HEIGHT 720.0f
static float gameScale = 1.0f;

/* Sprite-sheet frame dimensions. */
#define PLAYER_W 120.0f
#define PLAYER_H 120.0f

#define ENEMY_W 500.0f
#define ENEMY_H 500.0f
#define BALL_Y_OFFSET 140.0f

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
#define RAGE_DRAW_SIZE 64.0f
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
static int highScore = 0;

static const float pinYOffset[NUM_LANES] =
{
    50.0f,
    0.0f,
    -50.0f
};

//static const float obsYOffset[NUM_LANES] =
//{
//    120.0f,
//    65.0f,
//    10.0f
//};

// smaller value = higher on the screen, larger value = lower on the screen.
static const float obstacleLaneCenter[NUM_LANES] =
{
    0.28f,  // top lane
    0.55f,  // middle lane
    0.82f   // bottom lane
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
    if (a->frameTime <= 0.0f || a->frameCount <= 0)
    {
        return;
    }

	// if the animation is not looping and has reached the last frame, do not continue updating the frame index.
    if (!a->loop &&
        a->currentFrame == a->frameCount - 1)
    {
        return;
    }

    a->timer += CP_System_GetDt();

    while (a->timer >= a->frameTime)
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
                a->timer = 0.0f;
                break;
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

static float PinFeetY(int laneIndex)
{
	// keep bottom lane aligned with the bottom of the screen, and scale the other lanes.
    float bottomFeet =
        top_lane_y +
        (float)(NUM_LANES - 1) * lane_gap +
        (
            FEET_OFFSET +
            pinYOffset[NUM_LANES - 1]
            ) * gameScale;

    float laneDifference =
        obstacleLaneCenter[laneIndex] -
        obstacleLaneCenter[NUM_LANES - 1];

    return bottomFeet + (float)height * laneDifference;
}

// x is the obstacle's left edge; ground is its bottom edge.
static void DrawObstacle(int type, float x, int laneIndex)
{
    Animation* a = &obsAnim[type];

    float centerY =
        (float)height * obstacleLaneCenter[laneIndex];

    Anim_Draw(
        a,
        x + a->drawW / 2.0f,
        centerY
    );
}

static void RandomiseObstacle(int i)
{
    obs_type[i] = CP_Random_RangeInt(0, NUM_OBS_TYPES - 1);
    obs_lane[i] = CP_Random_RangeInt(0, NUM_LANES - 1);
}

int Game_GetHighScore(void)
{
    return highScore;
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
    enemyGround = PinFeetY(lane);

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

	// scale the text size based on the smaller of the width and height ratios to maintain aspect ratio
    float textScale = fminf(
        screenW / 1280.0f,
        screenH / 720.0f
    );

    float drawSize = RAGE_DRAW_SIZE * textScale;
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

    CP_Settings_TextSize(drawSize);

    for (int row = 0; row < RAGE_ROWS; row++)
    {
        float y = ((float)row + 0.5f) * rowHeight;
        float rowWidth = 0.0f;

        for (int phrase = 0;
            phrase < RAGE_PHRASES_PER_ROW;
            phrase++)
        {
            int index = (row * 5 + phrase * 7) % RAGE_WORD_COUNT;

            rowWidth += (float)strlen(rageWords[index]) * drawSize;

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
            int index = (row * 5 + phrase * 7) % RAGE_WORD_COUNT;

            const char* text = rageWords[index];

            float phraseWidth = (float)strlen(text) * drawSize;

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
    CP_System_SetWindowTitle("ALLEY RUNNER - BUILD CHECK 123");
    CP_Settings_NoStroke();

    CP_Settings_ImageMode(CP_POSITION_CENTER);
    CP_Settings_RectMode(CP_POSITION_CENTER);

    width = CP_System_GetWindowWidth();
    height = CP_System_GetWindowHeight();

    /* Scale obstacles relative to the original 720-pixel height. */
    gameScale = (float)height / BASE_GAME_HEIGHT;

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

    /* scale character display sizes with the window height. */
    Anim_SetDrawSize(
        &runAnim,
        PLAYER_W * gameScale,
        PLAYER_H * gameScale
    );

    Anim_SetDrawSize(
        &chaseAnim,
        ENEMY_W * gameScale,
        ENEMY_H * gameScale
    );

    Anim_SetDrawSize(
        &grabAnim,
        GRAB_W * gameScale,
        GRAB_H * gameScale
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

        // limit the obstacle height to 38% of a lane's height.
        float obstacleHeight = lane_gap * 0.38f;
        float obstacleWidth =
            obstacleHeight * (OBS_SRC_W / OBS_SRC_H);

        Anim_SetDrawSize(
            &obsAnim[t],
            obstacleWidth,
            obstacleHeight
        );
    }

    /* Scale initial spacing along with the obstacle sizes. */
    for (int i = 0; i < MAX_OBS; i++)
    {
        obs_x[i] =
            (float)width +
            (float)i * OBS_SPACING * gameScale;

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
            obs_lane[i]
        );
    }

    if (state == STATE_GRAB)
    {
        Anim_Draw(
            &grabAnim,
            enemyX +
            (GRAB_W / 2.0f - ENEMY_BODY_X) * gameScale,
            enemyGround - grabAnim.drawH / 2.0f
        );
    }
    else
    {
        float pinFeetY = PinFeetY(lane);

        Anim_Draw(
            &runAnim,
            player_x,
            pinFeetY - runAnim.drawH / 2.0f
        );

		// keep the ball anchored at its existing position.
        Anim_Draw(
            &chaseAnim,
            enemyX,
            enemyGround +
            BALL_Y_OFFSET * gameScale -
            chaseAnim.drawH / 2.0f
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
    float screenW = (float)CP_System_GetWindowWidth();
    float screenH = (float)CP_System_GetWindowHeight();

    float centerX = screenW * 0.5f;
    float centerY = screenH * 0.5f;

    float uiScale = fminf(
        screenW / 1280.0f,
        screenH / 720.0f
    );

    float buttonW = 240.0f * uiScale;
    float buttonH = 70.0f * uiScale;
    float buttonGap = 30.0f * uiScale;

    float restartX =
        centerX - (buttonW + buttonGap) / 2.0f;

    float menuX =
        centerX + (buttonW + buttonGap) / 2.0f;

    float buttonY = centerY + 60.0f * uiScale;

    CP_Settings_RectMode(CP_POSITION_CENTER);
    CP_Settings_NoStroke();

    /* Dim the frozen gameplay. */
    CP_Settings_Fill(CP_Color_Create(0, 0, 0, 165));

    CP_Graphics_DrawRect(
        centerX,
        centerY,
        screenW,
        screenH
    );

    /* Centered menu panel. */
    CP_Settings_Fill(CP_Color_Create(20, 20, 25, 245));

    CP_Graphics_DrawRect(
        centerX,
        centerY,
        620.0f * uiScale,
        300.0f * uiScale
    );

    if (deathFont != NULL)
    {
        CP_Font_Set(deathFont);
    }

    CP_Settings_TextAlignment(
        CP_TEXT_ALIGN_H_CENTER,
        CP_TEXT_ALIGN_V_MIDDLE
    );

    CP_Settings_TextSize(80.0f * uiScale);
    CP_Settings_Fill(CP_Color_Create(255, 255, 255, 255));

    CP_Font_DrawText(
        "YOU DIED",
        centerX,
        centerY - 65.0f * uiScale
    );

    /* Restart button. */
    CP_Settings_Fill(CP_Color_Create(100, 220, 100, 255));

    CP_Graphics_DrawRect(
        restartX,
        buttonY,
        buttonW,
        buttonH
    );

    CP_Settings_Fill(CP_Color_Create(0, 0, 0, 255));
    CP_Settings_TextSize(40.0f * uiScale);

    CP_Font_DrawText("RESTART", restartX, buttonY);

    /* Return-to-menu button. */
    CP_Settings_Fill(CP_Color_Create(255, 80, 80, 255));

    CP_Graphics_DrawRect(
        menuX,
        buttonY,
        buttonW,
        buttonH
    );

    CP_Settings_Fill(CP_Color_Create(0, 0, 0, 255));

    CP_Font_DrawText("MAIN MENU", menuX, buttonY);

    if (CP_Input_MouseTriggered(MOUSE_BUTTON_LEFT))
    {
        float mouseX = CP_Input_GetMouseX();
        float mouseY = CP_Input_GetMouseY();

        bool overRestart =
            fabsf(mouseX - restartX) <= buttonW / 2.0f &&
            fabsf(mouseY - buttonY) <= buttonH / 2.0f;

        bool overMenu =
            fabsf(mouseX - menuX) <= buttonW / 2.0f &&
            fabsf(mouseY - buttonY) <= buttonH / 2.0f;

        if (overRestart)
        {
            CP_Engine_SetNextGameStateForced(
                Game_Init,
                Game_Update,
                Game_Exit
            );

            return;
        }

        if (overMenu)
        {
            CP_Engine_SetNextGameStateForced(
                Main_Menu_Init,
                Main_Menu_Update,
                Main_Menu_Exit
            );

            return;
        }
    }
}

/* ============================================================
   Update
   ============================================================ */

void Game_Update(void)
{
    float dt = CP_System_GetDt();

    if (CP_Input_KeyTriggered(KEY_Q))
    {
        CP_Engine_SetNextGameStateForced(
            Main_Menu_Init,
            Main_Menu_Update,
            Main_Menu_Exit
        );

        return;
    }

    if (CP_Input_KeyTriggered(KEY_ESCAPE))
    {
        CP_Engine_Terminate();
        return;
    }

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

        int currentScore = (int)score;

        if (currentScore > highScore)
        {
            highScore = currentScore;
        }

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

            //obs_x[i] -= speed * dt;
			// convert base gameplay speed to screen pixels per second. (to have consistent gameplay speed across different window sizes)
            obs_x[i] -= speed * gameScale * dt;

            /* swept collision check */ 
            float playerHalfWidth = runAnim.drawW * 0.30f;
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
                    OBS_SPACING * gameScale * (speed / START_SPEED);

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

        float targetX = player_x - GRAB_REACH * gameScale;
        float targetY = PinFeetY(lane);

        float dx = targetX - enemyX;
        float dy = targetY - enemyGround;

        float dist = sqrtf(dx * dx + dy * dy);
        //float step = ENEMY_WALK_SPEED * dt;
        float step = ENEMY_WALK_SPEED * gameScale * dt;

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
}

/* ============================================================
   Cleanup
   ============================================================ */

void Game_Exit(void)
{
    CP_Sound_StopAll();

    if (sndJump != NULL)
    {
        CP_Sound_Free(sndJump);
        sndJump = NULL;
    }

    if (gameMusic != NULL)
    {
        CP_Sound_Free(gameMusic);
        gameMusic = NULL;
    }

    if (sndDeath != NULL)
    {
        CP_Sound_Free(sndDeath);
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