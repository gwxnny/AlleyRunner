#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include "cprocessing.h"
#include "game.h"
#include "mainmenu.h"
#include "utils.h"
#include "telemetry.h"

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/* UI FIX: map the native client-area mouse position to drawing coordinates. */
static void GetUIMouse(float* x, float* y)
{
    HWND window = CP_System_GetWindowHandle();
    POINT cursor;
    RECT client;

    *x = CP_Input_GetMouseX();
    *y = CP_Input_GetMouseY();

    if (window != NULL && GetCursorPos(&cursor) &&
        ScreenToClient(window, &cursor) && GetClientRect(window, &client))
    {
        int clientW = client.right - client.left;
        int clientH = client.bottom - client.top;

        if (clientW > 0 && clientH > 0)
        {
            *x = (float)cursor.x * (float)CP_System_GetWindowWidth() / (float)clientW;
            *y = (float)cursor.y * (float)CP_System_GetWindowHeight() / (float)clientH;
        }
    }
}

/* UI FIX: use the full centre-based rectangle, not a highlight strip. */
static int GameMouseInButton(float x, float y, float w, float h)
{
    float mouseX, mouseY;
    GetUIMouse(&mouseX, &mouseY);
    return mouseX >= x - w * 0.5f && mouseX <= x + w * 0.5f &&
        mouseY >= y - h * 0.5f && mouseY <= y + h * 0.5f;
}

/* ============================================================
   Gameplay audio
   ============================================================ */

static CP_Sound sndJump = NULL;
static CP_Sound sndCoin = NULL;
static CP_Sound gameMusic = NULL;
static CP_Sound sndDeath = NULL;

#define JUMP_VOL  1.0f
#define DEATH_VOL 1.0f
#define COIN_VOL 1.0f

/* ============================================================
   Gameplay settings
   ============================================================ */

#define NUM_LANES 3

#define MAX_OBS       4
#define NUM_OBS_TYPES 3
#define OBS_SPACING   450.0f

   /* Obstacle source-image dimensions. */
#define OBS_SRC_W 500.0f
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

#define ENEMY_W 350.0f
#define ENEMY_H 350.0f
#define BALL_Y_OFFSET 140.0f

#define GRAB_W 1000.0f
#define GRAB_H 500.0f

#define ENEMY_BODY_X 96.0f
#define GRAB_REACH   130.0f

#define ENEMY_WALK_SPEED 250.0f

#define START_SPEED 800.0f
#define SPEED_RAMP  10.0f

#define FEET_OFFSET 80.0f

/* ============================================================
   Power-ups
   ============================================================ */
#define INVINCIBLE_TIME 5.0f
#define INVINCIBLE_ANIM_SCALE 1.35f
#define INVINCIBLE_FEET_FRAC  0.88f
#define SHIELD_TIME     10.0f
#define MAGNET_TIME     8.0f

#define PU_SPAWN_MIN    8.0f /* seconds between power-up pickups */
#define PU_SPAWN_MAX    15.0f
#define PU_SIZE         70.0f
#define PU_PICKUP_DIST  60.0f
#define PU_FX_SIZE      190.0f /* height of the effect drawn on the pin */

   /* Mystery box (what the pickup looks like in the lane) and the item roulette. */
#define BOX_FRAMES      4
#define BOX_W           64.0f
#define BOX_H           64.0f
#define BOX_TIME        0.12f
#define ROULETTE_TIME   1.5f /* seconds the icons spin after the box is picked up */
#define REVEAL_TIME     0.8f /* seconds the final power-up stays shown */

#define MAX_COINS         15
#define COIN_PER_SPAWN    3 /* each spawn drops 1 to 3 coins in one lane */
#define COIN_SPACING      90.0f
#define COIN_SIZE         90.0f /* drawn height */

/* Coin sprite sheet: COIN_FRAMES frames side by side, each COIN_SRC_W x COIN_SRC_H. */
#define COIN_FRAMES       7
#define COIN_SRC_W        500.0f
#define COIN_SRC_H        500.0f
#define COIN_FRAME_TIME   0.10f
#define COIN_SPAWN_MIN    3.0f
#define COIN_SPAWN_MAX    6.0f
#define COIN_COLLECT_DIST 60.0f
#define COIN_SCORE        25


/* Multiplier: each one picked up raises the score multiplier. */
#define MULT_SIZE         90.0f   /* drawn size */
#define MULT_SPAWN_MIN    8.0f    /* seconds between stars */
#define MULT_SPAWN_MAX    12.0f
#define MULT_COLLECT_DIST 60.0f
#define MAX_MULT     5


#define MAGNET_RANGE      450.0f
#define MAGNET_PULL_SPEED 1400.0f

/* Death animation*/
#define DEATH_SCALE        0.38f /* 110 / 374 */
#define DEATH_PIN_SRC_X    695.0f /* pin centre x in frame 0 */
#define DEATH_PIN_SRC_FEET 468.0f /* pin feet y in frame 0 */
#define DEATH_FRAME_TIME   0.08f

/* Death-screen layout. */
#define WIN_W ((float)CP_System_GetWindowWidth())
#define WIN_H ((float)CP_System_GetWindowHeight())
#define window_center_x (WIN_W / 2.0f)
#define BTN_W (360.0f * HUD_SCALE)
#define BTN_H (80.0f * HUD_SCALE)
#define BTN_Y     (WIN_H * 0.76f)
#define RESTART_X (window_center_x - 200.0f * HUD_SCALE)
#define EXIT_X    (window_center_x + 200.0f * HUD_SCALE)

// Death Screen
CP_Font pixel_font;
int font_loaded = 0;
int show_death_screen = 0;
const char* death_title = "GAME OVER";

// Health Bar (in heart shape :)
CP_Image heart_full;

#define max_lives 3 //only a total of 3 hearts
#define heart_draw_size 50.0f //heart size
#define heart_spacing 60.0f //heart spacing
#define hearts_center_x (WIN_W - 1100.0f)  
#define hearts_center_y 65.0f  // same height as pause button

int lives = max_lives;
int obs_hit[MAX_OBS];

//Player Hits Obstacle Flick
#define hurt_duration   1.2f    // total time of the hit effect (seconds)
#define hurt_min_alpha  70      // how faint the runner is during a flicker
#define slow_factor     0.45f   // world speed right after the hit (1.0 = normal)
#define stumble_hold    0.4f    // fraction of hurt_duration spent at the slow speed
#define stumble_lurch   25.0f   // how far (px) the runner is knocked back visually
#define HIT_VOL 1.0f

float hurt_timer = 0.0f;
CP_Sound sndHit;

// Pause button (top right) and pause menu
#define pause_button_size (60.0f * HUD_SCALE)
#define pause_button_x    (WIN_W - 46.0f * HUD_SCALE)
#define pause_button_y    (48.0f * HUD_SCALE)
#define pause_resume_y    (WIN_H * 0.50f)
#define pause_quit_y      (WIN_H * 0.65f)
#define PAUSE_BTN_W       (460.0f * HUD_SCALE)

int is_paused = 0;

/* [ADDED BY LEKKAR] Heart pickup settings and sound */
//Hearts Spawn
#define HEART_SPAWN_MIN   5.0f   /* seconds between heart spawns */
#define HEART_SPAWN_MAX   8.0f
#define HEART_PICKUP_SIZE 60.0f   /* drawn size */
#define HEART_PICKUP_DIST 60.0f   /* collect distance */
#define HEART_FIRST_DELAY_MIN 3.0f   /* seconds after losing a heart */
#define HEART_FIRST_DELAY_MAX 6.0f
#define HEART_VOL 1.0f

static CP_Sound sndHeart = NULL;

/* ============================================================
   Rage overlay
   ============================================================ */

   // stored font for the rage overlay text, 
   // game_init() loads it once and keeps it in memory for the rest of the game.
static CP_Font rageFont = NULL;

#define RAGE_QUIET_MIN_MS 6000 // minimum wait: 6 seconds
#define RAGE_QUIET_MAX_MS 22000 // maximum wait: 22 seconds

#define RAGE_EVENT_DURATION 6.0f // how long the overlay lasts (seconds)
#define RAGE_DRAW_SIZE 64.0f // base size of the text (scaled up for larger windows)
#define RAGE_ROWS 20 // number of rows of text to draw (the text is randomly chosen from the rageWords array)
#define RAGE_PHRASES_PER_ROW 8 // number of phrases to draw in each row (the text is randomly chosen from the rageWords array)
#define RAGE_PHRASE_GAP 0.0f // horizontal gap between phrases (0 = no gap, the text is drawn right next to each other)

static float rageSpawnTimer = 0.0f; // time until the next rage event (randomly chosen between RAGE_QUIET_MIN_MS and RAGE_QUIET_MAX_MS)
static bool rageEventActive = false; // true if the rage overlay is currently being drawn
static float rageEventTimer = 0.0f; // time since the rage overlay started (used to determine when to stop drawing it)

static const char* rageWords[] = // the text that can be drawn in the rage overlay
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
    ((int)(sizeof(rageWords) / sizeof(rageWords[0]))) // size of all slots ÷ size of one slot = number of slots, each string above is one entry in the array.

/* ============================================================
   Types
   ============================================================ */

typedef enum
{
    STATE_PLAYING,
    STATE_ENEMY_WALK,
    STATE_GRAB,
    STATE_DYING /* pin plays a hole / oil death animation in place */
} GameplayPhase;

typedef enum
{
    OBS_BARRIER,
    OBS_OIL,
    OBS_HOLE
} ObsKind;

typedef enum
{
    PU_INVINCIBLE,
    PU_SHIELD,
    PU_MAGNET,
    NUM_POWERUPS
} PowerType;

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
static CP_Image borderImage = NULL;

static CP_Image hudCoinImage = NULL;
#define HUD_SCALE fminf(WIN_W / 1280.0f, WIN_H / 720.0f)

static CP_Font scoreFont = NULL;
static CP_Font deathFont = NULL;

static int distance = 0;
static float score = 0.0f;
static float distanceTimer = 0.0f;
static int highScore = 0;
static int highScoreAtStart = 0;

// results for the current run.
static float runSurvivalSeconds = 0.0f;
static const char* runDeathCause = "unknown";
static bool runResultReady = false;

static bool telemetryEnabled = true; // set to false to disable telemetry events (for testing or debugging)

/* Power-up state. */
static float invincibleTimer = 0.0f;
static float shieldTimer = 0.0f;
static float magnetTimer = 0.0f;

/* Collectable power-up (one on screen at a time). */
static bool pu_active = false;
static float pu_x = 0.0f;
static int pu_lane = 0;
static int pu_type = 0;
static float puSpawnTimer = 0.0f;

/* [ADDED BY LEKKAR] Heart pickup state */
/* Heart pickup (one on screen at a time). */
static bool  heart_active = false;
static float heart_x = 0.0f;
static int   heart_lane = 0;
static float heartSpawnTimer = 0.0f;
static CP_Image heartPickupImg = NULL;

