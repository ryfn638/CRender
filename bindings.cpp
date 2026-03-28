
#include "engine.h"

#include "pybind11/pybind11.h"
#include "engine.h"
// #include "opencv2/opencv.hpp"

/*
CRender, OpenGL alternative. I aim to make this a lot more streamlined and simpler than OpenGl.
Of course this comes with limited opportunities as a result, however that is the cost of streamlining the process.

heres a checklist of all the requirements I need to fully implement this


Not Done
Fragment pipeline - Decides the final color of each pixel
Texture Sampler - Reads color data from images and maps them onto geometry


Done:
Mathematics Library
Shape Creation and Operations
Memory Allocator (create a memory arena and manage the buffers cleanly), do this for each shape.
Camera - Defines the view into your scene via a view and projection matrix
Framebuffer - Output pixel array and ultimately what gets displayed
Depth Buffer - Tracks Z values per pixel so triangles draw on top correctly
Rasterizer converts triangles into actual pixels on the framebuffer
Vertex Pipeline - transforms 3D coordinates into 2D screen Coordinates
Window/Display output - blits youre framebuffer to an actual window on screen
Scene/buffer management - tracks all your geometry buffers so you know what to draw each frame
*/



namespace py = pybind11;

PYBIND11_MODULE(crender, m) {
	py::class_<Engine>(m, "Engine")
		.def(py::init<>())
		.def("init", &Engine::init)
		.def("addShape", &Engine::addShape)
		.def("removeShape", &Engine::removeShape)
		.def("updateCamera", &Engine::updateCamera)
		.def("get_framebuffer", &Engine::get_framebuffer);

	py::class_<Shape>(m, "Shape")
		.def("moveShape", &Shape::moveShape)
		.def("rotateShape", &Shape::rotateShape)
		.def("scaleShape", &Shape::scaleShape);

}