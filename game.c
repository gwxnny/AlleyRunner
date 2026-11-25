/*!
@file       : game.c
@author     : Isaac Tan (isaacd.tan@digipen.edu) - lane movement, speed curve, scoring, coins, multiplier, hole death, death flow, chase distance, debug hooks
@co-author  : Ggwendolynn Lee Rasni (ggwendolynnlee.r@digipen.edu) - obstacles (barrier, oil, hole), barrier collision
              Lek Kar Lim (lekkar.lim@digipen.edu) - hearts, damage, heart pickups, hit flicker and stumble, pause flow
              Ang Xiao Hui (a.xiaohui@digipen.edu) - character, coin, power-up and death animations, mystery box
@team       : IsaacACat
@module     : CSD1401 Software Engineering Project 1
@section    : B
@project    : Alley Runner
@brief      : Gameplay state. The pin runs in one of three lanes while the world
              scrolls left and gets faster. Barriers, oil and holes cost hearts;
              each heart lost lets the bowling ball roll closer. Coins, mystery
              boxes (power-ups), hearts and multiplier balls can be collected.

All content © 2026 DigiPen Institute of Technology Singapore. All Rights Reserved.
*//*________________________________________________________________________________*/
#include "cprocessing.h"
#include "game.h"
#include "game_types.h"
#include "game_menus.h"
#include "hud.h"
#include "animation.h"
#include "rage_overlay.h"
#include "tuning.h"
#include "debug_tools.h"
#include "transition.h"
#include "mainmenu.h"
#include "ui.h"

#include <math.h>
#include <stdbool.h>
#include <stdio.h>

/* ============================================================
   Settings
   ============================================================ */

#define NUM_LANES 3

   /* Obstacles */
#define MAX_OBS       4
#define NUM_OBS_TYPES 3
#define OBS_SRC_W     500.0f   /* obstacle source-image size */
#define OBS_SRC_H     500.0f
#define OBS_LANE_FILL 0.6f     /* obstacle height as a fraction of a lane's height */
#define OIL_SPAWN_AT  0.97f    /* oil starts its spawn animation once it is this far on screen */

/* All sizes were designed for a 720-pixel-high window. */
#define BASE_GAME_HEIGHT 720.0f

/* Characters (sprite-sheet frame sizes) */
#define PLAYER_W 120.0f
#define PLAYER_H 120.0f
#define ENEMY_W  500.0f
#define ENEMY_H  500.0f
#define GRAB_FRAME_W 1000.0f
#define GRAB_FRAME_H 500.0f
#define GRAB_PIN_SRC_X    733.0f  /* where the pin is in the first frame of the grab sheet */
#define GRAB_PIN_SRC_FEET 475.0f
#define FEET_OFFSET  80.0f        /* pin feet below the lane line */
#define FEET_PADDING 7.0f         /* empty pixels under the pin's feet in its sheet */
#define PLAYER_X_FRACTION 0.4f    /* pin's x as a fraction of the window width */
#define PLAYER_HIT_HALF_WIDTH 0.30f /* hit width either side of the pin, fraction of its draw width */

#define DISTANCE_DIVISOR 40.0f    /* lower = distance in metres grows faster */
#define SCORE_DIVISOR    100.0f   /* lower = score grows faster */


/* Death */
#define DEATH_FRAME_TIME     0.08f
#define DEATH_SCREEN_DELAY   0.45f  /* pause on the last death frame before the results */
#define DEATH_ANIM_RAISE     35.0f  /* hole / oil death sheets sit slightly higher */
#define HOLE_DEATH_SCALE     1.45f

/* Hit reaction: flicker, slow-down and knock-back after losing a heart */
#define HURT_DURATION  1.2f    /* total time of the hit effect (seconds) */
#define HURT_MIN_ALPHA 70      /* how faint the pin gets while flickering */
#define HURT_FLICKER_RATE 14.0f
#define SLOW_FACTOR    0.45f   /* world speed right after a hit (1.0 = normal) */
#define STUMBLE_HOLD   0.4f    /* fraction of HURT_DURATION spent at the slow speed */
#define STUMBLE_LURCH  25.0f   /* pixels the pin is knocked back */

/* Power-ups and mystery box */
#define INVINCIBLE_ANIM_SCALE 1.35f
#define INVINCIBLE_FEET_FRAC  0.88f
#define PU_SIZE        70.0f   /* mystery box size */
#define PU_PICKUP_DIST 60.0f
#define PU_FX_SIZE     190.0f  /* height of an effect drawn on the pin */
#define ROULETTE_TIME  1.5f    /* seconds the icons spin after a box is collected */
#define REVEAL_TIME    0.8f    /* seconds the result stays on screen */
#define REVIVE_ANIM_TIME 1.5f  /* angel pin shown after collecting a heart */

/* Coins */
#define MAX_COINS         15
#define COIN_PER_SPAWN    3
#define COIN_SPACING      90.0f
#define COIN_SIZE         90.0f
#define COIN_FRAMES       7
#define COIN_SRC_W        500.0f
#define COIN_SRC_H        500.0f
#define COIN_FRAME_TIME   0.10f
#define COIN_COLLECT_DIST 60.0f
#define FIRST_COIN_DELAY  2.0f

/* Multiplier ball */
#define MULT_SIZE         70.0f
#define MULT_COLLECT_DIST 60.0f
#define MAX_MULT          5
#define MAGNET_PULL_SPEED 1400.0f

/* Heart pickup */
#define HEART_PICKUP_SIZE     120.0f
#define HEART_PICKUP_DIST     60.0f
#define HEART_FIRST_DELAY_MIN 3.0f   /* seconds after losing a heart */
#define HEART_FIRST_DELAY_MAX 6.0f

/* Spawning: new items appear just off the right edge, only in a clear lane. */
#define SPAWN_TRIES         6
#define SPAWN_OFFSCREEN_X   100.0f
#define PICKUP_CLEAR_MARGIN 300.0f
#define COIN_CLEAR_MARGIN   400.0f

/* Audio volumes */
#define JUMP_VOL       0.5f
#define DEATH_VOL      1.0f
#define COIN_VOL       1.5f
#define HIT_VOL        1.0f
#define HEART_VOL      0.4f
#define MULT_VOL       0.4f
#define ROULETTE_VOL   1.5f
#define GAME_MUSIC_VOL 1.0f

/* ============================================================
   Types
   ============================================================ */

typedef enum
{
    STATE_PLAYING,
    STATE_GRAB,     /* the ball catches the pin */
    STATE_DYING     /* the pin plays a hole / oil death in place */
} GameplayPhase;

typedef enum
{
    OBS_BARRIER,
    OBS_OIL,
    OBS_HOLE
} ObsKind;

typedef enum
{
    OBS_PH_NORMAL,      /* normal sprite */
    OBS_PH_WAITING,     /* oil that has not appeared on screen yet */
    OBS_PH_SPAWN,       /* oil playing its spawn animation */
    OBS_PH_BROKEN       /* barrier playing its break animation, then holding the last frame */
} ObsPhase;

/* Layout of a sprite sheet, so art can be swapped by editing one line. */
typedef struct
{
    const char* path;
    int frames;
    float w;            /* size of ONE frame in the PNG */
    float h;
    float frameTime;    /* seconds per frame */
} SheetInfo;

/* ============================================================
   Art tables
   ============================================================ */

static const char* obsPaths[NUM_OBS_TYPES] =
{
    "Assets/barrier.png",
    "Assets/Oil Bubble.png",
    "Assets/hole.png"
};

static const SheetInfo oilSpawnInfo = { "Assets/Oil Bubble.png",    4, 500.0f, 500.0f, 0.08f };
static const SheetInfo barrierBreakInfo = { "Assets/Barrier Break.png", 4, 500.0f, 500.0f, 0.07f };
static const SheetInfo holeDeathInfo = { "Assets/Hole Fall.png",     5, 500.0f, 500.0f, 0.10f };
static const SheetInfo oilDeathInfo = { "Assets/Pin Fall.png",      4, 500.0f, 500.0f, 0.10f };

/* Power-up icons (used in the roulette) and effects drawn on the pin. */
static const SheetInfo puIconInfo[NUM_POWERUPS] =
{
    { "Assets/invincible.PNG",    1, 640.0f, 640.0f, 0.12f },
    { "Assets/pickup_shield.png", 1, 500.0f, 500.0f, 0.12f },
    { "Assets/magnet.PNG",        1, 640.0f, 640.0f, 0.12f }
};

static const SheetInfo puFxInfo[NUM_POWERUPS] =
{
    { "Assets/invincible_anim.png",  4, 384.0f, 426.0f, 0.15f },
    { "Assets/Pin Shield.png",       4, 500.0f, 500.0f, 0.10f },
    { "Assets/Magnet-animation.png", 4, 120.0f, 120.0f, 0.15f }
};

static const char* puLabels[NUM_POWERUPS] = { "I", "S", "M" };
static const char* puNames[NUM_POWERUPS] = { "INVINCIBLE", "SHIELD", "MAGNET" };

/* Pin height offset per lane, and obstacle centre per lane (fraction of the window). */
static const float pinYOffset[NUM_LANES] = { 50.0f, 0.0f, -50.0f };
static const float obstacleLaneCenter[NUM_LANES] = { 0.28f, 0.55f, 0.82f };

/* ============================================================
   State
   ============================================================ */