/* Mystery box + roulette. */
static Animation boxAnim;               /* the box in the lane */
static float rouletteTimer = 0.0f;      /* > 0 while the icons are spinning */
static float rouletteSwitch = 0.0f;     /* time until the icon changes */
static int rouletteShown = 0;           /* icon currently shown in the slot */
static int rouletteType = 0;            /* the real power-up being given */
static float revealTimer = 0.0f;        /* > 0 while the final icon is shown */

/* Coins. */
static float coin_x[MAX_COINS];
static float coin_y[MAX_COINS];
static bool coin_active[MAX_COINS];
static float coinSpawnTimer = 0.0f;
static Animation coinAnim;

/* Power-up sprite sheets. */
static Animation puIconAnim[NUM_POWERUPS];  /* pickup icon in the lane */
static Animation puFx[NUM_POWERUPS];        /* effect drawn on the pin */
static int coinCount = 0;

/*Multiplier sprite sheets*/
static Animation multiFx;

/* Star pickup and the score multiplier it raises. */
static bool  multiplier_active = false;
static float multiplier_x = 0.0f;
static float multiplier_y = 0.0f;
static float multiplierSpawnTimer = 0.0f;
static int   scoreMultiplier = 1;


static const float pinYOffset[NUM_LANES] =
{
    50.0f,
    0.0f,
    -50.0f
};

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
    "../Assets/pixil-layer-barrier.png",
    "../Assets/pixil-layer-Oil Bubble (500).png",
    "../Assets/pixil-layer-hole.png"
};

// obstacle animation data (shared sprite sheet, one frame counter per obstacle). 
typedef struct
{
    const char* path;
    int frames;
    float w; /* size of ONE frame in the PNG */
    float h;
    float frameTime; /* seconds per frame */
} FxSheetInfo;

/* Edit path / frame count / frame size / speed when the art is ready. */
static const FxSheetInfo oilSpawnInfo = { "../Assets/pixil-layer-Oil Bubble (500).png",      4, 500.0f, 500.0f, 0.08f };
static const FxSheetInfo barrierBreakInfo = { "../Assets/pixil-layer-Barrier Break (500).png",  4, 500.0f, 500.0f, 0.07f };
static const FxSheetInfo holeDeathInfo = { "../Assets/pixil-layer-Hole Fall (500).png", 5, 500.0f, 500.0f, 0.10f };
static const FxSheetInfo oilDeathInfo = { "../Assets/pixil-layer-Pin Fall (500).png",  4, 500.0f, 500.0f, 0.10f };

/* The oil spawn animation starts once the whole spill is on screen
   (fraction of the screen width; 1.0 = as soon as it is fully visible). */
#define OIL_SPAWN_AT 0.97f

typedef enum
{
    OBS_PH_NORMAL,      /* normal sprite */
    OBS_PH_WAITING,     /* oil that has not appeared on screen yet */
    OBS_PH_SPAWN,       /* oil playing its spawn animation */
    OBS_PH_BROKEN       /* barrier playing its break animation, then holding the last frame */
} ObsPhase;

static Animation oilSpawnAnim;       /* shared sheet, one frame counter per obstacle below */
static Animation barrierBreakAnim;
static Animation holeDeathAnim;      /* pin falls into the hole */
static Animation oilDeathAnim;       /* pin slips on the oil */
static Animation* activeDeathAnim = NULL;
static int deathObs = -1;

static int   obs_phase[MAX_OBS];
static int   obs_frame[MAX_OBS];
static float obs_timer[MAX_OBS];

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

////ADDED BY LEKKAR
void Anim_DrawAlpha(Animation* a, float x, float y, int alpha)
{
    if (a->sheet == NULL) {
        return;
    }

    float left = (float)a->currentFrame * a->frameW;

    CP_Image_DrawSubImage(a->sheet, x, y, a->drawW, a->drawH,
        left, 0.0f, left + a->frameW, a->frameH, alpha);
}
////ADDED BY LEKKAR
void Anim_Draw(Animation* a, float x, float y)
{
    Anim_DrawAlpha(a, x, y, 255);
}

/* Draw a specific frame (used when many obstacles share one sheet). */
static void Anim_DrawFrame(const Animation* a, int frame, float x, float y, int alpha)
{
    if (a->sheet == NULL)
    {
        return;
    }

    float left = (float)frame * a->frameW;

    CP_Image_DrawSubImage(a->sheet, x, y, a->drawW, a->drawH,
        left, 0.0f, left + a->frameW, a->frameH, alpha);
}

/* ============================================================
   Game helpers
   ============================================================ */

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
static void DrawObstacle(int i)
{
    Animation* a = &obsAnim[obs_type[i]];

    float centerX = obs_x[i] + a->drawW / 2.0f;
    float centerY = (float)height * obstacleLaneCenter[obs_lane[i]];

    /* Oil that has not appeared yet is not drawn. */
    if (obs_phase[i] == OBS_PH_WAITING)
    {
        return;
    }

    const Animation* fx = NULL;

    if (obs_phase[i] == OBS_PH_SPAWN)
    {
        fx = &oilSpawnAnim;
    }
    else if (obs_phase[i] == OBS_PH_BROKEN)
    {
        fx = &barrierBreakAnim;
    }

    if (fx != NULL && fx->sheet != NULL)
    {
        Anim_DrawFrame(fx, obs_frame[i], centerX, centerY, 255);
    }
    else
    {
        Anim_Draw(a, centerX, centerY);
    }
}

static void RandomiseObstacle(int i)
{
    obs_type[i] = CP_Random_RangeInt(0, NUM_OBS_TYPES - 1);
    obs_lane[i] = CP_Random_RangeInt(0, NUM_LANES - 1);

    /* Fresh obstacle: oil waits off screen until its spawn animation starts. */
    obs_phase[i] = OBS_PH_NORMAL;
    obs_frame[i] = 0;
    obs_timer[i] = 0.0f;

    if (obs_type[i] == OBS_OIL && oilSpawnAnim.sheet != NULL)
    {
        obs_phase[i] = OBS_PH_SPAWN;
    }
}

int Game_GetHighScore(void)
{
    return highScore;
}

void StartRun(void)
{
    state = STATE_PLAYING;

    runSurvivalSeconds = 0.0f;  // start this run's timer at zero.
    runDeathCause = "unknown";  // no fatal obstacle yet.
    runResultReady = false;     // this run has not finished yet.

    lane = 1;
    speed = START_SPEED;

    score = 0.0f;
    distance = 0;
    distanceTimer = 0.0f;
    highScoreAtStart = highScore;

    invincibleTimer = 0.0f;
    shieldTimer = 0.0f;
    magnetTimer = 0.0f;
    coinCount = 0;

    pu_active = false;
    puSpawnTimer = CP_Random_RangeFloat(PU_SPAWN_MIN, PU_SPAWN_MAX);

    // [ADDED BY LEKKAR] Reset heart pickup for a new run
    heart_active = false;
    heartSpawnTimer = CP_Random_RangeFloat(HEART_SPAWN_MIN, HEART_SPAWN_MAX);

    rouletteTimer = 0.0f;
    rouletteSwitch = 0.0f;
    revealTimer = 0.0f;

    for (int c = 0; c < MAX_COINS; c++)
    {
        coin_active[c] = false;
    }

    coinSpawnTimer = 2.0f;

    multiplier_active = false;
    multiplierSpawnTimer = CP_Random_RangeFloat(MULT_SPAWN_MIN, MULT_SPAWN_MAX);
    scoreMultiplier = 1;

    enemyX = (float)width * 0.12f;
    enemyGround = PinFeetY(lane);

    Anim_Reset(&runAnim);
    Anim_Reset(&chaseAnim);
    Anim_Reset(&grabAnim);
}

static void PlayDeathAudio(void)
{
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

/* Ball catches the pin (barrier deaths, or when there is no death art yet). */
static void KillPlayer(void)
{
    state = STATE_ENEMY_WALK;
    PlayDeathAudio();
}

/* The pin dies in place and plays its own animation (hole / oil). */
static void StartDying(Animation* a, int obsIndex)
{
    state = STATE_DYING;
    activeDeathAnim = a;
    deathObs = obsIndex;
    Anim_Reset(a);
    PlayDeathAudio();
}

/* ============================================================
   Power-up and coin helpers
   ============================================================ */

typedef struct
{
    const char* path;
    int frames;
    float w;            /* size of ONE frame in the PNG */
    float h;
    float frameTime;    /* seconds per frame */
} SheetInfo;

/* Pickup icons (the item sitting in the lane). Edit to match your art. */
static const SheetInfo puIconInfo[NUM_POWERUPS] =
{
    { "../Assets/invincible.PNG",    1, 640.0f, 640.0f, 0.12f },
    { "../Assets/pickup_shield.png", 1, 500.0f, 500.0f, 0.12f },
    { "../Assets/magnet.PNG",        1, 640.0f, 640.0f, 0.12f }
};

/* Effects drawn on the pin (they all loop). */
static const SheetInfo puFxInfo[NUM_POWERUPS] =
{
    { "../Assets/invincible_anim.png", 4, 384.0f, 426.0f, 0.15f },
    { "../Assets/pixil-layer-Pin Shield (500).png",     4, 500.0f, 500.0f, 0.10f },
    { "../Assets/fx_magnet.png",     6, 128.0f, 128.0f, 0.10f }
};

static const char* puLabels[NUM_POWERUPS] = { "I", "S", "M" };
static const char* puNames[NUM_POWERUPS] = { "INVINCIBLE", "SHIELD", "MAGNET" };

/* Send an obstacle back behind the furthest one (also used to "break" it). */
static void RespawnObstacle(int i)
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
    obs_hit[i] = 0;
}

/* y of the pin's centre when it is in lane l. */
static float PlayerCenterY(int l)
{
    return PinFeetY(l) - runAnim.drawH / 2.0f;
}

/* true if no obstacle in lane l is within margin pixels of x. */
static bool LaneClearAt(float x, int l, float margin)
{
    for (int i = 0; i < MAX_OBS; i++)
    {
        if (obs_lane[i] != l)
        {
            continue;
        }

        float centre = obs_x[i] + obsAnim[obs_type[i]].drawW / 2.0f;

        if (fabsf(centre - x) < margin)
        {
            return false;
        }
    }

    return true;
}

static CP_Color PowerColor(int type, int alpha)
{
    unsigned char a = (unsigned char)alpha;

    switch (type)
    {
    case PU_INVINCIBLE: return CP_Color_Create(255, 200, 0, a);
    case PU_SHIELD:     return CP_Color_Create(80, 180, 255, a);
    default:            return CP_Color_Create(170, 90, 255, a);
    }
}

static void ApplyPowerUp(int type)
{
    switch (type)
    {
    case PU_INVINCIBLE: invincibleTimer = INVINCIBLE_TIME; break;
    case PU_SHIELD:     shieldTimer = SHIELD_TIME;         break;
    case PU_MAGNET:     magnetTimer = MAGNET_TIME;         break;
    }
}

/* The pin picked up the mystery box: start the spinning icons. */
static void StartRoulette(int type)
{
    rouletteType = type;
    rouletteTimer = ROULETTE_TIME;
    rouletteSwitch = 0.0f;
    rouletteShown = CP_Random_RangeInt(0, NUM_POWERUPS - 1);
    revealTimer = 0.0f;
}

