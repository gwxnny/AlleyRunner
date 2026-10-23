#include "cprocessing.h"
#include "game.h"
#include <math.h>
#include "mainmenu.h"
#include "utils.h"
#include <stdbool.h>


/*Sounds*/
CP_Sound sndJump;
CP_Sound sndMusic;
CP_Sound sndDeath;

#define WIN_W        1280
#define WIN_H        720

#define MAX_OBS 4          /* how many obstacles exist at once */
#define OBS_SPACING 450.0f /* minimum gap between obstacles (px) */

// ---- sprite sizes (must match the sheets in Assets) ----
#define PLAYER_W     96.0f
#define PLAYER_H     96.0f
#define ENEMY_W      192.0f
#define ENEMY_H      192.0f
#define GRAB_W       288.0f   // grab frames are wider: enemy on the left, player on the right
#define GRAB_H       192.0f
#define ENEMY_BODY_X 96.0f    // enemy's body centre inside a grab frame
#define GRAB_REACH   130.0f   // distance from enemy body centre to player centre in a grab frame

#define ENEMY_WALK_SPEED 250.0f


int width;
int height;

float speed;

float player_x;
float player_y;
float box_size;

/* Lanes: 0 = top, 1 = middle, 2 = bottom */
int lane;
float lane_gap;
float top_lane_y;

/* Obstacle */
//float obs_x;
//float obs_y;
//int obs_lane;
float obs_x[MAX_OBS];
int   obs_lane[MAX_OBS];

/* 1 = alive, 0 = dead */
int alive;


typedef enum { STATE_PLAYING, STATE_ENEMY_WALK, STATE_GRAB } GameState;

typedef struct
{
    CP_Image sheet;
    int      frameCount;
    float    frameW;
    float    frameH;
    int      currentFrame;
    float    timer;
    float    frameTime;
    int      loop;
} Animation;

Animation runAnim, chaseAnim, grabAnim;
GameState state;

int   playerLane;
float playerX;
float enemyX;
float enemyGround;   // y of the enemy's feet (it stays in the middle lane until it walks to the player)

float obstacleX;
int   obstacleLane;
float obstacleSpeed = 300.0f;

void Anim_Init(Animation* a, const char* path, int frameCount,
    float frameW, float frameH, float frameTime, int loop)
{
    a->sheet = CP_Image_Load(path);
    a->frameCount = frameCount;
    a->frameW = frameW;
    a->frameH = frameH;
    a->currentFrame = 0;
    a->timer = 0.0f;
    a->frameTime = frameTime;
    a->loop = loop;
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
        a->timer = 0.0f;
        a->currentFrame++;
        if (a->currentFrame >= a->frameCount)
        {
            if (a->loop)
                a->currentFrame = 0;
            else
                a->currentFrame = a->frameCount - 1;   // freeze on the last frame
        }
    }
}

// x, y = centre of the sprite
void Anim_Draw(Animation* a, float x, float y)
{
    float left = a->currentFrame * a->frameW;
    CP_Image_DrawSubImage(a->sheet, x, y, a->frameW, a->frameH,
        left, 0.0f, left + a->frameW, a->frameH, 255);
}
/* y of the "feet" line for a lane (same line as the triangle bases) */
float LaneGround(int l)
{
    return top_lane_y + l * lane_gap + box_size;
}

