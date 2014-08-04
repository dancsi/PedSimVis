#pragma once

#include "field.h"
#include "geometry.h"

#include <vector>

using namespace std;

namespace world
{
	extern int width, height, n, m;
	extern float spacing;
	extern vector<line_t> walls;
	extern line_t last_wall;
	extern bool drawing;
	void init();
}