static void TrySpawnPickup(void)
{
    for (int tries = 0; tries < 6; tries++)
    {
        int l = CP_Random_RangeInt(0, NUM_LANES - 1);
        float x = (float)width + 100.0f * gameScale;

        if (LaneClearAt(x, l, 300.0f * gameScale))
        {
            pu_active = true;
            pu_x = x;
            pu_lane = l;
            pu_type = CP_Random_RangeInt(0, NUM_POWERUPS - 1);

            puSpawnTimer =
                CP_Random_RangeFloat(PU_SPAWN_MIN, PU_SPAWN_MAX);

            return;
        }
    }

    /* No clear lane right now: try again next frame. */
}

static void TrySpawnCoins(void)
{
    for (int tries = 0; tries < 6; tries++)
    {
        int l = CP_Random_RangeInt(0, NUM_LANES - 1);
        int count = COIN_PER_SPAWN;
        float x = (float)width + 100.0f * gameScale;
        float rowCentre =
            x + ((float)count * COIN_SPACING * gameScale) / 2.0f;

        if (LaneClearAt(rowCentre, l, 400.0f * gameScale))
        {
            int placed = 0;

            for (int c = 0; c < MAX_COINS && placed < count; c++)
            {
                if (!coin_active[c])
                {
                    coin_active[c] = true;
                    coin_x[c] = x + (float)placed * COIN_SPACING * gameScale;
                    coin_y[c] = PlayerCenterY(l);
                    placed++;
                }
            }

            coinSpawnTimer = CP_Random_RangeFloat(COIN_SPAWN_MIN, COIN_SPAWN_MAX);

            return;
        }
    }
}

static void SpawnMultiplier(void)
{
    for (int tries = 0; tries < 6; tries++)
    {
        int l = CP_Random_RangeInt(0, NUM_LANES - 1);
        float x = (float)width + 100.0f * gameScale;

        if (LaneClearAt(x, l, 400.0f * gameScale))
        {
            multiplier_active = true;
            multiplier_x = x;
            multiplier_y = PlayerCenterY(l);

            multiplierSpawnTimer = CP_Random_RangeFloat(MULT_SPAWN_MIN, MULT_SPAWN_MAX);

            return;
        }
    }

    /* No clear lane right now: try again next frame. */
}
// [ADDED BY LEKKAR] Spawn heart in a clear lane
static void TrySpawnHeart(void)
{
    for (int tries = 0; tries < 6; tries++)
    {
        int l = CP_Random_RangeInt(0, NUM_LANES - 1);
        float x = (float)width + 100.0f * gameScale;

        if (!LaneClearAt(x, l, 300.0f * gameScale))
        {
            continue;
        }

        if (pu_active && pu_lane == l &&
            fabsf(pu_x - x) < 250.0f * gameScale)
        {
            continue;
        }

        heart_active = true;
        heart_x = x;
        heart_lane = l;
        heartSpawnTimer = CP_Random_RangeFloat(HEART_SPAWN_MIN, HEART_SPAWN_MAX);
        return;
    }
}

/* A barrier breaks: play the break animation and hold its last frame.
   It keeps scrolling with the world and respawns normally off screen. */
static void BreakObstacle(int i)
{
    if (barrierBreakAnim.sheet != NULL)
    {
        obs_phase[i] = OBS_PH_BROKEN;
        obs_frame[i] = 0;
        obs_timer[i] = 0.0f;
    }
    else
    {
        RespawnObstacle(i);   /* no break art yet: old behaviour */
    }
}

/* Advance the spawn / break animation of obstacle i. */
static void ObsFx_Update(int i, float dt)
{
    if (obs_phase[i] == OBS_PH_WAITING)
    {
        /* start the spawn animation once the whole spill is on screen */
        if (obs_x[i] + obsAnim[obs_type[i]].drawW < (float)width * OIL_SPAWN_AT)
        {
            obs_phase[i] = OBS_PH_SPAWN;
            obs_frame[i] = 0;
            obs_timer[i] = 0.0f;
        }
        return;
    }

    const Animation* a = NULL;

    if (obs_phase[i] == OBS_PH_SPAWN)
    {
        a = &oilSpawnAnim;
    }
    else if (obs_phase[i] == OBS_PH_BROKEN)
    {
        a = &barrierBreakAnim;
    }
    else
    {
        return;
    }

    if (a->frameTime <= 0.0f || a->frameCount <= 0)
    {
        return;
    }

    obs_timer[i] += dt;

    while (obs_timer[i] >= a->frameTime)
    {
        obs_timer[i] -= a->frameTime;

        if (obs_frame[i] < a->frameCount - 1)
        {
            obs_frame[i]++;
        }
        else if (obs_phase[i] == OBS_PH_SPAWN)
        {
            obs_frame[i] = 0;      /* oil keeps looping its animation */
        }
        else
        {
            obs_timer[i] = 0.0f;   /* a broken barrier holds its last frame */
            break;
        }
    }
}

/* Called when the pin overlaps obstacle i. Returns true if the pin died. */
static bool HandleHit(int i)
{
    int type = obs_type[i];

    /* Invincible: smashes barriers, drives through oil, but holes still hurt. */
    if (invincibleTimer > 0.0f && type != OBS_HOLE)
    {
        if (type == OBS_BARRIER)
        {
            BreakObstacle(i);
        }

        return false;
    }

    /* Shield: absorbs exactly one barrier or oil hit. */
    if (shieldTimer > 0.0f && (type == OBS_BARRIER || type == OBS_OIL))
    {
        shieldTimer = 0.0f;

        if (type == OBS_BARRIER)
        {
            BreakObstacle(i);
        }

        return false;
    }

    /* No power-up protects against this obstacle: lose a heart. */
    if (type == OBS_BARRIER)
    {
        BreakObstacle(i);
    }
    lives--;

    // [ADDED BY LEKKAR] Schedule heart after taking damage
    if (!heart_active)
    {
        heartSpawnTimer = CP_Random_RangeFloat(HEART_FIRST_DELAY_MIN,
            HEART_FIRST_DELAY_MAX);
    }

    if (lives <= 0)
    {
        lives = 0;
        hurt_timer = 0.0f;

        // record the obstacle that removed the final heart.
        switch (type)
        {
        case OBS_BARRIER:
            runDeathCause = "barrier";
            break;

        case OBS_OIL:
            runDeathCause = "oil";
            break;

        case OBS_HOLE:
            runDeathCause = "hole";
            break;

        default:
            runDeathCause = "unknown";
            break;
        }

        runResultReady = true;  // the final results can now be submitted.

        /* Holes and oil have their own death animation. */
        Animation* dyingAnim = NULL;

        if (type == OBS_HOLE)
        {
            dyingAnim = &holeDeathAnim;
        }
        else if (type == OBS_OIL)
        {
            dyingAnim = &oilDeathAnim;
        }

        if (dyingAnim != NULL && dyingAnim->sheet != NULL)
        {
            StartDying(dyingAnim, i);
        }
        else
        {
            KillPlayer();                 /* ball catches the pin */
        }

        return true;                      /* the pin died */
    }

    hurt_timer = hurt_duration;           // start flicker + stumble

    if (sndHit != NULL)
    {
        CP_Sound_PlayAdvanced(sndHit, HIT_VOL, 1.0f, false, CP_SOUND_GROUP_SFX);
    }

    return false;
}

/* Effect on the pin: the sprite sheet, or a plain circle if it's missing. */
static void Draw_PowerFx(int type, float x, float y)
{
    if (puFx[type].sheet != NULL)
    {
        Anim_Draw(&puFx[type], x, y);
        return;
    }

    switch (type)
    {
    case PU_INVINCIBLE:
        CP_Settings_Fill(PowerColor(PU_INVINCIBLE, 90));
        CP_Graphics_DrawCircle(x, y, 190.0f * gameScale);
        break;

    case PU_SHIELD:
        CP_Settings_Fill(PowerColor(PU_SHIELD, 100));
        CP_Graphics_DrawCircle(x, y, 160.0f * gameScale);
        break;

    case PU_MAGNET:
        CP_Settings_Fill(PowerColor(PU_MAGNET, 70));
        CP_Graphics_DrawCircle(x, y, 220.0f * gameScale);
        break;

    }
}

static void Draw_Pickups(void)
{
    CP_Settings_NoStroke();

    CP_Settings_Fill(CP_Color_Create(255, 215, 0, 255));

    for (int c = 0; c < MAX_COINS; c++)
    {
        if (!coin_active[c])
        {
            continue;
        }

        if (coinAnim.sheet != NULL)
        {
            Anim_Draw(&coinAnim, coin_x[c], coin_y[c]);
        }
        else
        {
            /* fallback if coin.png is missing */
            CP_Graphics_DrawCircle(
                coin_x[c],
                coin_y[c],
                COIN_SIZE * gameScale
            );
        }
    }

    if (pu_active)
    {
        float y = PlayerCenterY(pu_lane);

        /* the real power-up stays hidden: every pickup looks like the same box */
        if (boxAnim.sheet != NULL)
        {
            Anim_Draw(&boxAnim, pu_x, y);
        }
        else
        {
            /* fallback if the box sheet is missing: gold circle with a ? */
            CP_Settings_Fill(CP_Color_Create(255, 200, 0, 255));
            CP_Graphics_DrawCircle(pu_x, y, PU_SIZE * gameScale);

            if (scoreFont != NULL)
            {
                CP_Font_Set(scoreFont);
            }

            CP_Settings_Fill(CP_Color_Create(255, 255, 255, 255));
            CP_Settings_TextSize(40.0f * gameScale);

            CP_Settings_TextAlignment(
                CP_TEXT_ALIGN_H_CENTER,
                CP_TEXT_ALIGN_V_MIDDLE
            );

            CP_Font_DrawText("?", pu_x, y);
        }
    }
    // [ADDED BY LEKKAR] Draw bobbing heart pickup
    if (heart_active)
    {
        float bob = sinf(CP_System_GetSeconds() * 5.0f) * 6.0f * gameScale;
        float s = HEART_PICKUP_SIZE * gameScale;
        float y = PlayerCenterY(heart_lane) + bob;

        if (heartPickupImg != NULL)
        {
            CP_Image_Draw(heartPickupImg, heart_x, y, s, s, 255);
        }
    }

}

/* Draws a power-up icon (or a coloured circle with a letter if the sheet is missing). */
static void Draw_PowerIcon(int type, float x, float y)
{
    if (puIconAnim[type].sheet != NULL)
    {
        Anim_Draw(&puIconAnim[type], x, y);
        return;
    }

    CP_Settings_Fill(PowerColor(type, 255));
    CP_Graphics_DrawCircle(x, y, PU_SIZE * gameScale);

    if (scoreFont != NULL)
    {
        CP_Font_Set(scoreFont);
    }

    CP_Settings_Fill(CP_Color_Create(255, 255, 255, 255));
    CP_Settings_TextSize(30.0f * gameScale);

    CP_Settings_TextAlignment(
        CP_TEXT_ALIGN_H_CENTER,
        CP_TEXT_ALIGN_V_MIDDLE
    );

    CP_Font_DrawText(puLabels[type], x, y);
}