/* Window layout */
static int width;
static int height;
static float gameScale = 1.0f;   /* window height / BASE_GAME_HEIGHT */
static float laneGap;
static float topLaneY;
static float playerX;

/* Run */
static GameplayPhase state;
static float speed;
static int lane;
static int lives = MAX_LIVES;
static float score = 0.0f;
static float distanceMetres = 0.0f;
static int highScore = 0;
static int highScoreAtStart = 0;
static int coinCount = 0;
static int scoreMultiplier = 1;
static const char* deathCauseText = NULL;

/* Flow */
static bool isPaused = false;
static bool showDeathScreen = false;
static bool leavingGame = false;    /* a restart / main menu transition has started */
static float deathHoldTimer = 0.0f;

/* Hit reaction */
static float hurtTimer = 0.0f;
static float reviveAnimTimer = 0.0f;

/* Chase */
static float enemyX;
static float enemyGround;

/* Background */
static CP_Image bgImage = NULL;
static float bgScroll = 0.0f;

/* Characters */
static Animation runAnim;
static Animation chaseAnim;
static Animation grabAnim;
static Animation reviveAnim;

/* Obstacles */
static float obsX[MAX_OBS];
static int obsLane[MAX_OBS];
static int obsType[MAX_OBS];
static bool obsHit[MAX_OBS];      /* one obstacle only costs one heart */
static int obsPhase[MAX_OBS];
static int obsFrame[MAX_OBS];
static float obsTimer[MAX_OBS];
static Animation obsAnim[NUM_OBS_TYPES];
static Animation oilSpawnAnim;
static Animation barrierBreakAnim;
static Animation holeDeathAnim;
static Animation oilDeathAnim;
static Animation* activeDeathAnim = NULL;
static int deathObs = -1;

/* Power-ups */
static float invincibleTimer = 0.0f;
static float shieldTimer = 0.0f;
static float magnetTimer = 0.0f;
static Animation puIconAnim[NUM_POWERUPS];
static Animation puFx[NUM_POWERUPS];

/* Mystery box (one on screen at a time) and the roulette it starts */
static CP_Image boxImg = NULL;
static bool puActive = false;
static float puX = 0.0f;
static int puLane = 0;
static float puSpawnTimer = 0.0f;
static float rouletteTimer = 0.0f;
static float rouletteSwitch = 0.0f;
static int rouletteShown = 0;
static int rouletteType = 0;
static float revealTimer = 0.0f;

/* Coins */
static float coinX[MAX_COINS];
static float coinY[MAX_COINS];
static bool coinActive[MAX_COINS];
static float coinSpawnTimer = 0.0f;
static Animation coinAnim;

/* Heart pickup (one on screen at a time) */
static CP_Image heartPickupImg = NULL;
static bool heartActive = false;
static float heartX = 0.0f;
static int heartLane = 0;
static float heartSpawnTimer = 0.0f;

/* Multiplier ball (one on screen at a time) */
static CP_Image multiplierItemImg = NULL;
static bool multiplierActive = false;
static float multiplierX = 0.0f;
static float multiplierY = 0.0f;
static float multiplierSpawnTimer = 0.0f;

/* Debug */
static int debugNextPowerUp = 0;

/* Audio */
static CP_Sound sndJump = NULL;
static CP_Sound sndCoin = NULL;
static CP_Sound gameMusic = NULL;
static CP_Sound sndDeath = NULL;
static CP_Sound sndHit = NULL;
static CP_Sound sndHeart = NULL;
static CP_Sound sndRouletteSpin = NULL;
static CP_Sound sndRouletteReveal = NULL;
static CP_Sound sndMultiplier = NULL;

/* ============================================================
   Small helpers
   ============================================================ */

/*! Specification for function RandomInt:
@brief Gets a random whole number in a range.

Wraps CProcessing's unsigned random function so gameplay code can use ints.

@param lowest - The smallest possible value.
@param highest - The largest possible value.
@return A random value from lowest to highest (inclusive).
*//*________________________________________________________________________________*/
static int RandomInt(int lowest, int highest)
{
    return (int)CP_Random_RangeInt((unsigned int)lowest, (unsigned int)highest);
}

/*! Specification for function PlaySfx:
@brief Plays a sound effect on the SFX group, if it loaded.

@param sound - The sound to play.
@param volume - The playback volume.
@return none.
*//*________________________________________________________________________________*/
static void PlaySfx(CP_Sound sound, float volume)
{
    if (sound != NULL)
    {
        CP_Sound_PlayAdvanced(sound, volume, 1.0f, false, CP_SOUND_GROUP_SFX);
    }
}

/*! Specification for function SetPaused:
@brief Pauses or resumes the game and its music.

@param paused - true to pause, false to resume.
@return none.
*//*________________________________________________________________________________*/
static void SetPaused(bool paused)
{
    isPaused = paused;
    GameMenus_Reset();

    if (paused)
    {
        CP_Sound_PauseGroup(CP_SOUND_GROUP_MUSIC);
    }
    else
    {
        CP_Sound_ResumeGroup(CP_SOUND_GROUP_MUSIC);
    }
}

/*! Specification for function FinalScore:
@brief Gets the score after the multiplier.

@param none.
@return The score multiplied by the current score multiplier.
*//*________________________________________________________________________________*/
static int FinalScore(void)
{
    return (int)(score * (float)scoreMultiplier);
}

/* ============================================================
   World helpers (lanes, obstacles, spawning)
   ============================================================ */

/*! Specification for function PinFeetY:
@brief Gets the vertical position of the pin's feet in a lane.

The bottom lane is anchored to the bottom of the screen and the other lanes
are placed relative to it.

@param laneIndex - The lane (0 = top, 2 = bottom).
@return The y position of the pin's feet.
*//*________________________________________________________________________________*/
static float PinFeetY(int laneIndex)
{
    float bottomFeet =
        topLaneY + (float)(NUM_LANES - 1) * laneGap +
        (FEET_OFFSET + pinYOffset[NUM_LANES - 1]) * gameScale;

    float laneDifference = obstacleLaneCenter[laneIndex] - obstacleLaneCenter[NUM_LANES - 1];

    return bottomFeet + (float)height * laneDifference;
}

/*! Specification for function PlayerCenterY:
@brief Gets the vertical centre of the pin in a lane.

@param laneIndex - The lane (0 = top, 2 = bottom).
@return The y position of the pin's centre.
*//*________________________________________________________________________________*/
static float PlayerCenterY(int laneIndex)
{
    return PinFeetY(laneIndex) - runAnim.drawH / 2.0f;
}

/*! Specification for function ObstacleCenterX:
@brief Gets the horizontal centre of an obstacle.

@param i - The obstacle's index.
@return The x position of the obstacle's centre.
*//*________________________________________________________________________________*/
static float ObstacleCenterX(int i)
{
    return obsX[i] + obsAnim[obsType[i]].drawW / 2.0f;
}

/*! Specification for function ObstacleCenterY:
@brief Gets the vertical centre of an obstacle.

@param i - The obstacle's index.
@return The y position of the obstacle's centre.
*//*________________________________________________________________________________*/
static float ObstacleCenterY(int i)
{
    return (float)height * obstacleLaneCenter[obsLane[i]];
}

/*! Specification for function RandomiseObstacle:
@brief Gives an obstacle a random type and lane and resets its animation.

Oil starts in its spawn animation.

@param i - The obstacle's index.
@return none.
*//*________________________________________________________________________________*/
static void RandomiseObstacle(int i)
{
    obsType[i] = RandomInt(0, NUM_OBS_TYPES - 1);
    obsLane[i] = RandomInt(0, NUM_LANES - 1);
    obsPhase[i] = OBS_PH_NORMAL;
    obsFrame[i] = 0;
    obsTimer[i] = 0.0f;

    /* Oil plays its spawn animation as it appears. */
    if (obsType[i] == OBS_OIL && oilSpawnAnim.sheet != NULL)
    {
        obsPhase[i] = OBS_PH_SPAWN;
    }
}

/*! Specification for function RespawnObstacle:
@brief Moves an obstacle behind the furthest one and randomises it.

The gap grows with speed, so the player always has the same reaction time.

@param i - The obstacle's index.
@return none.
*//*________________________________________________________________________________*/
static void RespawnObstacle(int i)
{
    const GameTuning* tuning = Tuning_Get();
    float furthest = (float)width;

    for (int j = 0; j < MAX_OBS; j++)
    {
        if (obsX[j] > furthest)
        {
            furthest = obsX[j];
        }
    }

    obsX[i] = furthest + tuning->obstacleSpacing * gameScale * (speed / tuning->startSpeed);

    RandomiseObstacle(i);
    obsHit[i] = false;
}

/*! Specification for function LaneClearAt:
@brief Checks whether a lane has no obstacle near a position.

@param x - The horizontal position to check.
@param laneIndex - The lane to check.
@param margin - How far away an obstacle must be, in pixels.
@return true - If no obstacle in the lane is within margin of x, otherwise false.
*//*________________________________________________________________________________*/
static bool LaneClearAt(float x, int laneIndex, float margin)
{
    for (int i = 0; i < MAX_OBS; i++)
    {
        if (obsLane[i] == laneIndex && fabsf(ObstacleCenterX(i) - x) < margin)
        {
            return false;
        }
    }

    return true;
}

