#include "graphics.h"
#include "util.h"
#include "vec.h"
#include "field.h"
#include "geometry.h"
#include "world.h"

#define NANOVG_GLEW
#define GLEW_STATIC

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include "nanovg.h"
#define NANOVG_GL3_IMPLEMENTATION
#include "nanovg_gl.h"
#include "perf.h"

using namespace world;

namespace graphics
{
	GLFWwindow* window;
	int window_width, window_height;
	int fb_width, fb_height;
	float one_pixel = 1.0;

	NVGcontext* vg = NULL;
	GPUtimer gpu_timer;
	PerfGraph fps_graph, cpu_graph, gpu_graph;
	double prev_time = 0, cpu_time = 0, render_start_time, delta_time = 1./60.0;

	vecd_t mouse_pos;

	void mouse_callback(GLFWwindow* window, int button, int action, int mods)
	{
		vecd_t pos;
		glfwGetCursorPos(window, &pos.x, &pos.y);
		vec_t world_pos = screen2world({ pos.x, pos.y });
		world_pos = { round(world_pos.x), round(world_pos.y) };
		if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
		{
			world_pos.x -= 0.1;
		}
		if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
		{
			world_pos.x += 0.1;
		}
		if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
		{
			world_pos.y -= 0.1;
		}
		if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
		{
			world_pos.y += 0.1;
		}

		if (action == GLFW_RELEASE)
		{
			if (drawing)
			{
				drawing = false;
				last_wall.q = world_pos;
				walls.push_back(last_wall);
			}
			else
			{
				drawing = true;
				last_wall.p = last_wall.q = world_pos;
			}
		}
	}

	void move_callback(GLFWwindow* window, double x, double y)
	{
		vecd_t pos = { x, y };
		vec_t world_pos = screen2world({ pos.x, pos.y });
		world_pos = { round(world_pos.x), round(world_pos.y) };
		if (drawing)
		{
			last_wall.q = world_pos;
		}
		else
		{
			nvgBeginPath(graphics::vg);
			nvgCircle(graphics::vg, world_pos.x, world_pos.y, .3);
			nvgFillColor(graphics::vg, nvgRGBAf(0, 0, 1, .7));
			nvgFill(graphics::vg);
		}
	}

	void init()
	{
		if (!glfwInit()) {
			LOG("Failed to init GLFW.");
			exit(-1);
		}

		initGraph(&fps_graph, GRAPH_RENDER_FPS, "Frame Time");
		initGraph(&cpu_graph, GRAPH_RENDER_MS, "CPU Time");
		initGraph(&gpu_graph, GRAPH_RENDER_MS, "GPU Time");

		glfwSetErrorCallback([](int error, const char* desc) {LOG("GLFW error %d: %s\n", error, desc); });
		glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, 1);
		int height = glfwGetVideoMode(glfwGetPrimaryMonitor())->height*0.6, width = height*double(world::width) / double(world::height);
		window = glfwCreateWindow(width, height, "PedestrianSimulator", NULL, NULL); if (!window) {
			glfwTerminate();
			exit(-1);
		}
		glfwSetKeyCallback(window, [](GLFWwindow* window, int key, int scancode, int action, int mods)
		{
			NVG_NOTUSED(scancode);
			NVG_NOTUSED(mods);
			if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
				glfwSetWindowShouldClose(window, GL_TRUE);
			if (key == GLFW_KEY_S && action == GLFW_RELEASE)
			{
				save_svg();
			}
		});

		glfwSetMouseButtonCallback(window, mouse_callback);
		glfwSetCursorPosCallback(window, move_callback);

		glfwMakeContextCurrent(window);
		glewExperimental = GL_TRUE;
		if (glewInit() != GLEW_OK) {
			LOG("Could not init glew.\n");
			exit(-1);
		}
		glGetError();
		vg = nvgCreateGL3(512, 512, NVG_ANTIALIAS | NVG_STENCIL_STROKES);
		if (vg == NULL) {
			LOG("Could not init nanovg.\n");
			exit(-1);
		}

		nvgCreateFont(vg, "sans", "roboto.ttf");