/* The item slot at the top-centre: icons spin, then land on the real power-up. */
static void Draw_Roulette(void)
{
    if (state != STATE_PLAYING ||
        (rouletteTimer <= 0.0f && revealTimer <= 0.0f))
    {
        return;
    }

    float x = (float)width / 2.0f;
    float y = 145.0f * HUD_SCALE;
    float boxSize = (PU_SIZE + 40.0f) * gameScale;

    /* dark square behind the icon */
    CP_Settings_RectMode(CP_POSITION_CENTER);
    CP_Settings_Fill(CP_Color_Create(20, 20, 25, 220));
    CP_Graphics_DrawRect(x, y, boxSize, boxSize);

    Draw_PowerIcon(rouletteShown, x, y);

    /* once the spin is over, show the name of what the player got */
    if (revealTimer > 0.0f)
    {
        if (scoreFont != NULL)
        {
            CP_Font_Set(scoreFont);
        }

        CP_Settings_Fill(PowerColor(rouletteShown, 255));
        CP_Settings_TextSize(24.0f * gameScale);

        CP_Settings_TextAlignment(
            CP_TEXT_ALIGN_H_CENTER,
            CP_TEXT_ALIGN_V_MIDDLE
        );

        CP_Font_DrawText(
            puNames[rouletteShown],
            x,
            y + boxSize / 2.0f + 25.0f * gameScale
        );
    }
}

/* Multiplier sprite sheet, or a plain circle if it's missing. */
static void Draw_Multiplier(float x, float y)
{
    if (multiFx.sheet != NULL)
    {
        Anim_Draw(&multiFx, x, y);
        return;
    }

    CP_Settings_NoStroke();
    CP_Settings_Fill(CP_Color_Create(255, 0, 0, 90));
    CP_Graphics_DrawCircle(x, y, MULT_SIZE * gameScale);

}

static void DrawHUDPanel(
    float x,
    float y,
    float w,
    float h,
    int hovered)
{
    float s = HUD_SCALE;
    float cut = 7.0f * s;

    float left = x - w * 0.5f;
    float right = x + w * 0.5f;
    float top = y - h * 0.5f;
    float bottom = y + h * 0.5f;

    CP_Settings_RectMode(CP_POSITION_CORNER);
    CP_Settings_NoStroke();

    CP_Settings_Fill(
        hovered
        ? CP_Color_Create(35, 35, 40, 235)
        : CP_Color_Create(5, 5, 10, 220)
    );

    /* Fill an eight-sided, clipped-corner rectangle. */
    CP_Graphics_DrawRect(
        left + cut,
        top,
        w - 2.0f * cut,
        h
    );

    CP_Graphics_DrawRect(
        left,
        top + cut,
        w,
        h - 2.0f * cut
    );

    CP_Graphics_DrawTriangle(
        left + cut, top,
        left + cut, top + cut,
        left, top + cut
    );

    CP_Graphics_DrawTriangle(
        right - cut, top,
        right, top + cut,
        right - cut, top + cut
    );

    CP_Graphics_DrawTriangle(
        left, bottom - cut,
        left + cut, bottom - cut,
        left + cut, bottom
    );

    CP_Graphics_DrawTriangle(
        right - cut, bottom - cut,
        right, bottom - cut,
        right - cut, bottom
    );

    /* Light border. */
    CP_Settings_Stroke(
        hovered
        ? CP_Color_Create(255, 225, 100, 255)
        : CP_Color_Create(205, 205, 215, 255)
    );

    CP_Settings_StrokeWeight(2.0f * s);

    CP_Graphics_DrawLine(left + cut, top, right - cut, top);
    CP_Graphics_DrawLine(right - cut, top, right, top + cut);

    CP_Graphics_DrawLine(
        right, top + cut,
        right, bottom - cut
    );

    CP_Graphics_DrawLine(
        right, bottom - cut,
        right - cut, bottom
    );

    CP_Graphics_DrawLine(
        right - cut, bottom,
        left + cut, bottom
    );

    CP_Graphics_DrawLine(
        left + cut, bottom,
        left, bottom - cut
    );

    CP_Graphics_DrawLine(
        left, bottom - cut,
        left, top + cut
    );

    CP_Graphics_DrawLine(left, top + cut, left + cut, top);

    CP_Settings_NoStroke();
}

/* Fit longer values inside their panel. */
static void DrawHUDText(
    const char* text,
    float x,
    float y,
    float preferredSize,
    float availableWidth,
    CP_Color color)
{
    size_t length = strlen(text);
    float size = preferredSize;

    if (length > 0)
    {
        float maximumSize = availableWidth / (float)length;
        size = fminf(size, maximumSize);
    }

    CP_Settings_TextAlignment(
        CP_TEXT_ALIGN_H_CENTER,
        CP_TEXT_ALIGN_V_MIDDLE
    );

    CP_Settings_TextSize(size);
    CP_Settings_Fill(color);
    CP_Font_DrawText(text, x, y);
}

static void Draw_HUD(void)
{
    char text[64];

    float s = HUD_SCALE;
    float y = 48.0f * s;
    float panelH = 60.0f * s;

    CP_Color white = CP_Color_Create(245, 245, 245, 255);
    CP_Color gold = CP_Color_Create(255, 220, 65, 255);

    if (scoreFont != NULL)
    {
        CP_Font_Set(scoreFont);
    }

    CP_Settings_ImageMode(CP_POSITION_CENTER);
    CP_Settings_NoStroke();

    /* ==================== SCORE ==================== */
    float scoreX = 84.0f * s;

    DrawHUDPanel(
        scoreX,
        y,
        136.0f * s,
        panelH,
        0
    );

    DrawHUDText(
        "SCORE",
        scoreX,
        y - 15.0f * s,
        13.0f * s,
        112.0f * s,
        white
    );

    snprintf(
        text,
        sizeof(text),
        "%05d",
        (int)score
    );

    DrawHUDText(
        text,
        scoreX,
        y + 12.0f * s,
        23.0f * s,
        112.0f * s,
        gold
    );

    /* ==================== COINS ==================== */
    float coinX = 222.0f * s;

    DrawHUDPanel(
        coinX,
        y,
        116.0f * s,
        panelH,
        0
    );

    if (hudCoinImage != NULL)
    {
        CP_Image_Draw(
            hudCoinImage,
            coinX - 35.0f * s,
            y,
            28.0f * s,
            38.0f * s,
            255
        );
    }
    else
    {
        CP_Settings_Fill(gold);

        CP_Graphics_DrawCircle(
            coinX - 35.0f * s,
            y,
            25.0f * s
        );
    }

    snprintf(
        text,
        sizeof(text),
        "x%02d",
        coinCount
    );

    DrawHUDText(
        text,
        coinX + 15.0f * s,
        y,
        23.0f * s,
        70.0f * s,
        white
    );
	 // hearts remain in their own panel, drawn by Draw_Hearts(). panel ends at x = 472 * s.

     /* ==================== DISTANCE ==================== */

    float distanceX = WIN_W - 153.0f * s;

    DrawHUDPanel(
        distanceX,
        y,
        130.0f * s,
        panelH,
        0
    );

    snprintf(
        text,
        sizeof(text),
        "%dm",
        distance
    );

    DrawHUDText(
        text,
        distanceX,
        y,
        23.0f * s,
        108.0f * s,
        white
    );

    /* ==================== MULTIPLIER ==================== */
    float multiplierX = WIN_W - 272.0f * s;

    DrawHUDPanel(
        multiplierX,
        y,
        84.0f * s,
        panelH,
        0
    );

    snprintf(
        text,
        sizeof(text),
        "x%d",
        scoreMultiplier
    );

    DrawHUDText(
        text,
        multiplierX,
        y,
        25.0f * s,
        64.0f * s,
        gold
    );

    /* ==================== POWER-UP TIMERS ==================== */
    float timerLeft = 484.0f * s;
    float timerRight = WIN_W - 326.0f * s;
    float timerAreaWidth = timerRight - timerLeft;

    int activeTypes[NUM_POWERUPS];
    float activeSeconds[NUM_POWERUPS];
    int activeCount = 0;

    if (invincibleTimer > 0.0f)
    {
        activeTypes[activeCount] = PU_INVINCIBLE;
        activeSeconds[activeCount] = invincibleTimer;
        activeCount++;
    }

    if (shieldTimer > 0.0f)
    {
        activeTypes[activeCount] = PU_SHIELD;
        activeSeconds[activeCount] = shieldTimer;
        activeCount++;
    }

    if (magnetTimer > 0.0f)
    {
        activeTypes[activeCount] = PU_MAGNET;
        activeSeconds[activeCount] = magnetTimer;
        activeCount++;
    }

    if (activeCount > 0 && timerAreaWidth > 0.0f)
    {
        float panelGap = 8.0f * s;

        float effectPanelWidth =
            (
                timerAreaWidth -
                panelGap * (float)(activeCount - 1)
                ) / (float)activeCount;

        if (scoreFont != NULL)
        {
            CP_Font_Set(scoreFont);
        }

        for (int i = 0; i < activeCount; i++)
        {
            int type = activeTypes[i];
            const char* effectName;
            char secondsText[32];

            float effectX =
                timerLeft +
                effectPanelWidth * 0.5f +
                (float)i * (effectPanelWidth + panelGap);

			// shorten the name if multiple effects are active, to fit the panel.
            switch (type)
            {
            case PU_INVINCIBLE:
                effectName =
                    activeCount == 1 ? "INVINCIBLE" : "INV";
                break;

            case PU_SHIELD:
                effectName =
                    activeCount == 1 ? "SHIELD" : "SHD";
                break;

            default:
                effectName =
                    activeCount == 1 ? "MAGNET" : "MAG";
                break;
            }

            snprintf(
                secondsText,
                sizeof(secondsText),
                "%.1fs",
                activeSeconds[i]
            );

            DrawHUDPanel(
                effectX,
                y,
                effectPanelWidth,
                panelH,
                0
            );

			// how much space is left for text after the padding on both sides of the panel?
            float padding = 14.0f * s;
            float availableWidth =
                fmaxf(1.0f, effectPanelWidth - 2.0f * padding);

            float textSize = 23.0f * s;

			// reduce the text size if the name + countdown + gap is too wide for the panel.
            float characterSlots =
                (float)strlen(effectName) +
                (float)strlen(secondsText) +
                1.5f;

            textSize = fminf(
                textSize,
                availableWidth / characterSlots
            );

            float nameWidth =
                (float)strlen(effectName) * textSize;

            float secondsWidth =
                (float)strlen(secondsText) * textSize;

            float textGap = 1.5f * textSize;

            float groupWidth =
                nameWidth + textGap + secondsWidth;

            float groupLeft = effectX - groupWidth * 0.5f;

            CP_Settings_TextSize(textSize);

			// draw the name in the power-up's colour, on the left side of the panel.
            CP_Settings_TextAlignment(
                CP_TEXT_ALIGN_H_LEFT,
                CP_TEXT_ALIGN_V_MIDDLE
            );

            CP_Settings_Fill(PowerColor(type, 255));

            CP_Font_DrawText(
                effectName,
                groupLeft,
                y
            );

			// draw the countdown in white, on the right side of the panel.
            CP_Settings_Fill(
                CP_Color_Create(245, 245, 245, 255)
            );

            CP_Font_DrawText(
                secondsText,
                groupLeft + nameWidth + textGap,
                y
            );
        }
    }

    CP_Settings_NoStroke();
    CP_Settings_RectMode(CP_POSITION_CORNER);
}

