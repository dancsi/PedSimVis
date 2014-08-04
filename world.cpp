#include "world.h"
#include "config.h"
#include "util.h"
#include "vec.h"
#include "graphics.h"

#include <limits>
#include <set>
#include <algorithm>

using namespace std;
using namespace util;

namespace world
{
	int width, height, n, m;
	float spacing;
	vector<line_t> walls;
	bool drawing;
	line_t last_wall;
	void init()
	{
		width = config::get<int>("world", "width");
		height = config::get<int>("world", "height");
		spacing = config::get<double>("world", "spacing");
		n = height / spacing + 1;
		m = width / spacing + 1;
		FILE *fwalls = fopen(config::get<std::string>("data", "walls").c_str(), "r");
		if (fwalls)
		{
			line_t wall;
			while (fscanf(fwalls, "%f%f%f%f", &wall.p.x, &wall.p.y, &wall.q.x, &wall.q.y) == 4)
			{
				walls.push_back(wall);
			}
			fclose(fwalls);
		}
	}
}