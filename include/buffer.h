#pragma once
#include <stdint.h>
#include "spatial.h"
#include "engine.h"

/// <summary>
/// Frame class that contains essential methods for creating a Frame.
/// </summary>
class Frame {
	public:
		uint32_t* pixels;
		int width;
		int height;

		static void clear(Frame* frame);
		static void createFrame(Frame* frame);

};

// Buffers for access
extern float* depth_buffer;
extern uint32_t* frame_buffer;


/// <summary>
/// Converts a vertex into a bunch of points relative to the screen
/// </summary>
/// <param name="vertex">The targeted vertex</param>
/// <param name="shape">Shape object pointer</param>
/// <param name="camera">Camera object</param>
/// <param name="fov">FOV of camera</param>
/// <param name="aspect">aspect ratio of screen</param>
/// <param name="near">clipping range for things too close</param>
/// <param name="far">clipping range for anything further</param>
/// <param name="width">screen width</param>
/// <param name="height">screen height</param>
/// <returns></returns>
point_t vertex_to_screen(vertex_t* vertex, Shape* shape, Camera* camera, float fov, float aspect, float near, float far, int width, int height);

void rasterize_face(Engine* engine, face_t face, Shape* shape, const mat4& mvp, int width, int height);