/* ============================================================
   [ADDED BY GWEN] Rage overlay helpers
   ============================================================ */

   /*! Specification for function RageOverlay_Reset:
   @brief Resets the rage words overlay and sets a random waiting time.

   Hides the rage words and sets a waiting time of 6 to 22 seconds
   before the next rage event.

   @param none.
   @return none.
   *//*________________________________________________________________________________*/
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

/*! Specification for function RageOverlay_Update:
@brief Updates the timers that show and hide the rage words.

Starts a rage event when the waiting time ends. After 6 seconds,
hides the words and sets another random waiting time.

@param dt - The time passed since the previous frame, in seconds.
@return none.
*//*________________________________________________________________________________*/
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

/*! Specification for function RageOverlay_Draw:
@brief Draws the rage words moving across the screen.

During an active rage event, displays rows of coloured phrases
moving from right to left. Scales the text to fit the window size.

@param none.
@return none.
*//*________________________________________________________________________________*/
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

        // calculate the starting and ending x positions for the row of phrases
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
    CP_System_SetWindowTitle("ALLEY RUNNER");
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

    /*Hearts*/
    heart_full = CP_Image_Load("../Assets/hearts.png");

    hudCoinImage = CP_Image_Load("../Assets/Coin_solo.png");

    if (hudCoinImage == NULL)
    {
        hudCoinImage = CP_Image_Load("Assets/Coin_solo.png");
    }

    // [ADDED BY LEKKAR] Load heart pickup image
    heartPickupImg = CP_Image_Load("../Assets/hearts.png");

    // [ADDED BY GWEN] rage words font: try the relative path first, then the local path if that fails.
    rageFont = CP_Font_Load("../Assets/VT323-Regular.ttf");

    if (rageFont == NULL)
    {
        rageFont = CP_Font_Load("Assets/VT323-Regular.ttf");
    }

    /* Background. */
    bgImage = CP_Image_Load("../Assets/background.png");

    /*Score Borders*/
    borderImage = CP_Image_Load("../Assets/background.png");


    /* Gameplay audio. */
    sndJump = CP_Sound_Load("../Assets/jump.mp3");
    sndCoin = CP_Sound_Load("../Assets/coin_pickup.mp3");
    sndDeath = CP_Sound_Load("../Assets/game_over.mp3");
    gameMusic = CP_Sound_Load("../Assets/bg_music.mp3");
    sndHit = CP_Sound_Load("../Assets/edr-video-game-hit-noise-001-135821.mp3");

    // [ADDED BY LEKKAR] Load heart pickup sound
    sndHeart = CP_Sound_Load("../Assets/heart_sound.mp3");

    if (gameMusic != NULL)
    {
        CP_Sound_PlayAdvanced(gameMusic, 1.0f, 1.0f, true, CP_SOUND_GROUP_MUSIC);
    }

    // Updated character sprite sheets.
    Anim_Init(&runAnim, "../Assets/pixil-layer-Pin Running.png", 4, PLAYER_W, PLAYER_H, 0.15f, 1);
    Anim_Init(&chaseAnim, "../Assets/pixil-layer-Ball Rolling (350).png", 8, ENEMY_W, ENEMY_H, 0.15f, 1);
    Anim_Init(&grabAnim, "../Assets/pixil-layer-0 (3).png", 8, 2000, 1000, DEATH_FRAME_TIME, 0);

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
        GRAB_W * gameScale * DEATH_SCALE,
        GRAB_H * gameScale * DEATH_SCALE
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
        float obstacleHeight = lane_gap * 0.6f;
        float obstacleWidth =
            obstacleHeight * (OBS_SRC_W / OBS_SRC_H);

        Anim_SetDrawSize(
            &obsAnim[t],
            obstacleWidth,
            obstacleHeight
        );
    }

    /* Obstacle event animations, drawn at the obstacle's size. */
    Anim_Init(&oilSpawnAnim, oilSpawnInfo.path, oilSpawnInfo.frames,
        oilSpawnInfo.w, oilSpawnInfo.h, oilSpawnInfo.frameTime, 0);
    Anim_SetDrawSize(&oilSpawnAnim,
        obsAnim[OBS_OIL].drawW, obsAnim[OBS_OIL].drawH);

    Anim_Init(&barrierBreakAnim, barrierBreakInfo.path, barrierBreakInfo.frames,
        barrierBreakInfo.w, barrierBreakInfo.h, barrierBreakInfo.frameTime, 0);
    Anim_SetDrawSize(&barrierBreakAnim,
        obsAnim[OBS_BARRIER].drawW, obsAnim[OBS_BARRIER].drawH);

    /* Pin death animations (hole / oil), same scale as the obstacles. */
    Anim_Init(&holeDeathAnim, holeDeathInfo.path, holeDeathInfo.frames, holeDeathInfo.w, holeDeathInfo.h, holeDeathInfo.frameTime, 0);
    Anim_SetDrawSize(&holeDeathAnim, obsAnim[OBS_HOLE].drawW * 1.45f, obsAnim[OBS_HOLE].drawH * 1.45f);
    Anim_Init(&oilDeathAnim, oilDeathInfo.path, oilDeathInfo.frames, oilDeathInfo.w, oilDeathInfo.h, oilDeathInfo.frameTime, 0);
    Anim_SetDrawSize(&oilDeathAnim, obsAnim[OBS_OIL].drawW, obsAnim[OBS_OIL].drawH);

    activeDeathAnim = NULL;

    /* Coin animation (one shared animation for every coin). */
    Anim_Init(&coinAnim, "../Assets/Coin.png", COIN_FRAMES, COIN_SRC_W, COIN_SRC_H, COIN_FRAME_TIME, 1);

    Anim_SetDrawSize(&coinAnim, COIN_SIZE * gameScale * (COIN_SRC_W / COIN_SRC_H), COIN_SIZE * gameScale);

    /* Power-up icons and effects. */
    for (int t = 0; t < NUM_POWERUPS; t++)
    {
        Anim_Init(
            &puIconAnim[t],
            puIconInfo[t].path,
            puIconInfo[t].frames,
            puIconInfo[t].w,
            puIconInfo[t].h,
            puIconInfo[t].frameTime,
            1
        );

        Anim_SetDrawSize(
            &puIconAnim[t],
            PU_SIZE * gameScale * (puIconInfo[t].w / puIconInfo[t].h),
            PU_SIZE * gameScale
        );

        Anim_Init(
            &puFx[t],
            puFxInfo[t].path,
            puFxInfo[t].frames,
            puFxInfo[t].w,
            puFxInfo[t].h,
            puFxInfo[t].frameTime,
            1
        );

        float fxH = PU_FX_SIZE * gameScale;
        float fxW = fxH * (puFxInfo[t].w / puFxInfo[t].h);

        if (t == PU_SHIELD)
        {
            fxW = runAnim.drawW;   /* same size as the pin */
            fxH = runAnim.drawH;
        }
        else if (t == PU_INVINCIBLE)
        {
            fxH = runAnim.drawH * INVINCIBLE_ANIM_SCALE;
            fxW = fxH * (puFxInfo[t].w / puFxInfo[t].h);
        }

        Anim_SetDrawSize(&puFx[t], fxW, fxH);
    }

    /* Mystery box shown in the lane before the power-up is picked up. */
    Anim_Init(
        &boxAnim,
        "../Assets/mystery_box.png",
        BOX_FRAMES,
        BOX_W,
        BOX_H,
        BOX_TIME,
        1
    );

    Anim_SetDrawSize(
        &boxAnim,
        PU_SIZE * gameScale * (BOX_W / BOX_H),
        PU_SIZE * gameScale
    );

    /* Scale initial spacing along with the obstacle sizes. */
    for (int i = 0; i < MAX_OBS; i++)
    {
        obs_x[i] =
            (float)width +
            (float)i * OBS_SPACING * gameScale;

        RandomiseObstacle(i);
        obs_hit[i] = 0;
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

    //Death Screen
    pixel_font = scoreFont;
    font_loaded = 1;
    show_death_screen = 0; // death screen is hidden hence 0
    is_paused = 0;

    //Health Bar
    lives = max_lives;
    hurt_timer = 0.0f;
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
        if (state == STATE_DYING && i == deathObs)
        {
            continue;     /* the death animation draws its own hole / oil */
        }

        DrawObstacle(i);
    }

    Draw_Pickups();
    if (multiplier_active)
    {
        Draw_Multiplier(multiplier_x, multiplier_y);
    }

    if (state == STATE_GRAB)
    {
        float k = DEATH_SCALE * gameScale;

        /* the running pin's feet sit about 7px above its frame bottom */
        float feetY = PinFeetY(lane) - 7.0f * gameScale;

        float cx = player_x + (GRAB_W / 2.0f - DEATH_PIN_SRC_X) * k;
        float cy = feetY - (DEATH_PIN_SRC_FEET - GRAB_H / 2.0f) * k;

        Anim_Draw(&grabAnim, cx, cy);
    }
    else if (state == STATE_DYING && activeDeathAnim != NULL)
    {
        /* hole / oil death: drawn where the obstacle was, at the obstacle's scale */
        if (deathObs >= 0)
        {
            float deathX = obs_x[deathObs] + obsAnim[obs_type[deathObs]].drawW / 2.0f;
            float deathY = (float)height * obstacleLaneCenter[obs_lane[deathObs]];

            if (obs_type[deathObs] == OBS_HOLE)
            {
                deathY -= 35.0f * gameScale;
            }

            if (obs_type[deathObs] == OBS_OIL)
            {
                deathY -= 35.0f * gameScale;
            }
            Anim_Draw(activeDeathAnim, deathX, deathY);
        }

        Anim_Draw(
            &chaseAnim,
            enemyX,
            enemyGround +
            BALL_Y_OFFSET * gameScale -
            chaseAnim.drawH / 2.0f
        );
    }
    else
    {
        float pinFeetY = PinFeetY(lane);
        float auraY = pinFeetY - runAnim.drawH / 2.0f;

        /* Effects that sit behind the pin. */
        if (invincibleTimer > 0.0f && puFx[PU_INVINCIBLE].sheet == NULL)
        {
            Draw_PowerFx(PU_INVINCIBLE, player_x, auraY);   /* circle fallback */
        }

        if (magnetTimer > 0.0f)
        {
            Draw_PowerFx(PU_MAGNET, player_x, auraY);
        }

        // Hit effect: flicker + knock-back
        int   player_alpha = 255;
        float player_draw_x = player_x;

        if (hurt_timer > 0.0f)
        {
            float frac = hurt_timer / hurt_duration;      // 1 right after the hit, 0 when recovered
            player_alpha = ((int)(hurt_timer * 14.0f) % 2) ? hurt_min_alpha : 255;   // flicker
            player_draw_x = player_x - stumble_lurch * gameScale * frac;              // knocked back, then slides forward
        }

        /* Draw the pin ONCE. While invincible or shielded, the effect sheet already
           contains the pin, so it is drawn instead of the normal run animation.
           Its frame follows runAnim so the two never go out of sync. */
        float pinCentreY = pinFeetY - runAnim.drawH / 2.0f;

        if (invincibleTimer > 0.0f && puFx[PU_INVINCIBLE].sheet != NULL)
        {
            const Animation* inv = &puFx[PU_INVINCIBLE];
            float invCentreY = pinFeetY - (INVINCIBLE_FEET_FRAC - 0.5f) * inv->drawH;

            Anim_DrawFrame(inv, runAnim.currentFrame % inv->frameCount,
                player_draw_x, invCentreY, player_alpha);
        }
        else if (shieldTimer > 0.0f && puFx[PU_SHIELD].sheet != NULL)
        {
            const Animation* sh = &puFx[PU_SHIELD];

            Anim_DrawFrame(sh, runAnim.currentFrame % sh->frameCount,
                player_draw_x, pinCentreY, player_alpha);
        }
        else
        {
            Anim_DrawAlpha(&runAnim, player_draw_x, pinCentreY, player_alpha);

            if (shieldTimer > 0.0f)
            {
                Draw_PowerFx(PU_SHIELD, player_draw_x, auraY);   /* circle fallback */
            }
        }

        // keep the ball anchored at its existing position.
        Anim_Draw(
            &chaseAnim,
            enemyX,
            enemyGround +
            BALL_Y_OFFSET * gameScale -
            chaseAnim.drawH / 2.0f
        );
    }

    // framed header panels for score, coins, multiplier, and distance.
    Draw_HUD();
    Draw_Roulette();

}