		glfwSwapInterval(0);
		initGPUTimer(&gpu_timer);
		glfwSetTime(0);
		prev_time = glfwGetTime();
	}

	void save_svg()
	{
		FILE* fout = fopen("screenshot.svg", "w");
		fprintf(fout, "<svg xmlns=\"http://www.w3.org/2000/svg\" version=\"1.1\">\n<g transform=\"scale(4)\">\n");
		fprintf(fout, "<rect x=\"0\" y=\"0\" width=\"%d\" height=\"%d\" fill=\"rgb(30%, 30%, 32%)\" />\n", world::width, world::height);
		for (int i = 0; i < n; i++)
		{
			for (int j = 0; j < m; j++)
			{
				fprintf(fout, "<circle cx=\"%.2f\" cy=\"%.2f\" r=\"0.2\" fill=\"black\" />\n", j*world::spacing, i*world::spacing);
			}
		}
		for (line_t wall : world::walls)
		{
			fprintf(fout, "<line x1=\"%f\" y1=\"%f\" x2=\"%f\" y2=\"%f\" stroke=\"blue\" stroke-width=\".1\" stroke-linecap=\"square\" />\n", wall.p.x, wall.p.y, wall.q.x, wall.q.y, 3*graphics::one_pixel);
		}
		fprintf(fout, "</g></svg>");
		fclose(fout);
		fprintf(stderr, "SVG saved.\n");
	}

	bool should_exit()
	{
		return glfwWindowShouldClose(window);
	}

	void begin_frame()
	{
		render_start_time = glfwGetTime();
		delta_time = render_start_time - prev_time;
		prev_time = render_start_time;

		startGPUTimer(&gpu_timer);
		glfwGetCursorPos(window, &mouse_pos.x, &mouse_pos.y);
		glfwGetWindowSize(window, &window_width, &window_height);
		glfwGetFramebufferSize(window, &fb_width, &fb_height);
		glViewport(0, 0, fb_width, fb_height);
		glClearColor(0.3, 0.3, 0.32, 1);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

		nvgBeginFrame(vg, fb_width, fb_height, 1);
		draw_world_scale();
	}

	void end_frame()
	{
		draw_ui_scale();
		//renderGraph(vg, 5, 5, &fps_graph);
		//renderGraph(vg, 5 + 200 + 5, 5, &cpu_graph);
		if (gpu_timer.supported)
			renderGraph(vg, 5 + 200 + 5 + 200 + 5, 5, &gpu_graph);
		draw_world_scale();
		nvgEndFrame(vg);

		cpu_time = glfwGetTime() - render_start_time;

		updateGraph(&fps_graph, delta_time);
		updateGraph(&cpu_graph, cpu_time);

		float gpu_times[3];
		int n = stopGPUTimer(&gpu_timer, gpu_times, 3);
		for (int i = 0; i < n; i++)
			updateGraph(&gpu_graph, gpu_times[i]);

		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	void destroy()
	{
		nvgDeleteGL3(vg);
		glfwTerminate();
	}

	void draw_ui_scale()
	{
		nvgResetTransform(vg);
		one_pixel = 1.0;
	}

	void draw_world_scale()
	{
		nvgResetTransform(vg);
		nvgTranslate(vg, fb_width / 2, fb_height / 2);
		nvgScale(vg, double(fb_width) / world::width, double(fb_height) / world::height);
		nvgTranslate(vg, -world::width / 2, -world::height / 2);
		one_pixel = double(world::width) / fb_width;
	}

	template<>
	void draw<vec_field_t>(vec_field_t& vf, NVGcolor& color)
	{
		vf.foreach_element([&](size_t i, size_t j, vec_t& v){
			vec_t pos = vf.get_coordinates(i, j), to = pos + v;

			nvgBeginPath(vg);
			nvgCircle(vg, pos.x, pos.y, .2);
			nvgFillColor(vg, color);
			nvgFill(vg);

			nvgBeginPath(vg);
			nvgMoveTo(vg, pos.x, pos.y);
			nvgLineTo(vg, to.x, to.y);
			nvgStrokeColor(vg, color);
			nvgStrokeWidth(vg, one_pixel);
			nvgStroke(vg);
		});
	}

	template<>
	void draw<circle_t>(circle_t& circle, NVGcolor& color)
	{
		nvgBeginPath(graphics::vg);
		nvgCircle(graphics::vg, circle.x, circle.y, circle.r);
		nvgFillColor(graphics::vg, color);
		nvgFill(graphics::vg);
	}

	vec_t world2screen(vec_t v)
	{
		float transform[6];
		nvgCurrentTransform(vg, transform);
		vec_t ret;
		nvgTransformPoint(&ret.x, &ret.y, transform, v.x, v.y);
		return ret;
	}

	vec_t screen2world(vec_t v)
	{
		float transform[6], itransform[6];
		nvgCurrentTransform(vg, transform);
		nvgTransformInverse(itransform, transform);
		vec_t ret;
		nvgTransformPoint(&ret.x, &ret.y, itransform, v.x, v.y);
		return ret;
	}

}