/*! Specification for function BreakObstacle:
@brief Starts a barrier's break animation.

The broken barrier holds its last frame and keeps scrolling until it respawns.
If the break art is missing, the barrier respawns straight away.

@param i - The obstacle's index.
@return none.
*//*________________________________________________________________________________*/
static void BreakObstacle(int i)
{
    if (barrierBreakAnim.sheet != NULL)
    {
        obsPhase[i] = OBS_PH_BROKEN;
        obsFrame[i] = 0;
        obsTimer[i] = 0.0f;
    }
    else
    {
        RespawnObstacle(i);
    }
}

/*! Specification for function UpdateObstacleFx:
@brief Advances an obstacle's oil spawn or barrier break animation.

Oil keeps looping its animation; a broken barrier stops on its last frame.

@param i - The obstacle's index.
@param dt - The time passed since the previous frame, in seconds.
@return none.
*//*________________________________________________________________________________*/
static void UpdateObstacleFx(int i, float dt)
{
    const Animation* anim;

    if (obsPhase[i] == OBS_PH_WAITING)
    {
        /* start the spawn animation once the whole spill is on screen */
        if (obsX[i] + obsAnim[obsType[i]].drawW < (float)width * OIL_SPAWN_AT)
        {
            obsPhase[i] = OBS_PH_SPAWN;
            obsFrame[i] = 0;
            obsTimer[i] = 0.0f;
        }
        return;
    }

    if (obsPhase[i] == OBS_PH_SPAWN)
    {
        anim = &oilSpawnAnim;
    }
    else if (obsPhase[i] == OBS_PH_BROKEN)
    {
        anim = &barrierBreakAnim;
    }
    else
    {
        return;
    }

    if (anim->frameTime <= 0.0f || anim->frameCount <= 0)
    {
        return;
    }

    obsTimer[i] += dt;

    while (obsTimer[i] >= anim->frameTime)
    {
        obsTimer[i] -= anim->frameTime;

        if (obsFrame[i] < anim->frameCount - 1)
        {
            obsFrame[i]++;
        }
        else if (obsPhase[i] == OBS_PH_SPAWN)
        {
            obsFrame[i] = 0;      /* oil keeps looping */
        }
        else
        {
            obsTimer[i] = 0.0f;   /* a broken barrier holds its last frame */
            break;
        }
    }
}

/*! Specification for function ApplyPowerUp:
@brief Starts a power-up's timer.

Durations come from the tuning file.

@param type - The power-up (invincible, shield or magnet).
@return none.
*//*________________________________________________________________________________*/
static void ApplyPowerUp(int type)
{
    const GameTuning* tuning = Tuning_Get();

    switch (type)
    {
    case PU_INVINCIBLE: invincibleTimer = tuning->invincibleTime; break;
    case PU_SHIELD:     shieldTimer = tuning->shieldTime;         break;
    default:            magnetTimer = tuning->magnetTime;         break;
    }
}

/*! Specification for function StartRoulette:
@brief Starts the spinning power-up icons after a mystery box is collected.

@param type - The power-up the player will receive when the spin ends.
@return none.
*//*________________________________________________________________________________*/
static void StartRoulette(int type)
{
    rouletteType = type;
    rouletteTimer = ROULETTE_TIME;
    rouletteSwitch = 0.0f;
    rouletteShown = RandomInt(0, NUM_POWERUPS - 1);
    revealTimer = 0.0f;
    PlaySfx(sndRouletteSpin, ROULETTE_VOL);
}

/*! Specification for function SpawnX:
@brief Gets the x position where new pickups appear.

@param none.
@return A position just off the right edge of the screen.
*//*________________________________________________________________________________*/
static float SpawnX(void)
{
    return (float)width + SPAWN_OFFSCREEN_X * gameScale;
}

/*! Specification for function TrySpawnPickup:
@brief Tries to place a mystery box in a lane with no obstacle nearby.

If no lane is clear, it tries again next frame.

@param none.
@return none.
*//*________________________________________________________________________________*/
static void TrySpawnPickup(void)
{
    const GameTuning* tuning = Tuning_Get();

    for (int tries = 0; tries < SPAWN_TRIES; tries++)
    {
        int laneIndex = RandomInt(0, NUM_LANES - 1);

        if (LaneClearAt(SpawnX(), laneIndex, PICKUP_CLEAR_MARGIN * gameScale))
        {
            puActive = true;
            puX = SpawnX();
            puLane = laneIndex;
            puSpawnTimer = CP_Random_RangeFloat(tuning->powerUpSpawnMin, tuning->powerUpSpawnMax);
            return;
        }
    }
    /* No clear lane right now: try again next frame. */
}

/*! Specification for function TrySpawnCoins:
@brief Tries to place a row of coins in a lane with no obstacle nearby.

@param none.
@return none.
*//*________________________________________________________________________________*/
static void TrySpawnCoins(void)
{
    const GameTuning* tuning = Tuning_Get();

    for (int tries = 0; tries < SPAWN_TRIES; tries++)
    {
        int laneIndex = RandomInt(0, NUM_LANES - 1);
        float x = SpawnX();
        float rowCentre = x + ((float)COIN_PER_SPAWN * COIN_SPACING * gameScale) / 2.0f;
        int placed = 0;

        if (!LaneClearAt(rowCentre, laneIndex, COIN_CLEAR_MARGIN * gameScale))
        {
            continue;
        }

        for (int c = 0; c < MAX_COINS && placed < COIN_PER_SPAWN; c++)
        {
            if (!coinActive[c])
            {
                coinActive[c] = true;
                coinX[c] = x + (float)placed * COIN_SPACING * gameScale;
                coinY[c] = PlayerCenterY(laneIndex);
                placed++;
            }
        }

        coinSpawnTimer = CP_Random_RangeFloat(tuning->coinSpawnMin, tuning->coinSpawnMax);
        return;
    }
}

/*! Specification for function TrySpawnMultiplier:
@brief Tries to place a multiplier ball in a lane with no obstacle nearby.

@param none.
@return none.
*//*________________________________________________________________________________*/
static void TrySpawnMultiplier(void)
{
    const GameTuning* tuning = Tuning_Get();

    for (int tries = 0; tries < SPAWN_TRIES; tries++)
    {
        int laneIndex = RandomInt(0, NUM_LANES - 1);

        if (LaneClearAt(SpawnX(), laneIndex, COIN_CLEAR_MARGIN * gameScale))
        {
            multiplierActive = true;
            multiplierX = SpawnX();
            multiplierY = PlayerCenterY(laneIndex);
            multiplierSpawnTimer = CP_Random_RangeFloat(tuning->multiplierSpawnMin, tuning->multiplierSpawnMax);
            return;
        }
    }
}

/*! Specification for function TrySpawnHeart:
@brief Tries to place a heart pickup in a clear lane.

The lane must be clear of obstacles and of the mystery box.

@param none.
@return none.
*//*________________________________________________________________________________*/
static void TrySpawnHeart(void)
{
    const GameTuning* tuning = Tuning_Get();

    for (int tries = 0; tries < SPAWN_TRIES; tries++)
    {
        int laneIndex = RandomInt(0, NUM_LANES - 1);
        float x = SpawnX();

        if (!LaneClearAt(x, laneIndex, PICKUP_CLEAR_MARGIN * gameScale))
        {
            continue;
        }

        if (puActive && puLane == laneIndex && fabsf(puX - x) < 250.0f * gameScale)
        {
            continue;
        }

        heartActive = true;
        heartX = x;
        heartLane = laneIndex;
        heartSpawnTimer = CP_Random_RangeFloat(tuning->heartSpawnMin, tuning->heartSpawnMax);
        return;
    }
}

/* ============================================================
   Collisions and damage
   ============================================================ */

/*! Specification for function PlayDeathAudio:
@brief Plays the game over sound and stops the music.

@param none.
@return none.
*//*________________________________________________________________________________*/
static void PlayDeathAudio(void)
{
    PlaySfx(sndDeath, DEATH_VOL);
    CP_Sound_StopGroup(CP_SOUND_GROUP_MUSIC);
}

/*! Specification for function KillPlayer:
@brief Starts the animation of the ball catching the pin.

@param none.
@return none.
*//*________________________________________________________________________________*/
static void KillPlayer(void)
{
    state = STATE_GRAB;
    Anim_Reset(&grabAnim);
    PlayDeathAudio();
}

/*! Specification for function StartDying:
@brief Starts a death animation that plays where the pin is (hole or oil).

@param anim - The death animation to play.
@param obsIndex - The obstacle that caused the death.
@return none.
*//*________________________________________________________________________________*/
static void StartDying(Animation* anim, int obsIndex)
{
    state = STATE_DYING;
    activeDeathAnim = anim;
    deathObs = obsIndex;
    Anim_Reset(anim);
    PlayDeathAudio();
}