/* ============================================================
   Draw Pause Button
   ============================================================ */
static void DrawPixelButton(float x, float y, float w, float h, const char* label, int hovered)
{
    float p = 6.0f;                       // size of one "pixel"
    float left = x - w / 2.0f;
    float top = y - h / 2.0f;

    CP_Color outline = CP_Color_Create(20, 0, 0, 255);
    CP_Color face = hovered ? CP_Color_Create(170, 45, 45, 255) : CP_Color_Create(120, 30, 30, 255);
    CP_Color highlight = hovered ? CP_Color_Create(230, 110, 110, 255) : CP_Color_Create(190, 80, 80, 255);
    CP_Color shadow = CP_Color_Create(70, 10, 10, 255);

    CP_Settings_RectMode(CP_POSITION_CORNER);
    CP_Settings_StrokeWeight(0.0f);

    CP_Settings_Fill(outline);
    CP_Graphics_DrawRect(left, top, w, h);
    CP_Settings_Fill(face);
    CP_Graphics_DrawRect(left + p, top + p, w - 2 * p, h - 2 * p);
    CP_Settings_Fill(highlight);
    CP_Graphics_DrawRect(left + p, top + p, w - 2 * p, p);
    CP_Graphics_DrawRect(left + p, top + p, p, h - 2 * p);
    CP_Settings_Fill(shadow);
    CP_Graphics_DrawRect(left + p, top + h - 2 * p, w - 2 * p, p);
    CP_Graphics_DrawRect(left + w - 2 * p, top + p, p, h - 2 * p);

    CP_Settings_TextAlignment(CP_TEXT_ALIGN_H_CENTER, CP_TEXT_ALIGN_V_MIDDLE);
    CP_Settings_TextSize(h * 0.32f);
    CP_Settings_Fill(CP_Color_Create(0, 0, 0, 255));
    CP_Font_DrawText(label, x + 3.0f, y + 3.0f);
    CP_Settings_Fill(CP_Color_Create(255, 230, 230, 255));
    CP_Font_DrawText(label, x, y);

    //CP_Settings_StrokeWeight(1.0f);
    CP_Settings_NoStroke();
}

/* ============================================================
   Death screen
   ============================================================ */

static void DrawTextWithOutline(
    const char* text,
    float x,
    float y,
    float size,
    CP_Color fillColor,
    CP_Color strokeColor,
    float outlineWidth)
{
    CP_Settings_TextSize(size);
    CP_Settings_Fill(strokeColor);

    CP_Font_DrawText(text, x - outlineWidth, y - outlineWidth);
    CP_Font_DrawText(text, x, y - outlineWidth);
    CP_Font_DrawText(text, x + outlineWidth, y - outlineWidth);

    CP_Font_DrawText(text, x - outlineWidth, y);
    CP_Font_DrawText(text, x + outlineWidth, y);

    CP_Font_DrawText(text, x - outlineWidth, y + outlineWidth);
    CP_Font_DrawText(text, x, y + outlineWidth);
    CP_Font_DrawText(text, x + outlineWidth, y + outlineWidth);

    CP_Font_DrawText(text, x - outlineWidth * 1.4f, y);
    CP_Font_DrawText(text, x + outlineWidth * 1.4f, y);
    CP_Font_DrawText(text, x, y - outlineWidth * 1.4f);
    CP_Font_DrawText(text, x, y + outlineWidth * 1.4f);

    CP_Settings_Fill(fillColor);
    CP_Font_DrawText(text, x, y);
}
// Pill-shaped button in the same style as the main menu buttons.
// x, y = centre of the button.
static void DrawPillButtonAt(float x, float y, float w, float h, const char* label, int hovered)
{
    CP_Settings_RectMode(CP_POSITION_CENTER);
    CP_Settings_Stroke(CP_Color_Create(20, 20, 20, 255));
    CP_Settings_StrokeWeight(5.0f * HUD_SCALE);

    if (hovered)
        CP_Settings_Fill(CP_Color_Create(225, 75, 95, 255));
    else
        CP_Settings_Fill(CP_Color_Create(195, 65, 80, 255));

    CP_Graphics_DrawRectAdvanced(x, y, w, h, 0.0f, h / 2.0f);

    // highlight strip on the top half
    CP_Settings_NoStroke();
    CP_Settings_Fill(CP_Color_Create(255, 255, 255, 45));
    CP_Graphics_DrawRectAdvanced(x, y - h * 0.18f, w * 0.85f, h * 0.35f, 0.0f, h * 0.18f);

    // label
    if (pixel_font != NULL)
    {
        CP_Font_Set(pixel_font);
    }
    CP_Settings_TextAlignment(CP_TEXT_ALIGN_H_CENTER, CP_TEXT_ALIGN_V_MIDDLE);
    CP_Settings_TextSize(h * 0.28f);

    if (hovered)
        CP_Settings_Fill(CP_Color_Create(255, 255, 255, 255));
    else
        CP_Settings_Fill(CP_Color_Create(245, 245, 245, 255));

    CP_Font_DrawText(label, x, y);
}

static void DrawDiamondIcon(float x, float y)
{
    CP_Settings_Fill(CP_Color_Create(255, 210, 50, 255));
    CP_Graphics_DrawTriangle(x - 20.0f, y, x + 20.0f, y, x, y - 22.0f);
    CP_Graphics_DrawTriangle(x - 20.0f, y, x + 20.0f, y, x, y + 22.0f);
}

static void FormatDeathNumber(int value, char* output, size_t outputSize)
{
    char digits[32];
    size_t length;
    size_t written = 0;

    if (output == NULL || outputSize == 0)
    {
        return;
    }

    if (value < 0)
    {
        value = 0;
    }

    snprintf(digits, sizeof(digits), "%d", value);
    length = strlen(digits);

    for (size_t i = 0; i < length; i++)
    {
        if (i > 0 && (length - i) % 3 == 0)
        {
            if (written + 1 >= outputSize)
            {
                break;
            }
            output[written++] = ',';
        }

        if (written + 1 >= outputSize)
        {
            break;
        }
        output[written++] = digits[i];
    }

    output[written] = '\0';
}