void StartRun(void)
{
    state = STATE_PLAYING;
    playerLane = 1;
    playerX = width * 0.65f;
    enemyX = width * 0.12f;     // big enemy, far behind the player
    enemyGround = LaneGround(1);    // enemy always chases from the middle lane
    Anim_Reset(&runAnim);
    Anim_Reset(&chaseAnim);
    Anim_Reset(&grabAnim);
    //SpawnObstacle();
}
void Game_Init(void)
{
    //CP_System_SetWindowSize(WIN_W, WIN_H);
    sndMusic = CP_Sound_Load("./Assets/bg_music.mp3");
    sndJump = CP_Sound_Load("./Assets/jump.mp3");
    sndDeath = CP_Sound_Load("./Assets/game_over.mp3");

    /*Looping music*/
    CP_Sound_PlayAdvanced(sndMusic, 0.5f, 1.0f, true, CP_SOUND_GROUP_MUSIC);

    Anim_Init(&runAnim, "./Assets/player_run.png", 4, PLAYER_W, PLAYER_H, 0.15f, 1);
    Anim_Init(&chaseAnim, "./Assets/enemy_chase.png", 4, ENEMY_W, ENEMY_H, 0.15f, 1);
    Anim_Init(&grabAnim, "./Assets/enemy_grab.png", 6, GRAB_W, GRAB_H, 0.20f, 0);

    StartRun();

    speed = 800;
    CP_Settings_NoStroke();
    
    width = CP_System_GetWindowWidth();
    height = CP_System_GetWindowHeight();


    /* Lanes and player */
    box_size = 80;
    lane = 1;                   
    lane_gap = height / 3.0f;
    top_lane_y = height / 6.0f;

    player_x = width * 0.4f;
    player_y = top_lane_y + lane * lane_gap;

    /* Obstacle starts off screen on the right, in a random lane */
    //obs_x = (float)width;
    //obs_lane = CP_Random_RangeInt(0, 2);
    for (int i = 0; i < MAX_OBS; i++)
    {
        obs_x[i] = (float)width + i * OBS_SPACING;
        obs_lane[i] = CP_Random_RangeInt(0, 2);
    }

    enemyX = width * 0.12f;          /* left side of the screen */
    enemyGround = LaneGround(lane);     /* middle lane */
    
    alive = 1;
}

