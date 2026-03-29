
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
RAYTRACE THIS SHIT

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

#include "spatial.h"
#include <pybind11/stl.h>

PYBIND11_MODULE(crender, m) {
    m.doc() = "CRender - A lightweight 3D rendering library";

    py::class_<point_t>(m, "point_t", "A point in 3D space");

    m.def("create_point", &create_point, py::arg("x"), py::arg("y"), py::arg("z"),
        "Creates a point in 3D space");

    
    py::class_<Engine>(m, "Engine", "Main rendering engine instance")
        .def(py::init<>())
        .def("init", &Engine::init, "Initialises the engine with autoupdate and fps settings")
        .def("update", &Engine::update, "Updates and renders the current frame")
        .def("addShape", &Engine::addShape, "Loads and adds a shape from an OBJ file")
        .def("removeShape", &Engine::removeShape, "Removes a shape from the scene by index")
        .def("updateCamera", &Engine::updateCamera, "Updates the camera position and angles")
        .def("get_framebuffer", &Engine::get_framebuffer, "Returns the current framebuffer as bytes")
        .def("create_light", &Engine::create_light, "Creates a light point object")
        .def("load_material", &Engine::loadMTL, "Loads a a mtl file into Material Library");

    py::class_<Shape>(m, "Shape", "A 3D shape object")
        .def("moveShape", &Shape::moveShape, "Moves the shape by an offset in each axis")
        .def("rotateShape", &Shape::rotateShape, "Rotates the shape by angles in each axis")
        .def("scaleShape", &Shape::scaleShape, "Scales the shape uniformly");
}

#include "engine.h"
#include "spatial.h"
#include <stdio.h>
#include <math.h>
#include <opencv2/opencv.hpp>

int main() {
    Engine engine;
    engine.init(800, 600);

    point_t pos = create_point(0, 0, 0);
    engine.loadMTL("InteriorTest.mtl");
    engine.addShape("InteriorTest.obj", pos, 1, 1);

    point_t light_pos = create_point(0, 1, 3);
    engine.create_light(light_pos, 0.9f, { 255, 255, 255 });

    float cam_x = 0, cam_y = 1, cam_z = 3;
    float yaw = 0.45f;

    engine.start();
    for (int frame = 0; frame < 60; frame++) {
        cam_z += .1f;
        point_t cam_pos = create_point(cam_x, cam_y, cam_z);
        engine.updateCamera(cam_pos, yaw, 0, 0);
        auto t1 = std::chrono::high_resolution_clock::now();

        engine.update();

        auto t2 = std::chrono::high_resolution_clock::now();
        float ms = std::chrono::duration<float, std::milli>(t2 - t1).count();

        char fpsLabel[64];
        sprintf_s(fpsLabel, "Frame %d | %.1fms | %.0ffps", frame, ms, 1000.0f / ms);

        // wrap framebuffer in cv::Mat and display
        cv::Mat img(600, 800, CV_8UC4, engine.frame_buffer);
        cv::cvtColor(img, img, cv::COLOR_BGRA2BGR);

        // burn frame number and cam_x onto the image so we can see it changing
        char label[64];
        sprintf_s(label, "Frame %d | cam_x=%.2f", frame, cam_x);
        //cv::putText(img, label, { 10, 30 }, cv::FONT_HERSHEY_SIMPLEX, 0.8, { 0, 255, 0 }, 2);
        cv::putText(img, fpsLabel, { 10, 30 }, cv::FONT_HERSHEY_SIMPLEX, 0.8, { 0, 255, 0 }, 2);
        cv::imshow("CRender Debug", img);

        int key = cv::waitKey(1); // 100ms per frame so you can see each one
        if (key == 27) break; // ESC to exit early
    }

    cv::waitKey(0);
    cv::destroyAllWindows();
    return 0;
}