/*! Specification for function HandleHit:
@brief Handles the pin running into an obstacle.

Invincibility and the shield protect against barriers and oil but not holes.
An unprotected hit costs a heart; losing the last heart starts the death that
matches the obstacle.

@param i - The obstacle's index.
@return true - If the pin died, otherwise false.
*//*________________________________________________________________________________*/
static bool HandleHit(int i)
{
    int type = obsType[i];
    Animation* dyingAnim = NULL;

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
    if (shieldTimer > 0.0f && type != OBS_HOLE)
    {
        shieldTimer = 0.0f;

        if (type == OBS_BARRIER)
        {
            BreakObstacle(i);
        }

        return false;
    }

    /* Unprotected: lose a heart. */
    if (type == OBS_BARRIER)
    {
        BreakObstacle(i);
    }

    lives--;

    if (!heartActive)
    {
        heartSpawnTimer = CP_Random_RangeFloat(HEART_FIRST_DELAY_MIN, HEART_FIRST_DELAY_MAX);
    }

    if (lives > 0)
    {
        hurtTimer = HURT_DURATION;   /* flicker + stumble */
        PlaySfx(sndHit, HIT_VOL);
        return false;
    }

    /* Last heart lost. */
    lives = 0;
    hurtTimer = 0.0f;

    switch (type)
    {
    case OBS_HOLE: deathCauseText = "YOU FELL INTO A HOLE!"; dyingAnim = &holeDeathAnim; break;
    case OBS_OIL:  deathCauseText = "YOU SLIPPED ON OIL!";   dyingAnim = &oilDeathAnim;  break;
    default:       deathCauseText = "THE BALL CAUGHT YOU!";  break;
    }

    if (dyingAnim != NULL && dyingAnim->sheet != NULL)
    {
        StartDying(dyingAnim, i);
    }
    else
    {
        deathCauseText = "THE BALL CAUGHT YOU!";
        KillPlayer();
    }

    return true;
}

/* ============================================================
   Drawing
   ============================================================ */

/*! Specification for function DrawObstacle:
@brief Draws an obstacle, or its spawn / break animation if one is playing.

@param i - The obstacle's index.
@return none.
*//*________________________________________________________________________________*/
static void DrawObstacle(int i)
{
    const Animation* fx = NULL;
    float centerX = ObstacleCenterX(i);
    float centerY = ObstacleCenterY(i);

    if (obsPhase[i] == OBS_PH_WAITING)
    {
        return;   /* oil that has not appeared yet */
    }

    if (obsPhase[i] == OBS_PH_SPAWN)
    {
        fx = &oilSpawnAnim;
    }
    else if (obsPhase[i] == OBS_PH_BROKEN)
    {
        fx = &barrierBreakAnim;
    }

    if (fx != NULL && fx->sheet != NULL)
    {
        Anim_DrawFrame(fx, obsFrame[i], centerX, centerY, 255);
    }
    else
    {
        Anim_Draw(&obsAnim[obsType[i]], centerX, centerY);
    }
}

/*! Specification for function DrawPowerFxFallback:
@brief Draws a coloured circle for a power-up effect whose art failed to load.

@param type - The power-up.
@param x - The horizontal centre.
@param y - The vertical centre.
@return none.
*//*________________________________________________________________________________*/
static void DrawPowerFxFallback(int type, float x, float y)
{
    static const float diameters[NUM_POWERUPS] = { 190.0f, 160.0f, 220.0f };

    CP_Settings_Fill(Hud_PowerUpColor((PowerType)type, 90));
    CP_Graphics_DrawCircle(x, y, diameters[type] * gameScale);
}

/*! Specification for function DrawPowerIcon:
@brief Draws a power-up icon, or a coloured circle with a letter if the art is missing.

@param type - The power-up.
@param x - The horizontal centre.
@param y - The vertical centre.
@return none.
*//*________________________________________________________________________________*/
static void DrawPowerIcon(int type, float x, float y)
{
    if (puIconAnim[type].sheet != NULL)
    {
        Anim_Draw(&puIconAnim[type], x, y);
        return;
    }

    CP_Settings_Fill(Hud_PowerUpColor((PowerType)type, 255));
    CP_Graphics_DrawCircle(x, y, PU_SIZE * gameScale);

    if (UI_GetTitleFont() != NULL)
    {
        CP_Font_Set(UI_GetTitleFont());
    }

    CP_Settings_Fill(CP_Color_Create(255, 255, 255, 255));
    CP_Settings_TextSize(30.0f * gameScale);
    CP_Settings_TextAlignment(CP_TEXT_ALIGN_H_CENTER, CP_TEXT_ALIGN_V_MIDDLE);
    CP_Font_DrawText(puLabels[type], x, y);
}

/*! Specification for function DrawPickups:
@brief Draws the coins, mystery box, heart pickup and multiplier ball.

@param none.
@return none.
*//*________________________________________________________________________________*/
static void DrawPickups(void)
{
    CP_Settings_NoStroke();

    /* Coins (one shared animation). */
    for (int c = 0; c < MAX_COINS; c++)
    {
        if (!coinActive[c])
        {
            continue;
        }

        if (coinAnim.sheet != NULL)
        {
            Anim_Draw(&coinAnim, coinX[c], coinY[c]);
        }
        else
        {
            CP_Settings_Fill(CP_Color_Create(255, 215, 0, 255));
            CP_Graphics_DrawCircle(coinX[c], coinY[c], COIN_SIZE * gameScale);
        }
    }

    /* Mystery box: every power-up looks the same until it is collected. */
    if (puActive)
    {
        float y = PlayerCenterY(puLane);
        float boxSize = PU_SIZE * gameScale;

        if (boxImg != NULL)
        {
            CP_Image_Draw(boxImg, puX, y, boxSize, boxSize, 255);
        }
        else
        {
            CP_Settings_Fill(CP_Color_Create(255, 200, 0, 255));
            CP_Graphics_DrawCircle(puX, y, boxSize);
        }
    }

    /* Heart pickup, bobbing up and down. */
    if (heartActive)
    {
        float bob = sinf(CP_System_GetSeconds() * 5.0f) * 6.0f * gameScale;
        float size = HEART_PICKUP_SIZE * gameScale;
        float y = PlayerCenterY(heartLane) + bob;

        if (heartPickupImg != NULL)
        {
            CP_Image_Draw(heartPickupImg, heartX, y, size, size, 255);
        }
        else
        {
            CP_Settings_Fill(CP_Color_Create(230, 40, 60, 255));
            CP_Graphics_DrawCircle(heartX, y, size * 0.6f);
        }
    }

    /* Multiplier ball. */
    if (multiplierActive)
    {
        float size = MULT_SIZE * gameScale;

        if (multiplierItemImg != NULL)
        {
            CP_Image_Draw(multiplierItemImg, multiplierX, multiplierY, size, size, 255);
        }
        else
        {
            CP_Settings_Fill(CP_Color_Create(255, 0, 0, 90));
            CP_Graphics_DrawCircle(multiplierX, multiplierY, size);
        }
    }
}

/*! Specification for function DrawRoulette:
@brief Draws the item slot at the top of the screen.

The icons spin, then land on the real power-up and show its name.

@param none.
@return none.
*//*________________________________________________________________________________*/
static void DrawRoulette(void)
{
    float x;
    float y;
    float boxSize;

    if (state != STATE_PLAYING || (rouletteTimer <= 0.0f && revealTimer <= 0.0f))
    {
        return;
    }

    x = (float)width / 2.0f;
    y = 145.0f * UI_GetScale();
    boxSize = (PU_SIZE + 40.0f) * gameScale;

    CP_Settings_RectMode(CP_POSITION_CENTER);
    CP_Settings_Fill(CP_Color_Create(20, 20, 25, 220));
    CP_Graphics_DrawRect(x, y, boxSize, boxSize);

    DrawPowerIcon(rouletteShown, x, y);

    if (revealTimer > 0.0f)
    {
        if (UI_GetTitleFont() != NULL)
        {
            CP_Font_Set(UI_GetTitleFont());
        }

        CP_Settings_Fill(Hud_PowerUpColor((PowerType)rouletteShown, 255));
        CP_Settings_TextSize(24.0f * gameScale);
        CP_Settings_TextAlignment(CP_TEXT_ALIGN_H_CENTER, CP_TEXT_ALIGN_V_MIDDLE);
        CP_Font_DrawText(puNames[rouletteShown], x, y + boxSize / 2.0f + 25.0f * gameScale);
    }
}

/*! Specification for function DrawPlayer:
@brief Draws the pin with whichever power-up effect is active.

Also draws the hit flicker and knock-back, and the angel pin after collecting a heart.

@param none.
@return none.
*//*________________________________________________________________________________*/
static void DrawPlayer(void)
{
    float pinFeetY = PinFeetY(lane);
    float pinCentreY = pinFeetY - runAnim.drawH / 2.0f;
    int alpha = 255;
    float drawX = playerX;
    int frame = runAnim.currentFrame;

    /* Fallback circles behind the pin when an effect sheet is missing. */
    if (invincibleTimer > 0.0f && puFx[PU_INVINCIBLE].sheet == NULL)
    {
        DrawPowerFxFallback(PU_INVINCIBLE, playerX, pinCentreY);
    }

    if (shieldTimer > 0.0f && puFx[PU_SHIELD].sheet == NULL)
    {
        DrawPowerFxFallback(PU_SHIELD, playerX, pinCentreY);
    }

    if (magnetTimer > 0.0f && puFx[PU_MAGNET].sheet == NULL)
    {
        DrawPowerFxFallback(PU_MAGNET, playerX, pinCentreY);
    }

    /* Hit reaction: flicker and knock-back that slides forward again. */
    if (hurtTimer > 0.0f)
    {
        float fraction = hurtTimer / HURT_DURATION;   /* 1 right after the hit, 0 when recovered */

        alpha = ((int)(hurtTimer * HURT_FLICKER_RATE) % 2) ? HURT_MIN_ALPHA : 255;
        drawX = playerX - STUMBLE_LURCH * gameScale * fraction;
    }

    if (reviveAnimTimer > 0.0f && reviveAnim.sheet != NULL && reviveAnim.frameCount > 0)
    {
        Anim_DrawFrame(&reviveAnim, frame % reviveAnim.frameCount, drawX,
            pinFeetY - reviveAnim.drawH / 2.0f, alpha);
    }
    else if (invincibleTimer > 0.0f && puFx[PU_INVINCIBLE].sheet != NULL)
    {
        const Animation* fx = &puFx[PU_INVINCIBLE];
        Anim_DrawFrame(fx, frame % fx->frameCount, drawX,
            pinFeetY - (INVINCIBLE_FEET_FRAC - 0.5f) * fx->drawH, alpha);
    }
    else if (shieldTimer > 0.0f && puFx[PU_SHIELD].sheet != NULL)
    {
        const Animation* fx = &puFx[PU_SHIELD];
        Anim_DrawFrame(fx, frame % fx->frameCount, drawX, pinFeetY - fx->drawH / 2.0f, alpha);
    }
    else if (magnetTimer > 0.0f && puFx[PU_MAGNET].sheet != NULL)
    {
        const Animation* fx = &puFx[PU_MAGNET];
        Anim_DrawFrame(fx, frame % fx->frameCount, drawX, pinFeetY - fx->drawH / 2.0f, alpha);
    }
    else
    {
        Anim_DrawAlpha(&runAnim, drawX, pinCentreY, alpha);
    }
}

