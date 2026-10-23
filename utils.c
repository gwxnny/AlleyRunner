#include <math.h>
#include "cprocessing.h"

int IsAreaClicked(float area_center_x, float area_center_y, float area_width, float area_height, float click_x, float click_y)
{
	float xmin = area_center_x - (area_width / 2);
	float xmax = area_center_x + (area_width / 2);

	float ymin = area_center_y - (area_height / 2);
	float ymax = area_center_y + (area_height / 2);

	if (click_x < xmax && click_x > xmin && click_y < ymax && click_y > ymin)
	{
		return 1;
	}
	return 0;
}

int IsCircleClicked(float circle_center_x, float circle_center_y, float diameter, float click_x, float click_y)
{
	if (fabs(click_x - circle_center_x) > (diameter / 2))return 0;
	if (fabs(click_y - circle_center_y) > (diameter / 2))return 0;
	return 1;
}

