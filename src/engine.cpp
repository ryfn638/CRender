#include "engine.h"
#include "spatial.h"
#include "buffer.h"
#include <thread>
#include <chrono>
#include <array>
#include "render.cuh"
#include <cuda_runtime.h>
//#include <opencv2/opencv.hpp>
#include "pybind11/pybind11.h"
namespace py = pybind11;

void Engine::init(int screen_width, int screen_height) {
	this->isRunning = false;
	this->screen_height = screen_height;
	this->screen_width = screen_width;

	cudaError_t err = cudaMallocHost(&this->frame_buffer, screen_width * screen_height * sizeof(uint32_t));
	if (err != cudaSuccess) {
		printf("cudaMallocHost failed: %s\n", cudaGetErrorString(err));
		// fallback to regular malloc
		this->frame_buffer = (uint32_t*)malloc(screen_width * screen_height * sizeof(uint32_t));
	}
	printf("frame_buffer = %p\n", this->frame_buffer);

	this->depth_buffer = (float*)malloc(this->screen_width * this->screen_height * sizeof(float));
	for (int i = 0; i < this->screen_width * this->screen_height; i++) {
		this->depth_buffer[i] = FLT_MAX;
	}
}

void Engine::start() {
	gpu_init(this);
}
void Engine::render() {
	gpu_render(this);
	//memset(this->frame_buffer, 0, this->screen_width * this->screen_height * sizeof(uint32_t));
	//std::fill(this->depth_buffer, this->depth_buffer + this->screen_width * this->screen_height, FLT_MAX);

	//for (int i = 0; i < this->shapeCount; i++) {
	//	// get shape from arena
	//	Shape* shape = (Shape*)this->allShapes[i].base;
	//	if (shape == nullptr) {
	//		printf("Shape is null at index %d\n", i);
	//		continue;
	//	}

	//	mat4 mvp = build_mvp(shape, &this->camera,
	//		90.0f * 3.1415/180,
	//		(float)this->screen_width / (float)this->screen_height,
	//		0.001f, 1000.0f);

	//	for (int f = 0; f < shape->face_count; f++) {
	//		rasterize_face(this, shape->faces[f], shape, mvp, screen_width, screen_height);
	//	}
	//}
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
	shape->loadShape(filepath, this->lib);
	shape->index = this->shapeCount - 1;

	return shape;
}

void Engine::removeShape(Shape* shape) {
	arena_free(&this->allShapes[shape->index]);
	// Free from memory then shift everything after by 1
	for (int i = shape->index; i < this->shapeCount - 1; i++) {
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

void Engine::create_light(point_t position, float intensity, std::array<uint8_t, 3> colourRGB) {

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
	return py::bytes((char*)this->frame_buffer, this->screen_width * this->screen_height * sizeof(uint32_t));
}

void Engine::update_light(light_t light, point_t position, float intensity, std::array<uint8_t, 3> colourRGB) {
	light.colour = convert_colour(colourRGB);
	light.position = position;
	light.intensity = intensity;
}

void Engine::cleanup() {
	cudaFreeHost(this->frame_buffer);
	gpu_free(this);
}