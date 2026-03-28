#include <stdint.h>
#include <memory>
#include "window.h"
#include "math.h"
#include "spatial.h"
#include "engine.h"
#include <iostream>
using namespace std;
/*
Frame buffer are the frames that are being loaded into the buffer, frames to be loaded

depth buffer is essentially the pixel that is closest to the camera, aka in frame. closest pixel wins
*/

uint32_t r_mask = 0x000000FF; // Red mask
uint32_t b_mask = 0x0000FF00; // Blue mask
uint32_t g_mask = 0x00FF0000; // Green mask
uint32_t o_mask = 0xFF000000; // Opacity mask, im not gna worry about this for now




point_t vertex_to_screen(vertex_t* vertex,
    Shape* shape,
    Camera* camera,
    float fov,
    float aspect,
    float near_plane,
    float far_plane,
    int width,
    int height) {
    matrix_t clip = transform_vertex(vertex, shape, camera, fov, aspect, near_plane, far_plane);
    float w = clip.values[3];
    int screen_x = (int)((clip.values[0] / w + 1.0f) * 0.5f * width);
    int screen_y = (int)((1.0f - clip.values[1] / w) * 0.5f * height);
    float depth = clip.values[2] / w;
    return create_point(screen_x, screen_y, depth);
}


// The idea is we rasterize a point and then immediately write it to our mask and then we dont need it anymore.
void rasterize_face(Engine* engine, face_t face, Shape* shape, Camera* camera, float fov, float aspect, float near_plane, float far_plane, int width, int height) {
    if (shape->vertices == nullptr) return;

    vertex_t* vA = &shape->vertices[face.v0];
    vertex_t* vB = &shape->vertices[face.v1];
    vertex_t* vC = &shape->vertices[face.v2];

    // transform vertices to screen space
    point_t screenA = vertex_to_screen(vA, shape, camera, fov, aspect, near_plane, far_plane, width, height);
    point_t screenB = vertex_to_screen(vB, shape, camera, fov, aspect, near_plane, far_plane, width, height);
    point_t screenC = vertex_to_screen(vC, shape, camera, fov, aspect, near_plane, far_plane, width, height);

    float ax = screenA.matrix.values[0], ay = screenA.matrix.values[1], az = screenA.matrix.values[2];
    float bx = screenB.matrix.values[0], by = screenB.matrix.values[1], bz = screenB.matrix.values[2];
    float cx = screenC.matrix.values[0], cy = screenC.matrix.values[1], cz = screenC.matrix.values[2];

    // bounding box
    int minX = max((int)min({ ax, bx, cx }), 0);
    int maxX = min((int)max({ ax, bx, cx }), width - 1);
    int minY = max((int)min({ ay, by, cy }), 0);
    int maxY = min((int)max({ ay, by, cy }), height - 1);

    float denom = (by - cy) * (ax - cx) + (cx - bx) * (ay - cy);
    if (denom == 0) return;

    for (int x = minX; x <= maxX; x++) {
        for (int y = minY; y <= maxY; y++) {
            float w0 = ((by - cy) * ((float)x - cx) + (cx - bx) * ((float)y - cy)) / denom;
            float w1 = ((cy - ay) * ((float)x - cx) + (ax - cx) * ((float)y - cy)) / denom;
            float w2 = 1.0f - w0 - w1;

            if (w0 >= 0 && w1 >= 0 && w2 >= 0) {
                float depth = w0 * az + w1 * bz + w2 * cz;

                if (depth < engine->depth_buffer[y * width + x]) {
                    engine->depth_buffer[y * width + x] = depth;

                    // interpolate normal
                    float nx = w0 * vA->normal.matrix.values[0] + w1 * vB->normal.matrix.values[0] + w2 * vC->normal.matrix.values[0];
                    float ny = w0 * vA->normal.matrix.values[1] + w1 * vB->normal.matrix.values[1] + w2 * vC->normal.matrix.values[1];
                    float nz = w0 * vA->normal.matrix.values[2] + w1 * vB->normal.matrix.values[2] + w2 * vC->normal.matrix.values[2];

                    // normalize
                    float len = sqrt(nx * nx + ny * ny + nz * nz);
                    if (len > 0) { nx /= len; ny /= len; nz /= len; }

                    // accumulate light from all lights
                    float total = 0.05f; // ambient
                    for (int l = 0; l < engine->light_count; l++) {
                        light_t* light = &engine->lights[l];

                        float lx = light->position.matrix.values[0];
                        float ly = light->position.matrix.values[1];
                        float lz = light->position.matrix.values[2];

                        // normalize light direction
                        float llen = sqrt(lx * lx + ly * ly + lz * lz);
                        if (llen > 0) { lx /= llen; ly /= llen; lz /= llen; }

                        float diffuse = max(nx * lx + ny * ly + nz * lz, 0.0f);
                        total += diffuse * light->intensity;
                    }

                    total = min(total, 1.0f);
                    uint8_t shade = (uint8_t)(total * 255);
                    engine->frame_buffer[y * width + x] = (0xFF << 24) | (shade << 16) | (shade << 8) | shade;
                }
            }
        }
    }
}

/// <summary>
/// Rasterized vertex structure
/// </summary>
typedef struct {
    int x, y;
    float depth;
    float u, v;
} raster_vertex_t;