void Game_Update(void)
{
    float dt = CP_System_GetDt();

    //player_y = top_lane_y + lane * lane_gap;
    //obs_y = top_lane_y + obs_lane * lane_gap;

    if (state == STATE_PLAYING)
    {
        /* switching lanes */
        if (CP_Input_KeyTriggered(KEY_W) && lane > 0)
        {
            lane--;
            CP_Sound_Play(sndJump);
        }
        if (CP_Input_KeyTriggered(KEY_S) && lane < 2)
        {
            lane++;
            CP_Sound_Play(sndJump);

        }
        player_y = top_lane_y + lane * lane_gap;

        ///* Moving the obstacle */
        //obs_x -= speed * dt;

        ///* Respawn obstacle on a random lane */
        //if (obs_x + box_size < 50)
        //{
        //    obs_x = (float)width;
        //    obs_lane = CP_Random_RangeInt(0, 2);
        //}
        //obs_y = top_lane_y + obs_lane * lane_gap;

        ///* Hit obstacle -> the enemy walks in from off screen (left) */
        //if (obs_lane == lane &&
        //    obs_x < player_x &&
        //    obs_x + box_size > player_x)
        //{
        //    alive = 0;
        //    state = STATE_ENEMY_WALK;
        //}

        for (int i = 0; i < MAX_OBS; i++)
        {
            obs_x[i] -= speed * dt;

            /* Respawn behind the furthest obstacle */
            if (obs_x[i] + box_size < 0)
            {
                float furthest = (float)width;
                for (int j = 0; j < MAX_OBS; j++)
                    if (obs_x[j] > furthest)
                        furthest = obs_x[j];

                obs_x[i] = furthest + OBS_SPACING + CP_Random_RangeInt(0, 1);
                obs_lane[i] = CP_Random_RangeInt(0, 2);
            }

            /* Hit check */
            if (obs_lane[i] == lane &&
                obs_x[i] < player_x &&
                obs_x[i] + box_size > player_x)
            {
                alive = 0;
                state = STATE_ENEMY_WALK;
                CP_Sound_Play(sndDeath);
                CP_Sound_StopGroup(CP_SOUND_GROUP_MUSIC);
            }
        }

        Anim_Update(&runAnim);
        Anim_Update(&chaseAnim);
    }
    else if (state == STATE_ENEMY_WALK)
    {
        /* player and obstacle freeze, the enemy walks to the player */
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
            //CP_Sound_Play(sndDeath);
        }
        else
        {
            enemyX += dx / dist * step;
            enemyGround += dy / dist * step;
        }
    }
    else /* STATE_GRAB */
    {
        Anim_Update(&grabAnim);
        
    }

    CP_Graphics_ClearBackground(CP_Color_Create(255, 255, 255, 255));

    /* red triangle */
    CP_Settings_Fill(CP_Color_Create(220, 30, 30, 255));
    for (int i = 0; i < MAX_OBS; i++)
    {
        float y = top_lane_y + obs_lane[i] * lane_gap;
        CP_Graphics_DrawTriangle(obs_x[i], y + box_size,
            obs_x[i] + box_size, y + box_size,
            obs_x[i] + box_size / 2, y);
    }

    /* characters */
    if (state == STATE_PLAYING)
    {
        Anim_Draw(&runAnim, player_x, LaneGround(lane) - PLAYER_H / 2.0f);
        Anim_Draw(&chaseAnim, enemyX, enemyGround - ENEMY_H / 2.0f);
    }
    else if (state == STATE_ENEMY_WALK)
    {
        Anim_Draw(&runAnim, player_x, LaneGround(lane) - PLAYER_H / 2.0f);
        Anim_Draw(&chaseAnim, enemyX, enemyGround - ENEMY_H / 2.0f);
    }
    else /* STATE_GRAB: this frame already contains both enemy and player */
    {
        Anim_Draw(&grabAnim, enemyX + (GRAB_W / 2.0f - ENEMY_BODY_X),
            enemyGround - GRAB_H / 2.0f);
    }

    /* Death screen only once the grab animation has finished */
    bool grabDone = (state == STATE_GRAB &&
        grabAnim.currentFrame == grabAnim.frameCount - 1);

    if (grabDone)
    {
        CP_Settings_Fill(CP_Color_Create(0, 0, 0, 255));
        CP_Settings_TextSize(100);
        CP_Settings_TextAlignment(CP_TEXT_ALIGN_H_CENTER, CP_TEXT_ALIGN_V_MIDDLE);
        CP_Font_DrawText("YOU DIED", 800, 225);

        /* Restart button */
        CP_Settings_Fill(CP_Color_Create(100, 220, 100, 255));
        CP_Graphics_DrawRect(400, 400, 300, 200);
        CP_Settings_Fill(CP_Color_Create(0, 0, 0, 255));
        CP_Settings_TextSize(80);
        CP_Font_DrawText("RESTART", 400, 400);

        /* Exit button */
        CP_Settings_Fill(CP_Color_Create(255, 0, 0, 255));
        CP_Graphics_DrawRect(1200, 400, 300, 200);
        CP_Settings_Fill(CP_Color_Create(0, 0, 0, 255));
        CP_Settings_TextSize(100);
        CP_Font_DrawText("EXIT", 1200, 400);

        if (CP_Input_MouseClicked())
        {
            float Xpos = CP_Input_GetMouseX();
            float Ypos = CP_Input_GetMouseY();

            if (IsAreaClicked(400, 400, 300, 200, Xpos, Ypos))
            {
                CP_Engine_SetNextGameStateForced(Game_Init, Game_Update, Game_Exit);
            }
            if (IsAreaClicked(1200, 400, 300, 200, Xpos, Ypos))
            {
                CP_Engine_SetNextGameStateForced(Main_Menu_Init, Main_Menu_Update, Main_Menu_Exit);
            }
        }
    }

    if (CP_Input_KeyDown(KEY_ESCAPE))
    {
        CP_Engine_Terminate();
    }

    if (CP_Input_KeyDown(KEY_Q))
    {
        CP_Engine_SetNextGameStateForced(Main_Menu_Init, Main_Menu_Update, Main_Menu_Exit);
    }
}

void Game_Exit(void)
{
    CP_Sound_StopAll();
    CP_Sound_Free(&sndJump);
    CP_Sound_Free(&sndMusic);

    CP_Image_Free(&runAnim.sheet);
    CP_Image_Free(&chaseAnim.sheet);
    CP_Image_Free(&grabAnim.sheet);
}