void DeathScreen_Draw(const char* title)
{
    char scoreText[64];
    char multiplierText[32];
    char finalScoreText[64];
    char coinsText[64];

    int baseScore = (int)score;
    int finalScore = (int)(score * (float)scoreMultiplier);

    bool isNewHighScore =
        finalScore > 0 &&
        finalScore > highScoreAtStart;

    FormatDeathNumber(baseScore, scoreText, sizeof(scoreText));
    FormatDeathNumber(finalScore, finalScoreText, sizeof(finalScoreText));
    FormatDeathNumber(coinCount, coinsText, sizeof(coinsText));

    snprintf(
        multiplierText,
        sizeof(multiplierText),
        "x%d",
        scoreMultiplier
    );

    if (pixel_font != NULL)
    {
        CP_Font_Set(pixel_font);
    }

    CP_Settings_NoStroke();
    CP_Settings_RectMode(CP_POSITION_CORNER);

    // Dark red tint over gameplay
    CP_Settings_Fill(CP_Color_Create(40, 0, 0, 170));
    CP_Graphics_DrawRect(0.0f, 0.0f, WIN_W, WIN_H);

    // Main results panel
    float boxW = WIN_W * 0.76f;
    float boxH = WIN_H * 0.76f;
    float boxX = window_center_x - boxW * 0.5f;
    float boxY = WIN_H * 0.12f;

    float scale = fminf(
        WIN_W / 1280.0f,
        WIN_H / 720.0f
    );

    float borderThickness = 6.0f * scale;

    CP_Settings_Fill(CP_Color_Create(25, 5, 5, 215));
    CP_Graphics_DrawRect(boxX, boxY, boxW, boxH);

    CP_Settings_Fill(CP_Color_Create(180, 50, 50, 255));

    CP_Graphics_DrawRect(
        boxX,
        boxY,
        boxW,
        borderThickness
    );

    CP_Graphics_DrawRect(
        boxX,
        boxY + boxH - borderThickness,
        boxW,
        borderThickness
    );

    CP_Graphics_DrawRect(
        boxX,
        boxY,
        borderThickness,
        boxH
    );

    CP_Graphics_DrawRect(
        boxX + boxW - borderThickness,
        boxY,
        borderThickness,
        boxH
    );

    // GAME OVER title
    float titleSize = WIN_H * 0.065f;
    float titleGap = titleSize * 0.18f;
    float titleY = WIN_H * 0.205f;

    int titleLength = (int)strlen(title);

    float titleWidth =
        (float)titleLength * titleSize +
        (float)(titleLength > 0 ? titleLength - 1 : 0) * titleGap;

    float characterX =
        window_center_x -
        titleWidth * 0.5f +
        titleSize * 0.5f;

    CP_Settings_TextAlignment(
        CP_TEXT_ALIGN_H_CENTER,
        CP_TEXT_ALIGN_V_MIDDLE
    );

    for (int i = 0; i < titleLength; i++)
    {
        char character[2] = { title[i], '\0' };

        DrawTextWithOutline(
            character,
            characterX,
            titleY,
            titleSize,
            CP_Color_Create(255, 255, 255, 255),
            CP_Color_Create(0, 0, 0, 255),
            4.0f * scale
        );

        characterX += titleSize + titleGap;
    }

    // title underline
    CP_Settings_Fill(CP_Color_Create(255, 255, 255, 255));

    CP_Graphics_DrawRect(
        window_center_x - titleWidth * 0.5f,
        WIN_H * 0.265f,
        titleWidth,
        6.0f * scale
    );

    /*
     Four result rows:
     SCORE
     MULTIPLIER
     FINAL SCORE
     TOTAL COINS
    */
    float rowSize = WIN_H * 0.028f;
    float rowGap = WIN_H * 0.082f;

    float row1Y = WIN_H * 0.35f;
    float row2Y = row1Y + rowGap;
    float row3Y = row1Y + rowGap * 2.0f;
    float row4Y = row1Y + rowGap * 3.0f;

    // left-aligned X positions for the icon, label, and value in each row
    float resultsLeft = window_center_x - 225.0f * scale;
    float iconX = resultsLeft;
    float labelX = resultsLeft + 32.0f * scale;
    float valueX = resultsLeft + 310.0f * scale;

    CP_Color rowColor = CP_Color_Create(255, 220, 220, 255);
    CP_Color goldColor = CP_Color_Create(255, 220, 50, 255);

    CP_Settings_TextAlignment(
        CP_TEXT_ALIGN_H_LEFT,
        CP_TEXT_ALIGN_V_MIDDLE
    );

    CP_Settings_TextSize(rowSize);

    // row 1: base score before the multiplier
    DrawDiamondIcon(iconX, row1Y);

    CP_Settings_Fill(rowColor);
    CP_Font_DrawText("SCORE:", labelX, row1Y);
    CP_Font_DrawText(scoreText, valueX, row1Y);

    // If the player achieved a new high score, display a pulsing "NEW HIGH SCORE!" message above the results.
    if (isNewHighScore)
    {
        float pulse =
            1.0f +
            0.08f * sinf(CP_System_GetSeconds() * 6.0f);

        float tagSize = rowSize * 0.9f * pulse;

        // Calculate the width of the score text to position the "NEW!" tag correctly.
        float scoreWidth =
            (float)strlen(scoreText) * rowSize;

        float tagX =
            valueX +
            scoreWidth +
            12.0f * scale +
            2.0f * tagSize;

        CP_Settings_Translate(tagX, row1Y);
        CP_Settings_Rotate(-12.0f);

        CP_Settings_TextAlignment(
            CP_TEXT_ALIGN_H_CENTER,
            CP_TEXT_ALIGN_V_MIDDLE
        );

        DrawTextWithOutline(
            "NEW!",
            0.0f,
            0.0f,
            tagSize,
            CP_Color_Create(255, 220, 50, 255),
            CP_Color_Create(120, 20, 20, 255),
            2.0f * scale
        );

        CP_Settings_ResetMatrix();

        /* Restore text settings for the remaining result rows. */
        CP_Settings_TextAlignment(
            CP_TEXT_ALIGN_H_LEFT,
            CP_TEXT_ALIGN_V_MIDDLE
        );

        CP_Settings_TextSize(rowSize);
    }

    // row 2: accumulated score multiplier
    DrawDiamondIcon(iconX, row2Y);

    CP_Settings_Fill(rowColor);
    CP_Font_DrawText("MULTIPLIER:", labelX, row2Y);

    CP_Settings_Fill(goldColor);
    CP_Font_DrawText(multiplierText, valueX, row2Y);

    // row 3: score after applying the multiplier
    DrawDiamondIcon(iconX, row3Y);

    CP_Settings_Fill(rowColor);
    CP_Font_DrawText("FINAL SCORE:", labelX, row3Y);

    CP_Settings_Fill(goldColor);
    CP_Font_DrawText(finalScoreText, valueX, row3Y);

    // row 4: total coins collected
    CP_Settings_NoStroke();

    CP_Settings_Fill(CP_Color_Create(230, 150, 30, 255));
    CP_Graphics_DrawCircle(
        iconX,
        row4Y,
        32.0f * scale
    );

    CP_Settings_Fill(CP_Color_Create(255, 200, 60, 255));
    CP_Graphics_DrawCircle(
        iconX,
        row4Y,
        20.0f * scale
    );

    CP_Settings_Fill(rowColor);
    CP_Font_DrawText("TOTAL COINS:", labelX, row4Y);
    CP_Font_DrawText(coinsText, valueX, row4Y);

    // divider line between the results and the buttons
    CP_Settings_RectMode(CP_POSITION_CORNER);
    CP_Settings_Fill(CP_Color_Create(255, 255, 255, 255));

    CP_Graphics_DrawRect(
        boxX + boxW * 0.08f,
        WIN_H * 0.655f,
        boxW * 0.84f,
        4.0f * scale
    );

    DrawPillButtonAt(
        RESTART_X,
        BTN_Y,
        BTN_W,
        BTN_H,
        "RESTART (R)",
        GameMouseInButton(RESTART_X, BTN_Y, BTN_W, BTN_H)
    );

    DrawPillButtonAt(
        EXIT_X,
        BTN_Y,
        BTN_W,
        BTN_H,
        "QUIT (Q)",
        GameMouseInButton(EXIT_X, BTN_Y, BTN_W, BTN_H)
    );

    CP_Settings_NoStroke();
    CP_Settings_RectMode(CP_POSITION_CORNER);
}

///* ============================================================
//   Draw Pause Button
//   ============================================================ */
void DrawPauseButton(void)
{
    float s = HUD_SCALE;

    int hovered = GameMouseInButton(pause_button_x, pause_button_y, pause_button_size, pause_button_size);
    DrawHUDPanel(pause_button_x, pause_button_y, pause_button_size, pause_button_size, hovered);

    CP_Settings_RectMode(CP_POSITION_CENTER);
    CP_Settings_NoStroke();

    // Change the color of the pause button when hovered
    CP_Settings_Fill(
        hovered
        ? CP_Color_Create(255, 225, 100, 255)
        : CP_Color_Create(235, 235, 240, 255)
    );

    // Draw the two vertical bars of the pause icon
    CP_Graphics_DrawRect(pause_button_x - 9.0f * s, pause_button_y, 7.0f * s, 30.0f * s);
    CP_Graphics_DrawRect(pause_button_x + 9.0f * s, pause_button_y, 7.0f * s, 30.0f * s);
    CP_Settings_RectMode(CP_POSITION_CORNER);
}
/* ============================================================
   Pause Menu
   ============================================================ */
void PauseMenu_Draw(void)
{
    CP_Font_Set(pixel_font);
    CP_Settings_RectMode(CP_POSITION_CORNER);
    CP_Settings_Fill(CP_Color_Create(0, 0, 0, 150));
    CP_Graphics_DrawRect(0, 0, WIN_W, WIN_H);

    CP_Settings_TextAlignment(CP_TEXT_ALIGN_H_CENTER, CP_TEXT_ALIGN_V_MIDDLE);
    CP_Settings_TextSize(WIN_H * 0.08f);
    CP_Settings_Fill(CP_Color_Create(255, 230, 230, 255));
    CP_Font_DrawText("PAUSED", window_center_x, WIN_H * 0.28f);

    DrawPillButtonAt(window_center_x, pause_resume_y, PAUSE_BTN_W, BTN_H, "RESUME (SPACE)",
        GameMouseInButton(window_center_x, pause_resume_y, PAUSE_BTN_W, BTN_H));
    DrawPillButtonAt(window_center_x, pause_quit_y, PAUSE_BTN_W, BTN_H, "QUIT (Q)",
        GameMouseInButton(window_center_x, pause_quit_y, PAUSE_BTN_W, BTN_H));
}

/* ============================================================
    Draw Hearts [ADDED BY LEKKAR]
   ============================================================ */
void Draw_Hearts(void)
{
    // draw a panel behind the hearts to make them more visible against the background
    float s = HUD_SCALE;
    float x = 382.0f * s;
    float y = 48.0f * s;
    DrawHUDPanel(x, y, 180.0f * s, 60.0f * s, 0);

    CP_Settings_ImageMode(CP_POSITION_CENTER); // draw hearts from their centre

    if (heart_full != NULL) // draw the hearts only if the image loaded successfully
    {
        float heartSize = 44.0f * s;
        float spacing = 52.0f * s;

        float firstX =
            x - ((float)(max_lives - 1) * spacing) * 0.5f;

        for (int i = 0; i < max_lives; i++)
        {
            CP_Image_Draw(heart_full, firstX + (float)i * spacing, y, heartSize, heartSize, i < lives ? 255 : 60
            );
        }
    }

    CP_Settings_NoStroke();
    CP_Settings_RectMode(CP_POSITION_CORNER);
}

/* Pause or resume the game and its music. */
void SetPaused(int paused)
{
    is_paused = paused;

    if (paused) CP_Sound_PauseGroup(CP_SOUND_GROUP_MUSIC);
    else        CP_Sound_ResumeGroup(CP_SOUND_GROUP_MUSIC);
}


/* ============================================================
   Update
   ============================================================ */