/*! Specification for function FeetPadding:
@brief Gets the empty space under the pin's feet in its sprite sheet.

@param none.
@return The padding in pixels, scaled to the pin's draw size.
*//*________________________________________________________________________________*/
static float FeetPadding(void)
{
    return FEET_PADDING * (runAnim.drawH / PLAYER_H);
}

/*! Specification for function DrawWorld:
@brief Draws the scrolling background, obstacles, pickups, pin and ball.

@param none.
@return none.
*//*________________________________________________________________________________*/
static void DrawWorld(void)
{
    CP_Graphics_ClearBackground(CP_Color_Create(255, 255, 255, 255));
    CP_Settings_ImageMode(CP_POSITION_CENTER);

    /* Two copies of the background side by side make an endless scroll. */
    if (bgImage != NULL)
    {
        float w = (float)width;
        float h = (float)height;

        CP_Image_Draw(bgImage, w / 2.0f - bgScroll, h / 2.0f, w + 2.0f, h, 255);
        CP_Image_Draw(bgImage, w / 2.0f - bgScroll + w, h / 2.0f, w + 2.0f, h, 255);
    }

    for (int i = 0; i < MAX_OBS; i++)
    {
        if (state == STATE_DYING && i == deathObs)
        {
            continue;   /* the death animation draws its own hole / oil */
        }

        DrawObstacle(i);
    }

    DrawPickups();

    if (state == STATE_GRAB)
    {
        /* Line the pin in the grab sheet up with where the running pin was. */
        float scaleX = grabAnim.drawW / grabAnim.frameW;
        float scaleY = grabAnim.drawH / grabAnim.frameH;
        float runningFeetY = PinFeetY(lane) - FeetPadding();
        float cx = playerX + (grabAnim.frameW * 0.5f - GRAB_PIN_SRC_X) * scaleX;
        float cy = runningFeetY + (grabAnim.frameH * 0.5f - GRAB_PIN_SRC_FEET) * scaleY;

        Anim_Draw(&grabAnim, cx, cy);
    }
    else if (state == STATE_DYING && activeDeathAnim != NULL)
    {
        /* Hole / oil death, drawn where the obstacle was. */
        if (deathObs >= 0)
        {
            Anim_Draw(activeDeathAnim, ObstacleCenterX(deathObs),
                ObstacleCenterY(deathObs) - DEATH_ANIM_RAISE * gameScale);
        }

        Anim_Draw(&chaseAnim, enemyX, enemyGround - FeetPadding() - chaseAnim.drawH * 0.22f);
    }
    else
    {
        DrawPlayer();
        Anim_Draw(&chaseAnim, enemyX, enemyGround - FeetPadding() - chaseAnim.drawH * 0.5f);
    }
}

/* ============================================================
   Debug tools hookup
   ============================================================ */

/*! Specification for function DrawDebugHitboxes:
@brief Outlines everything that can collide (debug overlay only).

Uses the same numbers as the collision code: red = obstacle, yellow = coin,
green = pickups, cyan = the pin's hit area, purple = magnet range.

@param none.
@return none.
*//*________________________________________________________________________________*/
static void DrawDebugHitboxes(void)
{
    CP_Color obstacleColor = CP_Color_Create(255, 60, 60, 255);
    CP_Color usedColor = CP_Color_Create(120, 120, 120, 255);
    CP_Color coinColor = CP_Color_Create(255, 230, 0, 255);
    CP_Color pickupColor = CP_Color_Create(60, 255, 120, 255);
    float playerCentreY = PlayerCenterY(lane);

    Debug_DrawBox(playerX, playerCentreY, runAnim.drawW * PLAYER_HIT_HALF_WIDTH * 2.0f, runAnim.drawH,
        CP_Color_Create(0, 255, 255, 255));

    if (magnetTimer > 0.0f)
    {
        Debug_DrawCircle(playerX, playerCentreY, Tuning_Get()->magnetRange * gameScale,
            CP_Color_Create(200, 120, 255, 255));
    }

    for (int i = 0; i < MAX_OBS; i++)
    {
        Animation* anim = &obsAnim[obsType[i]];
        Debug_DrawBox(ObstacleCenterX(i), ObstacleCenterY(i), anim->drawW, anim->drawH,
            obsHit[i] ? usedColor : obstacleColor);
    }

    for (int c = 0; c < MAX_COINS; c++)
    {
        if (coinActive[c])
        {
            Debug_DrawCircle(coinX[c], coinY[c], COIN_COLLECT_DIST * gameScale, coinColor);
        }
    }

    if (puActive)
    {
        Debug_DrawBox(puX, PlayerCenterY(puLane), PU_PICKUP_DIST * 2.0f * gameScale, PU_SIZE * gameScale, pickupColor);
    }

    if (heartActive)
    {
        Debug_DrawBox(heartX, PlayerCenterY(heartLane), HEART_PICKUP_DIST * 2.0f * gameScale,
            HEART_PICKUP_SIZE * gameScale, pickupColor);
    }

    if (multiplierActive)
    {
        Debug_DrawCircle(multiplierX, multiplierY, MULT_COLLECT_DIST * gameScale, pickupColor);
    }
}

/*! Specification for function DrawDebugOverlay:
@brief Draws the debug panel with the live game values.

@param none.
@return none.
*//*________________________________________________________________________________*/
static void DrawDebugOverlay(void)
{
    static const char* phaseNames[] = { "PLAYING", "GRAB", "DYING" };
    static const char* obstacleNames[NUM_OBS_TYPES] = { "barrier", "oil", "hole" };
    char lines[12][96];
    const char* linePointers[12];
    int count = 0;
    int activeCoins = 0;

    for (int c = 0; c < MAX_COINS; c++)
    {
        activeCoins += coinActive[c] ? 1 : 0;
    }

    snprintf(lines[count++], 96, "State %s%s  Lane %d  Lives %d", phaseNames[state],
        isPaused ? " (paused)" : "", lane, lives);
    snprintf(lines[count++], 96, "Speed %.0f  Distance %.0fm", (double)speed, (double)distanceMetres);
    snprintf(lines[count++], 96, "Score %.0f x%d = %d  High %d", (double)score, scoreMultiplier, FinalScore(), highScore);
    snprintf(lines[count++], 96, "Inv %.1f  Shield %.1f  Magnet %.1f",
        (double)invincibleTimer, (double)shieldTimer, (double)magnetTimer);
    snprintf(lines[count++], 96, "Hurt timer %.2f", (double)hurtTimer);
    snprintf(lines[count++], 96, "Coins on screen %d  Box %s  Heart %s  Mult %s", activeCoins,
        puActive ? "yes" : "no", heartActive ? "yes" : "no", multiplierActive ? "yes" : "no");
    snprintf(lines[count++], 96, "Next box %.1fs  heart %.1fs  mult %.1fs",
        (double)puSpawnTimer, (double)heartSpawnTimer, (double)multiplierSpawnTimer);

    for (int i = 0; i < MAX_OBS; i++)
    {
        snprintf(lines[count++], 96, "Obs %d: %-7s lane %d  x %5.0f%s", i, obstacleNames[obsType[i]],
            obsLane[i], (double)obsX[i], obsHit[i] ? "  (hit)" : "");
    }

    for (int i = 0; i < count; i++)
    {
        linePointers[i] = lines[i];
    }

    Debug_DrawOverlay(linePointers, count);
}

/*! Specification for function ApplyDebugCommand:
@brief Carries out a debug key request (F5 or F6).

@param command - The request: give a power-up or add a heart.
@return none.
*//*________________________________________________________________________________*/
static void ApplyDebugCommand(DebugCommand command)
{
    if (command == DEBUG_COMMAND_GIVE_POWERUP)
    {
        ApplyPowerUp(debugNextPowerUp);
        rouletteShown = debugNextPowerUp;
        revealTimer = REVEAL_TIME;
        debugNextPowerUp = (debugNextPowerUp + 1) % NUM_POWERUPS;
    }
    else if (command == DEBUG_COMMAND_ADD_LIFE && lives < MAX_LIVES)
    {
        lives++;
    }
}

/* ============================================================
   Update
   ============================================================ */

