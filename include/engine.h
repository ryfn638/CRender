#pragma once
#include "window.h"
#include "spatial.h"
#include "arena.h"
#include <string>
#include "light.h"
#include "pybind11/pybind11.h"
namespace py = pybind11;
/// <summary>
/// Container for a running engine instance, multiple instances can be ran at once
/// </summary>
class Engine {
public:
	light_t* lights;
	uint32_t* frame_buffer;
	float* depth_buffer;

	uint32_t screen_width;
	uint32_t screen_height;

	int light_count = 0;

	/// <summary>
	/// All shapes currently in the scene
	/// </summary>
	Arena* allShapes;

	/// <summary>
	/// Total number of shapes currently in the scene
	/// </summary>
	int shapeCount;

	/// <summary>
	/// Whether the engine is currently running
	/// </summary>
	bool isRunning;

	/// <summary>
	/// Time in seconds between the last frame and the current frame
	/// </summary>
	float deltaTime;

	/// <summary>
	/// Whether the engine should automatically update each frame. This is on by default
	/// </summary>
	bool autoUpdate = true;
	/// <summary>
	/// Camera instance for this engine
	/// </summary>
	Camera camera;

	/// <summary>
	/// Initialises the engine
	/// </summary>
	/// <param name="autoUpdate">Whether the engine should automatically update each frame</param>
	/// <param name="fps">Target frames per second</param>
	void init(bool autoUpdate, int fps);

	/// <summary>
	/// Updates all shapes and scene state for the current frame
	/// </summary>
	void update();

	/// <summary>
	/// Renders all shapes in an engine to the canvas
	/// </summary>
	void render();
	
	/// <summary>
	/// Adds a shape to the scene
	/// </summary>
	/// <param name="type">Type of shape to add</param>
	Shape* addShape(std::string filepath, point_t position, int width, int height);

	/// <summary>
	/// Removes a shape from the scene by index
	/// </summary>
	/// <param name="index">Index of the shape to remove</param>
	void removeShape(int index);

	/// <summary>
	/// Updates the position and angle of the camera object
	/// </summary>
	/// <param name="position">New position of the camera</param>
	/// <param name="angleX">New Angle of the camera on the X axis</param>
	/// <param name="angleY">New angle of the camera on the Y axis</param>
	/// <param name="angleZ">New angle of the camera on the Z axis</param>
	void updateCamera(point_t position, float angleX, float angleY, float angleZ);

	/// <summary>
	/// Creates a light object
	/// </summary>
	/// <param name="position"></param>
	/// <param name="intensity"></param>
	/// <param name="colour"></param>
	void create_light(point_t position, float intensity, uint8_t* colourRGB);
	
	/// <summary>
	/// Returns the frame_buffer for bytes in python code
	/// </summary>
	py::bytes get_framebuffer();

};



uint32_t convert_colour(uint8_t* colour);