void Game_Update(void)
{
    CP_Settings_ResetMatrix();
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

    /* ---------- Pause input ---------- */
    if (state == STATE_PLAYING && !show_death_screen)
    {
        if (CP_Input_KeyTriggered(KEY_SPACE))
        {
            SetPaused(!is_paused);
        }
        else if (CP_Input_MouseTriggered(MOUSE_BUTTON_1))
        {
            if (!is_paused &&
                GameMouseInButton(pause_button_x, pause_button_y, pause_button_size, pause_button_size))
            {
                SetPaused(1);
            }
            else if (is_paused)
            {
                if (GameMouseInButton(window_center_x, pause_resume_y, PAUSE_BTN_W, BTN_H))
                {
                    SetPaused(0);
                }
                else if (GameMouseInButton(window_center_x, pause_quit_y, PAUSE_BTN_W, BTN_H))
                {
                    CP_Engine_SetNextGameStateForced(
                        Main_Menu_Init, Main_Menu_Update, Main_Menu_Exit);
                    return;
                }
            }
        }
    }

    /* ---------- Gameplay (frozen while paused; drawing below still runs) ---------- */
    if (is_paused)
    {
        /* paused: nothing in the world updates */
    }
    else if (state == STATE_PLAYING)
    {
        runSurvivalSeconds += dt;  // excludes pause time and death animations.

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

        int currentScore = (int)(score * (float)scoreMultiplier);

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

        if (hurt_timer > 0.0f) hurt_timer -= dt;

        float speed_mult = 1.0f;
        if (hurt_timer > 0.0f)
        {
            float frac = hurt_timer / hurt_duration;      // 1 right after the hit, 0 when recovered
            float hold_end = 1.0f - stumble_hold;
            if (frac > hold_end)
                speed_mult = slow_factor;                 // stumble: hold slow
            else
                speed_mult = slow_factor + (1.0f - slow_factor) * (1.0f - frac / hold_end);   // recovery
        }
        /* Move and respawn obstacles. */
        for (int i = 0; i < MAX_OBS; i++)
        {
            float previousX = obs_x[i];
            float obsW = obsAnim[obs_type[i]].drawW;

            // convert base gameplay speed to screen pixels per second. (to have consistent gameplay speed across different window sizes)
            // speed_mult slows the world briefly after a hit (stumble + recovery).
            obs_x[i] -= speed * gameScale * speed_mult * dt;

            ObsFx_Update(i, dt);   /* oil spawn / barrier break animation */

            /* swept collision check */
            float playerHalfWidth = runAnim.drawW * 0.30f;
            float playerLeft = player_x - playerHalfWidth;
            float playerRight = player_x + playerHalfWidth;

            bool crossedPlayer =
                obs_x[i] <= playerRight &&
                previousX + obsW >= playerLeft;

            if (obs_lane[i] == lane && crossedPlayer && !obs_hit[i])
            {
                obs_hit[i] = 1;   /* one obstacle only costs one heart */

                if (HandleHit(i))
                {
                    break;        /* the pin died */
                }
            }

            /* Respawn behind the furthest obstacle. */
            if (obs_x[i] + obsW < 0.0f)
            {
                RespawnObstacle(i);

            }
        }
        /* Power-ups and coins. */
        if (state == STATE_PLAYING)
        {
            invincibleTimer = fmaxf(0.0f, invincibleTimer - dt);
            shieldTimer = fmaxf(0.0f, shieldTimer - dt);
            magnetTimer = fmaxf(0.0f, magnetTimer - dt);

            Anim_Update(&coinAnim);
            Anim_Update(&boxAnim);

            /* Item roulette: icons spin, slow down, then land on the real power-up. */
            if (rouletteTimer > 0.0f)
            {
                rouletteTimer -= dt;
                rouletteSwitch -= dt;

                if (rouletteTimer <= 0.0f)
                {
                    /* the spin is over: show the real power-up and give it */
                    rouletteShown = rouletteType;
                    ApplyPowerUp(rouletteType);
                    revealTimer = REVEAL_TIME;
                }
                else if (rouletteSwitch <= 0.0f)
                {
                    /* change the icon; changes get slower towards the end */
                    float progress = 1.0f - rouletteTimer / ROULETTE_TIME;

                    rouletteShown = (rouletteShown + 1) % NUM_POWERUPS;
                    rouletteSwitch = 0.06f + 0.14f * progress;
                }
            }
            else if (revealTimer > 0.0f)
            {
                revealTimer -= dt;
            }

            for (int t = 0; t < NUM_POWERUPS; t++)
            {
                Anim_Update(&puIconAnim[t]);
            }

            if (invincibleTimer > 0.0f)
            {
                Anim_Update(&puFx[PU_INVINCIBLE]);
            }

            if (shieldTimer > 0.0f)
            {
                Anim_Update(&puFx[PU_SHIELD]);
            }

            if (magnetTimer > 0.0f)
            {
                Anim_Update(&puFx[PU_MAGNET]);
            }

            float worldMove = speed * gameScale * speed_mult * dt;

            /* Power-up pickup scrolls with the world. */
            if (pu_active)
            {
                pu_x -= worldMove;

                if (pu_lane == lane &&
                    fabsf(pu_x - player_x) < PU_PICKUP_DIST * gameScale &&
                    rouletteTimer <= 0.0f)
                {
                    StartRoulette(pu_type);
                    pu_active = false;
                }
                else if (pu_x < -100.0f * gameScale)
                {
                    pu_active = false;
                }
            }
            else
            {
                puSpawnTimer -= dt;

                if (puSpawnTimer <= 0.0f)
                {
                    TrySpawnPickup();
                }
            }

            /* Coins. */
            coinSpawnTimer -= dt;

            if (coinSpawnTimer <= 0.0f)
            {
                TrySpawnCoins();
            }

            float playerCentreY = PlayerCenterY(lane);

            for (int c = 0; c < MAX_COINS; c++)
            {
                if (!coin_active[c])
                {
                    continue;
                }

                coin_x[c] -= worldMove;

                float cdx = player_x - coin_x[c];
                float cdy = playerCentreY - coin_y[c];
                float cdist = sqrtf(cdx * cdx + cdy * cdy);

                /* Magnet pulls nearby coins towards the pin. */
                if (magnetTimer > 0.0f &&
                    cdist < MAGNET_RANGE * gameScale &&
                    cdist > 1.0f)
                {
                    float pull = MAGNET_PULL_SPEED * gameScale * dt;

                    coin_x[c] += cdx / cdist * pull;
                    coin_y[c] += cdy / cdist * pull;
                }

                if (cdist < COIN_COLLECT_DIST * gameScale)
                {
                    CP_Sound_PlayAdvanced(sndCoin, COIN_VOL, 1.0f, false, CP_SOUND_GROUP_SFX);
                    coin_active[c] = false;

                    coinCount++;
                    score += (float)COIN_SCORE;
                }
                else if (coin_x[c] < -50.0f * gameScale)
                {
                    coin_active[c] = false;
                }
            }

            // [ADDED BY LEKKAR] heart pickup: update position, check for collection, and respawn if missing
            if (heart_active)
            {
                heart_x -= worldMove;

                if (heart_lane == lane &&
                    fabsf(heart_x - player_x) < HEART_PICKUP_DIST * gameScale)
                {
                    if (lives < max_lives)
                    {
                        lives++;
                    }

                    heart_active = false;

                    if (sndHeart != NULL)
                    {
                        CP_Sound_PlayAdvanced(sndHeart, HEART_VOL, 1.0f, false, CP_SOUND_GROUP_SFX);
                    }
                    else if (sndCoin != NULL)
                    {
                        CP_Sound_PlayAdvanced(sndCoin, COIN_VOL, 1.0f, false, CP_SOUND_GROUP_SFX);
                    }
                }
                else if (heart_x < -100.0f * gameScale)
                {
                    heart_active = false;
                }
            }
            else if (lives < max_lives)
            {
                heartSpawnTimer -= dt;

                if (heartSpawnTimer <= 0.0f)
                {
                    TrySpawnHeart();
                }
            }

            if (multiFx.sheet != NULL)
            {
                Anim_Update(&multiFx);
            }

            if (!multiplier_active)
            {
                multiplierSpawnTimer -= dt;

                if (multiplierSpawnTimer <= 0.0f)
                {
                    SpawnMultiplier();
                }
            }
            else
            {
                multiplier_x -= worldMove;

                float sdx = player_x - multiplier_x;
                float sdy = playerCentreY - multiplier_y;
                float sdist = sqrtf(sdx * sdx + sdy * sdy);

                if (sdist < MULT_COLLECT_DIST * gameScale)
                {
                    multiplier_active = false;

                    if (scoreMultiplier < MAX_MULT)
                    {
                        scoreMultiplier++;
                    }
                }
                else if (multiplier_x < -50.0f * gameScale)
                {
                    multiplier_active = false;
                }
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
    else if (state == STATE_DYING)
    {
        /* hole / oil death: only the pin animation plays (world is frozen) */
        if (activeDeathAnim != NULL)
        {
            Anim_Update(activeDeathAnim);
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

    if (runResultReady)
    {
        if (!telemetryEnabled)
        {
            runResultReady = false;
        }
        else if (Telemetry_SubmitRun(
            (int)score,
            runSurvivalSeconds,
            coinCount,
            runDeathCause))
        {
            /* The upload worker accepted this run. */
            runResultReady = false;
        }
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

    bool dyingDone =
        state == STATE_DYING &&
        activeDeathAnim != NULL &&
        activeDeathAnim->currentFrame == activeDeathAnim->frameCount - 1;

    if (grabDone || dyingDone)
    {
        //Draw_DeathScreen();
        show_death_screen = 1;
    }

    /* ---------- Death screen input ---------- */
    if (show_death_screen)
    {
        if (CP_Input_KeyTriggered(KEY_R))
        {
            CP_Engine_SetNextGameStateForced(Game_Init, Game_Update, Game_Exit);
            return;
        }

        if (CP_Input_MouseTriggered(MOUSE_BUTTON_1))
        {
            if (GameMouseInButton(RESTART_X, BTN_Y, BTN_W, BTN_H))
            {
                CP_Engine_SetNextGameStateForced(Game_Init, Game_Update, Game_Exit);
                return;
            }
            else if (GameMouseInButton(EXIT_X, BTN_Y, BTN_W, BTN_H))
            {
                CP_Engine_SetNextGameStateForced(
                    Main_Menu_Init, Main_Menu_Update, Main_Menu_Exit);
                return;
            }
        }
    }

    Draw_Hearts();

    if (!is_paused && !show_death_screen)
    {
        DrawPauseButton();
    }

    if (is_paused)
    {
        PauseMenu_Draw();
    }

    /* Drawn last so it sits on top of everything. */
    if (show_death_screen)
    {
        DeathScreen_Draw(death_title);
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

    if (sndHit != NULL)
    {
        CP_Sound_Free(sndHit);
        sndHit = NULL;
    }

    if (bgImage != NULL)
    {
        CP_Image_Free(bgImage);
        bgImage = NULL;
    }

    // [ADDED BY LEKKAR] Free heart pickup resources
    if (heartPickupImg != NULL)
    {
        CP_Image_Free(heartPickupImg);
        heartPickupImg = NULL;
    }

    if (sndHeart != NULL)
    {
        CP_Sound_Free(sndHeart);
        sndHeart = NULL;
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

    Animation* fxAnims[4] = { &oilSpawnAnim, &barrierBreakAnim, &holeDeathAnim, &oilDeathAnim };

    for (int t = 0; t < 4; t++)
    {
        if (fxAnims[t]->sheet != NULL)
        {
            CP_Image_Free(fxAnims[t]->sheet);
            fxAnims[t]->sheet = NULL;
        }
    }

    activeDeathAnim = NULL;

    if (coinAnim.sheet != NULL)
    {
        CP_Image_Free(coinAnim.sheet);
        coinAnim.sheet = NULL;
    }

    if (boxAnim.sheet != NULL)
    {
        CP_Image_Free(boxAnim.sheet);
        boxAnim.sheet = NULL;
    }

    for (int t = 0; t < NUM_POWERUPS; t++)
    {
        if (puIconAnim[t].sheet != NULL)
        {
            CP_Image_Free(puIconAnim[t].sheet);
            puIconAnim[t].sheet = NULL;
        }

        if (puFx[t].sheet != NULL)
        {
            CP_Image_Free(puFx[t].sheet);
            puFx[t].sheet = NULL;
        }
    }

    if (heart_full != NULL)
    {
        CP_Image_Free(heart_full);
        heart_full = NULL;
    }

    if (hudCoinImage != NULL)
    {
        CP_Image_Free(hudCoinImage);
        hudCoinImage = NULL;
    }

    rageEventActive = false;
    rageEventTimer = 0.0f;
    rageSpawnTimer = 0.0f;

    rageFont = NULL;
    scoreFont = NULL;
    deathFont = NULL;
}