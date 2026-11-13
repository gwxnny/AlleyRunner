/*!
@file       : utils.h
@team       : IsaacACat
@module     : CSD1401 Software Engineering Project 1
@section    : section b
@project    : Team Project 1 - Alley Runner
@brief      : This file contains the interface for utility functions for the Alley Runner game.
*//*________________________________________________________________________________*/
#pragma once

int IsAreaClicked(
    float area_center_x,
    float area_center_y,
    float area_width,
    float area_height,
    float click_x,
    float click_y
);

int IsCircleClicked(
    float circle_center_x,
    float circle_center_y,
    float diameter,
    float click_x,
    float click_y
);

float ClampFloat(
    float value,
    float min_value,
    float max_value
);

int IsMouseInButton(float btn_x, float btn_y, float w, float h);