/*! Specification for function UpdateLaneInput:
@brief Moves the pin up or down one lane with W / S.

@param none.
@return none.
*//*________________________________________________________________________________*/
static void UpdateLaneInput(void)
{
    if (CP_Input_KeyTriggered(KEY_W) && lane > 0)
    {
        lane--;
        PlaySfx(sndJump, JUMP_VOL);
    }

    if (CP_Input_KeyTriggered(KEY_S) && lane < NUM_LANES - 1)
    {
        lane++;
        PlaySfx(sndJump, JUMP_VOL);
    }
}

/*! Specification for function StumbleSpeedMultiplier:
@brief Gets how much a recent hit slows the world down.

@param none.
@return 1.0 normally; lower right after a hit, then recovering smoothly.
*//*________________________________________________________________________________*/
static float StumbleSpeedMultiplier(void)
{
    float fraction;
    float holdEnd;

    if (hurtTimer <= 0.0f)
    {
        return 1.0f;
    }

    fraction = hurtTimer / HURT_DURATION;
    holdEnd = 1.0f - STUMBLE_HOLD;

    if (fraction > holdEnd)
    {
        return SLOW_FACTOR;
    }

    return SLOW_FACTOR + (1.0f - SLOW_FACTOR) * (1.0f - fraction / holdEnd);
}

/*! Specification for function UpdateObstacles:
@brief Moves the obstacles, checks collisions and respawns obstacles that left the screen.

The check is swept, so a fast obstacle cannot skip over the pin in one frame.

@param dt - The time passed since the previous frame, in seconds.
@param worldMove - How far the world scrolls this frame, in pixels.
@return true - If the pin died, otherwise false.
*//*________________________________________________________________________________*/
static bool UpdateObstacles(float dt, float worldMove)
{
    float playerHalfWidth = runAnim.drawW * PLAYER_HIT_HALF_WIDTH;
    float playerLeft = playerX - playerHalfWidth;
    float playerRight = playerX + playerHalfWidth;

    for (int i = 0; i < MAX_OBS; i++)
    {
        float previousX = obsX[i];
        float obsW = obsAnim[obsType[i]].drawW;
        bool crossedPlayer;

        obsX[i] -= worldMove;
        UpdateObstacleFx(i, dt);

        /* Did the obstacle pass over the pin at any point this frame? */
        crossedPlayer = obsX[i] <= playerRight && previousX + obsW >= playerLeft;

        if (obsLane[i] == lane && crossedPlayer && !obsHit[i])
        {
            obsHit[i] = true;

            if (!Debug_IsGodMode() && HandleHit(i))
            {
                return true;
            }
        }

        if (obsX[i] + obsW < 0.0f)
        {
            RespawnObstacle(i);
        }
    }

    return false;
}

/*! Specification for function UpdatePowerUps:
@brief Counts down the power-up timers and runs the item roulette.

@param dt - The time passed since the previous frame, in seconds.
@return none.
*//*________________________________________________________________________________*/
static void UpdatePowerUps(float dt)
{
    invincibleTimer = fmaxf(0.0f, invincibleTimer - dt);
    shieldTimer = fmaxf(0.0f, shieldTimer - dt);
    magnetTimer = fmaxf(0.0f, magnetTimer - dt);

    if (reviveAnimTimer > 0.0f)
    {
        reviveAnimTimer -= dt;
    }

    /* Icons spin, slow down, then land on the real power-up. */
    if (rouletteTimer > 0.0f)
    {
        rouletteTimer -= dt;
        rouletteSwitch -= dt;

        if (rouletteTimer <= 0.0f)
        {
            rouletteShown = rouletteType;
            ApplyPowerUp(rouletteType);
            revealTimer = REVEAL_TIME;
            PlaySfx(sndRouletteReveal, ROULETTE_VOL);
        }
        else if (rouletteSwitch <= 0.0f)
        {
            float progress = 1.0f - rouletteTimer / ROULETTE_TIME;

            rouletteShown = (rouletteShown + 1) % NUM_POWERUPS;
            rouletteSwitch = 0.06f + 0.14f * progress;   /* changes get slower towards the end */
        }
    }
    else if (revealTimer > 0.0f)
    {
        revealTimer -= dt;
    }

    for (int t = 0; t < NUM_POWERUPS; t++)
    {
        Anim_Update(&puIconAnim[t], dt);
    }

    if (invincibleTimer > 0.0f) Anim_Update(&puFx[PU_INVINCIBLE], dt);
    if (shieldTimer > 0.0f)     Anim_Update(&puFx[PU_SHIELD], dt);
    if (magnetTimer > 0.0f)     Anim_Update(&puFx[PU_MAGNET], dt);
}

