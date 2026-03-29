#include "engine.h"
#include "spatial.h"
#include "buffer.h"
#include <thread>
#include <chrono>
//#include <opencv2/opencv.hpp>
#include "pybind11/pybind11.h"
namespace py = pybind11;

void Engine::init() {
	// If its unclear what the point of the fps is, the main purpose is to dictate how fast frames inside the buffer get added
	this->isRunning = false; // Enable the Engine, have options to pause as well
	
	int sleep_ms = (int)(this->deltaTime * 1000);

	this->frame_buffer = (uint32_t*)malloc(window_height * window_width * sizeof(uint32_t));
	this->depth_buffer = (float*)malloc(window_width * window_height * sizeof(float));

	for (int i = 0; i < window_width * window_height; i++) {
		this->depth_buffer[i] = FLT_MAX;
	}
}

void Engine::render() {
	// clear buffers each frame
	memset(this->frame_buffer, 0, window_width * window_height * sizeof(uint32_t));
	for (int i = 0; i < window_width * window_height; i++) {
		depth_buffer[i] = FLT_MAX;
	}

	for (int i = 0; i < this->shapeCount; i++) {
		// get shape from arena
		Shape* shape = (Shape*)this->allShapes[i].base;
		if (shape == nullptr) {
			printf("Shape is null at index %d\n", i);
			continue;
		}

		for (int f = 0; f < shape->face_count; f++) {
			rasterize_face(
				this,
				shape->faces[f],
				shape,
				&this->camera,
				90.0f,
				(float)window_width / (float)window_height,
				0.1f,
				1000.0f,
				window_width,
				window_height
			);
		} 
	}
}

void Engine::update() {
	this->render();
	//printf("Renderer has ran");

	//std::cout << frame_buffer[0] << std::endl;
	//cv::Mat frame(this->screen_height, this->screen_width, CV_8UC4, frame_buffer);
	//cv::cvtColor(frame, frame, cv::COLOR_BGRA2RGBA);
	//cv::imshow("Renderer", frame);
	//cv::waitKey(1);
}


Shape* Engine::addShape(std::string filepath, point_t position, int width, int height) {
	Arena arena = create_arena(sizeof(Shape));

	this->allShapes = (Arena*)realloc(this->allShapes, (this->shapeCount + 1) * sizeof(Arena));
	this->allShapes[this->shapeCount] = arena;
	this->shapeCount++;

	// get pointer from stored arena THEN load data into it
	Shape* shape = (Shape*)this->allShapes[this->shapeCount - 1].base;
	shape->initShape(position, width, height);
	shape->loadShape(filepath);
	return shape;
}

void Engine::removeShape(int index) {
	arena_free(&this->allShapes[index]);
	// Free from memory then shift everything after by 1
	for (int i = index; i < this->shapeCount - 1; i++) {
		this->allShapes[i] = this->allShapes[i + 1];
	}
	this->shapeCount--;
}

void Engine::updateCamera(point_t position, float angleX, float angleY, float angleZ)
{
	// Update the cameras position, do this everytime the camera moves a lil bit
	this->camera.position = position;
	this->camera.angleX = angleX;
	this->camera.angleY = angleY;
	this->camera.angleZ = angleZ;
}

void Engine::create_light(point_t position, float intensity, uint8_t* colourRGB) {

	uint32_t colour = convert_colour(colourRGB);
	light_t newLight;
	newLight.position = position;
	newLight.intensity = 0.9f;
	newLight.colour = colour;
	this->lights = (light_t*)realloc(this->lights, (this->light_count + 1) * sizeof(light_t));
	this->lights[light_count] = newLight;

	this->light_count++;
}

py::bytes Engine::get_framebuffer() {
	return py::bytes((char*)this->frame_buffer, window_width * window_height * sizeof(uint32_t));
}
