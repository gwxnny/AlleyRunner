#include "cprocessing.h"
#include "game.h"
#include "utils.h"
#include <stdbool.h>

CP_Font Font;
CP_Sound sndMusic;

void Main_Menu_Init(void)
{
	sndMusic = CP_Sound_Load("./Assets/bg_music.mp3");
	CP_Sound_PlayAdvanced(sndMusic, 0.5f, 1.0f, true, CP_SOUND_GROUP_MUSIC);

	Font = CP_Font_Load("Assets/Exo2-Regular.ttf");
	CP_Font_Set(Font);
	
	CP_Settings_RectMode(CP_POSITION_CENTER);
	CP_Settings_Stroke(CP_Color_Create(0, 0, 0, 255)); // can be removed after adding real background
}

void Main_Menu_Update(void)
{
	CP_Graphics_ClearBackground(CP_Color_Create(211, 211, 211, 255));

	CP_Settings_Fill(CP_Color_Create(255, 127, 127, 255));
	CP_Graphics_DrawRect(800, 225, 300, 200);

	CP_Settings_Fill(CP_Color_Create(0, 0, 0, 255));
	CP_Settings_TextSize(100);
	CP_Settings_TextAlignment(CP_TEXT_ALIGN_H_CENTER, CP_TEXT_ALIGN_V_MIDDLE);
	CP_Font_DrawText("Play", 800, 225);

	CP_Settings_Fill(CP_Color_Create(255, 127, 127, 255));
	CP_Graphics_DrawRect(800, 650, 300, 200);

	CP_Settings_Fill(CP_Color_Create(0, 0, 0, 255));
	CP_Settings_TextSize(100);
	CP_Settings_TextAlignment(CP_TEXT_ALIGN_H_CENTER, CP_TEXT_ALIGN_V_MIDDLE);
	CP_Font_DrawText("Exit", 800, 650);

	if (CP_Input_MouseClicked())
	{
		float Xpos = CP_Input_GetMouseX();
		float Ypos = CP_Input_GetMouseY();

		if (IsAreaClicked(800, 225, 300, 200, Xpos, Ypos))
		{
			CP_Engine_SetNextGameStateForced(Game_Init, Game_Update, Game_Exit);
		}

		if (IsAreaClicked(800, 650, 300, 200, Xpos, Ypos))
		{
			CP_Engine_Terminate();
		}
	}
}

void Main_Menu_Exit(void)
{

}