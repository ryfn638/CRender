#include <stdint.h>
#include <memory>
#include "math.h"
#include "spatial.h"
#include "engine.h"
#include <iostream>
#include "material.h"
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
void rasterize_face(Engine* engine, face_t face, Shape* shape, const mat4& mvp, int width, int height) {
    if (shape->vertices == nullptr) return;

    vertex_t* vA = &shape->vertices[face.v0];
    vertex_t* vB = &shape->vertices[face.v1];
    vertex_t* vC = &shape->vertices[face.v2];

    float ax, ay, az, bx, by, bz, cx, cy, cz;
    float aw, bw, cw;

    // replace clip_w with this — just skip the perspective divide if w <= 0
    auto clip_w = [&](vertex_t* v, float& ox, float& oy, float& oz, float& ow) {
        float x = v->position.matrix.values[0];
        float y = v->position.matrix.values[1];
        float z = v->position.matrix.values[2];
        float ccx = mvp.v[0] * x + mvp.v[1] * y + mvp.v[2] * z + mvp.v[3];
        float ccy = mvp.v[4] * x + mvp.v[5] * y + mvp.v[6] * z + mvp.v[7];
        float ccz = mvp.v[8] * x + mvp.v[9] * y + mvp.v[10] * z + mvp.v[11];
        float ccw = mvp.v[12] * x + mvp.v[13] * y + mvp.v[14] * z + mvp.v[15];
        ow = ccw;
        if (ccw <= 0) {
            // clamp to edge of screen instead of projecting garbage
            ox = (ccx > 0) ? width : 0;
            oy = (ccy > 0) ? 0 : height;
            oz = 1.0f;
            return;
        }
        float inv_w = 1.0f / ccw;
        ox = (ccx * inv_w + 1.0f) * 0.5f * width;
        oy = (1.0f - ccy * inv_w) * 0.5f * height;
        oz = ccz * inv_w;
        };

    // then just remove the near plane discard entirely
    // if (aw <= 0 || bw <= 0 || cw <= 0) return;  <-- delete this

    clip_w(vA, ax, ay, az, aw);
    clip_w(vB, bx, by, bz, bw);
    clip_w(vC, cx, cy, cz, cw);

    // compute face normal in world space
    float fnx = (vB->normal.matrix.values[0] + vA->normal.matrix.values[0]) * 0.5f;
    float fny = (vB->normal.matrix.values[1] + vA->normal.matrix.values[1]) * 0.5f;
    float fnz = (vB->normal.matrix.values[2] + vA->normal.matrix.values[2]) * 0.5f;

    // vector from face to camera
    float dcx = engine->camera.position.matrix.values[0] - vA->position.matrix.values[0];
    float dcy = engine->camera.position.matrix.values[0] - vA->position.matrix.values[1];
    float dcz = engine->camera.position.matrix.values[2] - vA->position.matrix.values[2];

    // if normal points away from camera, skip
    if (fnx * dcx + fny * dcy + fnz * dcz < 0) return;
    // bounding box
    int minX = max((int)min({ ax, bx, cx }), 0);
    int maxX = min((int)max({ ax, bx, cx }), width - 1);
    int minY = max((int)min({ ay, by, cy }), 0);
    int maxY = min((int)max({ ay, by, cy }), height - 1);

    float denom = (by - cy) * (ax - cx) + (cx - bx) * (ay - cy);
    if (denom == 0) return;


    struct LightCache { float lx, ly, lz, intensity; } lights_cache[8];
    for (int l = 0; l < engine->light_count; l++) {
        float lx = engine->lights[l].position.matrix.values[0];
        float ly = engine->lights[l].position.matrix.values[1];
        float lz = engine->lights[l].position.matrix.values[2];
        float llen = sqrt(lx * lx + ly * ly + lz * lz);
        lights_cache[l] = {
            lx / llen,
            ly / llen,
            lz / llen,
            engine->lights[l].intensity
        };
    }

    float w0_row = ((by - cy) * (minX - cx) + (cx - bx) * (minY - cy)) / denom;
    float w1_row = ((cy - ay) * (minX - cx) + (ax - cx) * (minY - cy)) / denom;

    float w0_dx = (by - cy) / denom;
    float w1_dx = (cy - ay) / denom;

    float w0_dy = (cx - bx) / denom;
    float w1_dy = (ax - cx) / denom;

    // w2 = 1 - w0 - w1, so no need to track separately

    for (int y = minY; y <= maxY; y++) {
        float w0 = w0_row;
        float w1 = w1_row;

        for (int x = minX; x <= maxX; x++) {
            float w2 = 1.0f - w0 - w1;

            if (w0 >= 0 && w1 >= 0 && w2 >= 0) {
                float depth = w0 * az + w1 * bz + w2 * cz;

                if (depth < engine->depth_buffer[y * width + x]) {
                    engine->depth_buffer[y * width + x] = depth;

                    float nx = w0 * vA->normal.matrix.values[0] + w1 * vB->normal.matrix.values[0] + w2 * vC->normal.matrix.values[0];
                    float ny = w0 * vA->normal.matrix.values[1] + w1 * vB->normal.matrix.values[1] + w2 * vC->normal.matrix.values[1];
                    float nz = w0 * vA->normal.matrix.values[2] + w1 * vB->normal.matrix.values[2] + w2 * vC->normal.matrix.values[2];

                    float inv_len = 1.0f / sqrt(nx * nx + ny * ny + nz * nz);
                    nx *= inv_len; ny *= inv_len; nz *= inv_len;

                    float total = 0.05f;
                    for (int l = 0; l < engine->light_count; l++) {
                        float diffuse = max(nx * lights_cache[l].lx +
                            ny * lights_cache[l].ly +
                            nz * lights_cache[l].lz, 0.0f);
                        total += diffuse * lights_cache[l].intensity;
                    }

                    total = min(total, 1.0f);
                    uint8_t shade = (uint8_t)(total * 255);

                    if (engine->lib != nullptr && face.materialIndex >= 0) {
                        Material& mat = engine->lib->materials[face.materialIndex];
                        uint8_t r = (uint8_t)(((mat.diffuse >> 16) & 0xFF) * shade / 255);
                        uint8_t g = (uint8_t)(((mat.diffuse >> 8) & 0xFF) * shade / 255);
                        uint8_t b = (uint8_t)(((mat.diffuse) & 0xFF) * shade / 255);
                        engine->frame_buffer[y * width + x] = (0xFF << 24) | (r << 16) | (g << 8) | b;
                    }
                    else {
                        engine->frame_buffer[y * width + x] = (0xFF << 24) | (shade << 16) | (shade << 8) | shade;
                    }
                }
            }
            w0 += w0_dx;
            w1 += w1_dx;
        }
        w0_row += w0_dy;
        w1_row += w1_dy;
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



