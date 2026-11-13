/*!
@file       : utils.c
@team       : IsaacACat
@module     : CSD1401 Software Engineering Project 1
@section    : section b
@project    : Team Project 1 - Alley Runner
@brief      : This file contains utility functions for the Alley Runner game.
*//*________________________________________________________________________________*/
#include "utils.h"
#include "cprocessing.h"

int IsAreaClicked(
    float area_center_x,
    float area_center_y,
    float area_width,
    float area_height,
    float click_x,
    float click_y)
{
    float half_width = area_width / 2.0f;
    float half_height = area_height / 2.0f;

    float xmin = area_center_x - half_width;
    float xmax = area_center_x + half_width;

    float ymin = area_center_y - half_height;
    float ymax = area_center_y + half_height;

    return (
        click_x >= xmin &&
        click_x <= xmax &&
        click_y >= ymin &&
        click_y <= ymax
        );
}

int IsCircleClicked(
    float circle_center_x,
    float circle_center_y,
    float diameter,
    float click_x,
    float click_y)
{
    if (diameter < 0.0f) {
        return 0;
    }

    float radius = diameter / 2.0f;

    float dx = click_x - circle_center_x;
    float dy = click_y - circle_center_y;

    return dx * dx + dy * dy <= radius * radius;
}

float ClampFloat(
    float value,
    float min_value,
    float max_value)
{
    if (value < min_value) {
        return min_value;
    }

    if (value > max_value) {
        return max_value;
    }

    return value;
}

int IsMouseInButton(float btn_x, float btn_y, float w, float h)
{
    float mx = CP_Input_GetMouseX();
    float my = CP_Input_GetMouseY();
    return (mx >= btn_x - w / 2.0f && mx <= btn_x + w / 2.0f &&
        my >= btn_y - h / 2.0f && my <= btn_y + h / 2.0f);
}