/*! Specification for function UpdatePickups:
@brief Scrolls, collects and spawns the mystery box, coins, hearts and multiplier ball.

@param dt - The time passed since the previous frame, in seconds.
@param worldMove - How far the world scrolls this frame, in pixels.
@return none.
*//*________________________________________________________________________________*/
static void UpdatePickups(float dt, float worldMove)
{
    const GameTuning* tuning = Tuning_Get();
    float playerCentreY = PlayerCenterY(lane);

    /* Mystery box. */
    if (puActive)
    {
        puX -= worldMove;

        if (puLane == lane && fabsf(puX - playerX) < PU_PICKUP_DIST * gameScale && rouletteTimer <= 0.0f)
        {
            StartRoulette(RandomInt(0, NUM_POWERUPS - 1));
            puActive = false;
        }
        else if (puX < -SPAWN_OFFSCREEN_X * gameScale)
        {
            puActive = false;
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

    /* Coins (pulled in by the magnet). */
    Anim_Update(&coinAnim, dt);
    coinSpawnTimer -= dt;

    if (coinSpawnTimer <= 0.0f)
    {
        TrySpawnCoins();
    }

    for (int c = 0; c < MAX_COINS; c++)
    {
        float dx;
        float dy;
        float distance;

        if (!coinActive[c])
        {
            continue;
        }

        coinX[c] -= worldMove;

        dx = playerX - coinX[c];
        dy = playerCentreY - coinY[c];
        distance = sqrtf(dx * dx + dy * dy);

        if (magnetTimer > 0.0f && distance < tuning->magnetRange * gameScale && distance > 1.0f)
        {
            float pull = MAGNET_PULL_SPEED * gameScale * dt;

            coinX[c] += dx / distance * pull;
            coinY[c] += dy / distance * pull;
        }

        if (distance < COIN_COLLECT_DIST * gameScale)
        {
            PlaySfx(sndCoin, COIN_VOL);
            coinActive[c] = false;
            coinCount++;
        }
        else if (coinX[c] < -50.0f * gameScale)
        {
            coinActive[c] = false;
        }
    }

    /* Heart: only spawns while the player is missing a heart. */
    if (heartActive)
    {
        heartX -= worldMove;

        if (heartLane == lane && fabsf(heartX - playerX) < HEART_PICKUP_DIST * gameScale)
        {
            if (lives < MAX_LIVES)
            {
                lives++;
            }

            heartActive = false;
            reviveAnimTimer = REVIVE_ANIM_TIME;   /* angel pin */
            PlaySfx(sndHeart != NULL ? sndHeart : sndCoin, HEART_VOL);
        }
        else if (heartX < -SPAWN_OFFSCREEN_X * gameScale)
        {
            heartActive = false;
        }
    }
    else if (lives < MAX_LIVES)
    {
        heartSpawnTimer -= dt;

        if (heartSpawnTimer <= 0.0f)
        {
            TrySpawnHeart();
        }
    }

    /* Multiplier ball. */
    if (!multiplierActive)
    {
        multiplierSpawnTimer -= dt;

        if (multiplierSpawnTimer <= 0.0f)
        {
            TrySpawnMultiplier();
        }
    }
    else
    {
        float dx;
        float dy;

        multiplierX -= worldMove;
        dx = playerX - multiplierX;
        dy = playerCentreY - multiplierY;

        if (sqrtf(dx * dx + dy * dy) < MULT_COLLECT_DIST * gameScale)
        {
            multiplierActive = false;

            if (scoreMultiplier < MAX_MULT)
            {
                scoreMultiplier++;
            }

            PlaySfx(sndMultiplier, MULT_VOL);
        }
        else if (multiplierX < -50.0f * gameScale)
        {
            multiplierActive = false;
        }
    }
}

/*! Specification for function UpdateChase:
@brief Keeps the bowling ball on the pin's lane.

The ball stays at the left edge (about half of it visible) and only moves
up and down to follow the pin's lane.

@param none.
@return none.
*//*________________________________________________________________________________*/
static void UpdateChase(void)
{
    enemyX = 0.0f;
    enemyGround = PinFeetY(lane);
}

/*! Specification for function UpdatePlaying:
@brief Runs one frame of normal gameplay.

Updates input, score, distance, speed, obstacles, power-ups, pickups,
the chase and the rage overlay.

@param dt - The time passed since the previous frame, in seconds.
@return none.
*//*________________________________________________________________________________*/
static void UpdatePlaying(float dt)
{
    const GameTuning* tuning = Tuning_Get();
    float worldMove;
    int currentScore;

    UpdateLaneInput();

    score += speed * dt / SCORE_DIVISOR;
    distanceMetres += speed * dt / DISTANCE_DIVISOR;
    speed += tuning->speedRamp * dt;

    currentScore = FinalScore();

    if (currentScore > highScore)
    {
        highScore = currentScore;
    }

    if (hurtTimer > 0.0f)
    {
        hurtTimer -= dt;
    }

    /* Everything in the world moves left by the same amount. */
    worldMove = speed * gameScale * StumbleSpeedMultiplier() * dt;

    bgScroll = fmodf(bgScroll + worldMove, (float)width);

    for (int t = 0; t < NUM_OBS_TYPES; t++)
    {
        Anim_Update(&obsAnim[t], dt);
    }

    if (UpdateObstacles(dt, worldMove))
    {
        return;   /* the pin died this frame */
    }

    UpdatePowerUps(dt);
    UpdatePickups(dt, worldMove);
    UpdateChase();

    Anim_Update(&runAnim, dt);
    Anim_Update(&chaseAnim, dt);
    RageOverlay_Update(dt);
}

/*! Specification for function UpdateDeath:
@brief Plays the death animation, then shows the results screen after a short hold.

@param dt - The time passed since the previous frame, in seconds.
@return none.
*//*________________________________________________________________________________*/
static void UpdateDeath(float dt)
{
    Animation* anim = (state == STATE_GRAB) ? &grabAnim : activeDeathAnim;

    if (anim == NULL)
    {
        return;
    }

    Anim_Update(anim, dt);

    if (Anim_IsFinished(anim))
    {
        deathHoldTimer += dt;

        if (deathHoldTimer >= DEATH_SCREEN_DELAY)
        {
            showDeathScreen = true;
        }
    }
}

/*! Specification for function StartRun:
@brief Resets everything for a new run.

@param none.
@return none.
*//*________________________________________________________________________________*/
static void StartRun(void)
{
    const GameTuning* tuning = Tuning_Get();

    state = STATE_PLAYING;
    lane = 1;
    lives = MAX_LIVES;
    speed = tuning->startSpeed;

    score = 0.0f;
    distanceMetres = 0.0f;
    coinCount = 0;
    scoreMultiplier = 1;
    highScoreAtStart = highScore;
    deathCauseText = NULL;

    isPaused = false;
    showDeathScreen = false;
    leavingGame = false;
    deathHoldTimer = 0.0f;
    hurtTimer = 0.0f;
    reviveAnimTimer = 0.0f;
    activeDeathAnim = NULL;
    deathObs = -1;

    invincibleTimer = 0.0f;
    shieldTimer = 0.0f;
    magnetTimer = 0.0f;

    puActive = false;
    puSpawnTimer = CP_Random_RangeFloat(tuning->powerUpSpawnMin, tuning->powerUpSpawnMax);
    rouletteTimer = 0.0f;
    rouletteSwitch = 0.0f;
    revealTimer = 0.0f;

    heartActive = false;
    heartSpawnTimer = CP_Random_RangeFloat(tuning->heartSpawnMin, tuning->heartSpawnMax);

    multiplierActive = false;
    multiplierSpawnTimer = CP_Random_RangeFloat(tuning->multiplierSpawnMin, tuning->multiplierSpawnMax);

    for (int c = 0; c < MAX_COINS; c++)
    {
        coinActive[c] = false;
    }

    coinSpawnTimer = FIRST_COIN_DELAY;

    /* Obstacles start just off screen, evenly spaced. */
    for (int i = 0; i < MAX_OBS; i++)
    {
        obsX[i] = (float)width + (float)i * tuning->obstacleSpacing * gameScale;
        RandomiseObstacle(i);
        obsHit[i] = false;
    }

    bgScroll = 0.0f;

    /* Ball centred on the left edge, so about half of it is visible. */
    enemyX = 0.0f;
    enemyGround = PinFeetY(lane);

    Anim_Reset(&runAnim);
    Anim_Reset(&chaseAnim);
    Anim_Reset(&grabAnim);

    RageOverlay_Reset();
    GameMenus_Reset();
    Debug_Reset();
}

/* ============================================================
   Loading
   ============================================================ */

/*! Specification for function LoadCharacters:
@brief Loads the pin, ball, grab and angel pin animations.

@param none.
@return none.
*//*________________________________________________________________________________*/
static void LoadCharacters(void)
{
    Anim_Init(&runAnim, "Assets/bowling_pin_running.png", 4, PLAYER_W, PLAYER_H, 0.15f, true);
    Anim_Init(&chaseAnim, "Assets/bowling_ball_chasing.png", 8, ENEMY_W, ENEMY_H, 0.15f, true);
    Anim_Init(&grabAnim, "Assets/pin_death_by_ball.png", 7, GRAB_FRAME_W, GRAB_FRAME_H, DEATH_FRAME_TIME, false);

    /* Angel pin shown after collecting a heart. */
    Anim_Init(&reviveAnim, "Assets/ExtraLife-animation.png", 4, PLAYER_W, PLAYER_H, 0.15f, true);
    Anim_FitHorizontalSheet(&reviveAnim, runAnim.drawH);
}

/*! Specification for function LoadObstacles:
@brief Loads the obstacle images and their spawn, break and death animations.

Obstacles are sized to part of a lane's height and keep their aspect ratio.

@param none.
@return none.
*//*________________________________________________________________________________*/
static void LoadObstacles(void)
{
    /* Obstacles fill part of a lane's height, keeping their aspect ratio. */
    float obstacleHeight = laneGap * OBS_LANE_FILL;
    float obstacleWidth = obstacleHeight * (OBS_SRC_W / OBS_SRC_H);

    for (int t = 0; t < NUM_OBS_TYPES; t++)
    {
        Anim_Init(&obsAnim[t], obsPaths[t], 1, OBS_SRC_W, OBS_SRC_H, 0.15f, true);
        Anim_SetDrawSize(&obsAnim[t], obstacleWidth, obstacleHeight);
    }

    Anim_Init(&oilSpawnAnim, oilSpawnInfo.path, oilSpawnInfo.frames,
        oilSpawnInfo.w, oilSpawnInfo.h, oilSpawnInfo.frameTime, false);
    Anim_SetDrawSize(&oilSpawnAnim, obsAnim[OBS_OIL].drawW, obsAnim[OBS_OIL].drawH);

    Anim_Init(&barrierBreakAnim, barrierBreakInfo.path, barrierBreakInfo.frames,
        barrierBreakInfo.w, barrierBreakInfo.h, barrierBreakInfo.frameTime, false);
    Anim_SetDrawSize(&barrierBreakAnim, obsAnim[OBS_BARRIER].drawW, obsAnim[OBS_BARRIER].drawH);

    Anim_Init(&holeDeathAnim, holeDeathInfo.path, holeDeathInfo.frames,
        holeDeathInfo.w, holeDeathInfo.h, holeDeathInfo.frameTime, false);
    Anim_SetDrawSize(&holeDeathAnim, obsAnim[OBS_HOLE].drawW * HOLE_DEATH_SCALE, obsAnim[OBS_HOLE].drawH * HOLE_DEATH_SCALE);

    Anim_Init(&oilDeathAnim, oilDeathInfo.path, oilDeathInfo.frames,
        oilDeathInfo.w, oilDeathInfo.h, oilDeathInfo.frameTime, false);
    Anim_SetDrawSize(&oilDeathAnim, obsAnim[OBS_OIL].drawW, obsAnim[OBS_OIL].drawH);
}

/*! Specification for function LoadPickups:
@brief Loads the coin, power-up, mystery box, heart and multiplier art.

@param none.
@return none.
*//*________________________________________________________________________________*/
static void LoadPickups(void)
{
    Anim_Init(&coinAnim, "Assets/Coin.png", COIN_FRAMES, COIN_SRC_W, COIN_SRC_H, COIN_FRAME_TIME, true);
    Anim_SetDrawSize(&coinAnim, COIN_SIZE * gameScale * (COIN_SRC_W / COIN_SRC_H), COIN_SIZE * gameScale);

    for (int t = 0; t < NUM_POWERUPS; t++)
    {
        float fxH = PU_FX_SIZE * gameScale;
        float fxW = fxH * (puFxInfo[t].w / puFxInfo[t].h);

        Anim_Init(&puIconAnim[t], puIconInfo[t].path, puIconInfo[t].frames,
            puIconInfo[t].w, puIconInfo[t].h, puIconInfo[t].frameTime, true);
        Anim_SetDrawSize(&puIconAnim[t], PU_SIZE * gameScale * (puIconInfo[t].w / puIconInfo[t].h), PU_SIZE * gameScale);

        Anim_Init(&puFx[t], puFxInfo[t].path, puFxInfo[t].frames,
            puFxInfo[t].w, puFxInfo[t].h, puFxInfo[t].frameTime, true);

        /* Shield and magnet are drawn at the pin's size; invincible is a bit larger. */
        if (t == PU_SHIELD || t == PU_MAGNET)
        {
            fxW = runAnim.drawW;
            fxH = runAnim.drawH;
        }
        else
        {
            fxH = runAnim.drawH * INVINCIBLE_ANIM_SCALE;
            fxW = fxH * (puFxInfo[t].w / puFxInfo[t].h);
        }

        Anim_SetDrawSize(&puFx[t], fxW, fxH);
    }

    Anim_FitHorizontalSheet(&puFx[PU_MAGNET], runAnim.drawH);

    boxImg = CP_Image_Load("Assets/mysterybox-item.png");
    heartPickupImg = CP_Image_Load("Assets/ExtraLife-item.png");
    multiplierItemImg = CP_Image_Load("Assets/multiplier-ball-item.png");
}

/*! Specification for function LoadAudio:
@brief Loads the gameplay music and sound effects.

@param none.
@return none.
*//*________________________________________________________________________________*/
static void LoadAudio(void)
{
    sndJump = CP_Sound_Load("Assets/jump.mp3");
    sndCoin = CP_Sound_Load("Assets/coin_pickup.mp3");
    sndDeath = CP_Sound_Load("Assets/game_over.mp3");
    gameMusic = CP_Sound_Load("Assets/bg_music.mp3");
    sndHit = CP_Sound_Load("Assets/taking_dmg.mp3");
    sndHeart = CP_Sound_Load("Assets/heart_sound.mp3");
    sndRouletteSpin = CP_Sound_Load("Assets/roulette_spin_natural_speed_1_5s.mp3");
    sndRouletteReveal = CP_Sound_Load("Assets/roulette_reveal_2.mp3");
    sndMultiplier = CP_Sound_Load("Assets/multiplier_sound.mp3");
}

/*! Specification for function FreeSound:
@brief Frees a sound and clears the pointer.

@param sound - The sound to free (set to NULL afterwards).
@return none.
*//*________________________________________________________________________________*/
static void FreeSound(CP_Sound* sound)
{
    if (*sound != NULL)
    {
        CP_Sound_Free(*sound);
        *sound = NULL;
    }
}

/*! Specification for function FreeImage:
@brief Frees an image and clears the pointer.

@param image - The image to free (set to NULL afterwards).
@return none.
*//*________________________________________________________________________________*/
static void FreeImage(CP_Image* image)
{
    if (*image != NULL)
    {
        CP_Image_Free(*image);
        *image = NULL;
    }
}

/* ============================================================
   State functions
   ============================================================ */

/*! Specification for function Game_GetHighScore:
@brief Gets the best final score this session.

@param none.
@return The high score.
*//*________________________________________________________________________________*/
int Game_GetHighScore(void)
{
    return highScore;
}

/*! Specification for function Game_Init:
@brief Prepares the gameplay state when it is entered.

Loads the tuning file, fonts, HUD, art and audio, starts the music
and starts a new run.

@param none.
@return none.
*//*________________________________________________________________________________*/
void Game_Init(void)
{
    UI_Load();
    Tuning_Init();
    Hud_Load();

    CP_Settings_NoStroke();
    CP_Settings_ImageMode(CP_POSITION_CENTER);
    CP_Settings_RectMode(CP_POSITION_CENTER);

    width = CP_System_GetWindowWidth();
    height = CP_System_GetWindowHeight();
    gameScale = (float)height / BASE_GAME_HEIGHT;
    laneGap = (float)height / 3.0f;
    topLaneY = (float)height / 6.0f;
    playerX = (float)width * PLAYER_X_FRACTION;

    bgImage = CP_Image_Load("Assets/background.png");

    LoadCharacters();
    LoadObstacles();
    LoadPickups();
    LoadAudio();

    if (gameMusic != NULL)
    {
        CP_Sound_PlayAdvanced(gameMusic, GAME_MUSIC_VOL, 1.0f, true, CP_SOUND_GROUP_MUSIC);
    }

    StartRun();
}

/*! Specification for function Game_Update:
@brief Updates and draws the game each frame.

Handles pausing, the debug tools, the world update, drawing, the pause
menu and the results screen.

@param none.
@return none.
*//*________________________________________________________________________________*/
void Game_Update(void)
{
    float frameDt = CP_System_GetDt();
    float worldDt = frameDt;
    bool justPaused = false;
    bool busy = leavingGame || Transition_IsBusy();

    CP_Settings_ResetMatrix();

    Debug_Update();
    Tuning_Update(frameDt);

    /* ---------- Pause keys (closing the menu is handled by the menu) ---------- */
    if (!busy && state == STATE_PLAYING && !isPaused &&
        (CP_Input_KeyTriggered(KEY_ESCAPE) || CP_Input_KeyTriggered(KEY_SPACE) || CP_Input_KeyTriggered(KEY_P)))
    {
        UI_PlayClick();
        SetPaused(true);
        justPaused = true;
    }

    /* ---------- Debug freeze / single-step ---------- */
    if (Debug_IsFrozen())
    {
        worldDt = Debug_IsSteppingFrame() ? DEBUG_FIXED_STEP : 0.0f;
    }

    {
        DebugCommand command = Debug_TakeCommand();

        if (state == STATE_PLAYING && !isPaused)
        {
            ApplyDebugCommand(command);
        }
    }

    /* ---------- World update ---------- */
    Debug_ProfileBegin(DEBUG_SECTION_UPDATE);

    if (!isPaused && worldDt > 0.0f)
    {
        if (state == STATE_PLAYING)
        {
            UpdatePlaying(worldDt);
        }
        else
        {
            UpdateDeath(worldDt);
        }
    }

    Debug_ProfileEnd(DEBUG_SECTION_UPDATE);

    /* ---------- Draw world ---------- */
    Debug_ProfileBegin(DEBUG_SECTION_DRAW_WORLD);
    DrawWorld();

    if (Debug_IsOverlayOn())
    {
        DrawDebugHitboxes();
    }

    Debug_ProfileEnd(DEBUG_SECTION_DRAW_WORLD);

    /* ---------- Draw UI and handle menus ---------- */
    Debug_ProfileBegin(DEBUG_SECTION_DRAW_UI);

    if (state == STATE_PLAYING && !isPaused)
    {
        RageOverlay_Draw();
    }

    {
        HudData hud;

        hud.score = (int)score;
        hud.coins = coinCount;
        hud.distance = (int)distanceMetres;
        hud.multiplier = scoreMultiplier;
        hud.lives = lives;
        hud.powerUpTime[PU_INVINCIBLE] = invincibleTimer;
        hud.powerUpTime[PU_SHIELD] = shieldTimer;
        hud.powerUpTime[PU_MAGNET] = magnetTimer;
        Hud_Draw(&hud);
    }

    DrawRoulette();

    if (state == STATE_PLAYING && !isPaused && !busy && GameMenus_PauseButton())
    {
        SetPaused(true);
        justPaused = true;
    }

    if (isPaused && !busy)
    {
        switch (GameMenus_PauseMenu(!justPaused))
        {
        case MENU_ACTION_RESUME:
            SetPaused(false);
            break;

        case MENU_ACTION_RESTART:
            leavingGame = true;
            Transition_GoTo(Game_Init, Game_Update, Game_Exit);
            break;

        case MENU_ACTION_MAIN_MENU:
            leavingGame = true;
            Transition_GoTo(MainMenu_Init, MainMenu_Update, MainMenu_Exit);
            break;

        default:
            break;
        }
    }

    if (showDeathScreen)
    {
        RunResults results;
        MenuAction action;

        results.baseScore = (int)score;
        results.multiplier = scoreMultiplier;
        results.finalScore = FinalScore();
        results.coins = coinCount;
        results.isNewHighScore = results.finalScore > 0 && results.finalScore > highScoreAtStart;
        results.causeText = deathCauseText;

        action = GameMenus_DeathScreen(&results);

        if (!busy && action == MENU_ACTION_RESTART)
        {
            leavingGame = true;
            Transition_GoTo(Game_Init, Game_Update, Game_Exit);
        }
        else if (!busy && action == MENU_ACTION_MAIN_MENU)
        {
            leavingGame = true;
            Transition_GoTo(MainMenu_Init, MainMenu_Update, MainMenu_Exit);
        }
    }

    Debug_ProfileEnd(DEBUG_SECTION_DRAW_UI);

    /* ---------- Tools (drawn last, off by default) ---------- */
    if (Debug_IsOverlayOn())
    {
        DrawDebugOverlay();
    }

    Tuning_Draw();
}

/*! Specification for function Game_Exit:
@brief Cleans up the gameplay state when it is left.

Stops all sounds and frees every sound, image and animation loaded by Game_Init.

@param none.
@return none.
*//*________________________________________________________________________________*/
void Game_Exit(void)
{
    CP_Sound_StopAll();
    CP_Sound_ResumeGroup(CP_SOUND_GROUP_MUSIC);   /* undo a pause so the menu music can play */
    isPaused = false;

    FreeSound(&sndJump);
    FreeSound(&sndCoin);
    FreeSound(&gameMusic);
    FreeSound(&sndDeath);
    FreeSound(&sndHit);
    FreeSound(&sndHeart);
    FreeSound(&sndRouletteSpin);
    FreeSound(&sndRouletteReveal);
    FreeSound(&sndMultiplier);

    FreeImage(&bgImage);
    FreeImage(&boxImg);
    FreeImage(&heartPickupImg);
    FreeImage(&multiplierItemImg);

    Anim_Free(&runAnim);
    Anim_Free(&chaseAnim);
    Anim_Free(&grabAnim);
    Anim_Free(&reviveAnim);
    Anim_Free(&oilSpawnAnim);
    Anim_Free(&barrierBreakAnim);
    Anim_Free(&holeDeathAnim);
    Anim_Free(&oilDeathAnim);
    Anim_Free(&coinAnim);

    for (int t = 0; t < NUM_OBS_TYPES; t++)
    {
        Anim_Free(&obsAnim[t]);
    }

    for (int t = 0; t < NUM_POWERUPS; t++)
    {
        Anim_Free(&puIconAnim[t]);
        Anim_Free(&puFx[t]);
    }

    activeDeathAnim = NULL;
    Hud_Free();
    RageOverlay_Reset();
}