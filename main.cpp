#include <iostream>
#include <cstdio>
#include <random>

#include "graphics.h"
#include "util.h"
#include "config.h"
#include "world.h"

using namespace std;
using namespace world;

void draw_wall(line_t wall)
{
	nvgBeginPath(graphics::vg);
	nvgMoveTo(graphics::vg, wall.p.x, wall.p.y);
	nvgLineTo(graphics::vg, wall.q.x, wall.q.y);
	nvgStrokeColor(graphics::vg, nvgRGBAf(0, 0, 1, .5));
	nvgStrokeWidth(graphics::vg, 3 * graphics::one_pixel);
	nvgStroke(graphics::vg);
}

void dump()
{
	FILE* f = fopen(config::get<std::string>("data", "walls").c_str(), "w");
	for (line_t wall : walls)
	{
		fprintf(f, "%f %f %f %f\n", wall.p.x, wall.p.y, wall.q.x, wall.q.y);
	}
	fclose(f);
}

int main(int argc, char** argv)
{
	config::read("config.ini");
	util::init();
	world::init();
	graphics::init();

	atexit(dump);

	int step_counter = 0;
	while (!graphics::should_exit())
	{
		++step_counter;
		graphics::begin_frame();
		for (int i = 0; i < n; i++)
		{
			for (int j = 0; j < m; j++)
			{
				graphics::draw(circle_t{ j*spacing, i*spacing, .2f }, nvgRGBAf(0, 0, 0, 1.0));
			}
		}
		for (line_t& wall : walls)
		{
			draw_wall(wall);
		}
		if (drawing) draw_wall(last_wall);
		graphics::end_frame();
	}

	return 0;
}