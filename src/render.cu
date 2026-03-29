#include "render.cuh"
#include "spatial.h"
#include <cuda_runtime.h>
#include "engine.h"
// GPU side pointers — stored globally in this file
static vertex_t* d_vertices = nullptr;
static face_t* d_faces = nullptr;
static float4* d_clip_verts = nullptr;
static uint32_t* d_frame_buffer = nullptr;
static float* d_depth_buffer = nullptr;
static LightCache* d_lights = nullptr;
static int total_vertex_count = 0;
static int total_face_count = 0;
using namespace std;


void gpu_init(Engine* engine) {
    // count total verts and faces across all shapes
    total_vertex_count = 0;
    total_face_count = 0;
    for (int i = 0; i < engine->shapeCount; i++) {
        Shape* s = (Shape*)engine->allShapes[i].base;
        total_vertex_count += s->point_count;
        total_face_count += s->face_count;
    }

    // allocate GPU buffers
    cudaMalloc(&d_vertices, total_vertex_count * sizeof(vertex_t));
    cudaMalloc(&d_faces, total_face_count * sizeof(face_t));
    cudaMalloc(&d_clip_verts, total_vertex_count * sizeof(float4));
    cudaMalloc(&d_frame_buffer, engine->screen_width * engine->screen_height * sizeof(uint32_t));
    cudaMalloc(&d_depth_buffer, engine->screen_width * engine->screen_height * sizeof(float));
    cudaMalloc(&d_lights, engine->light_count * sizeof(LightCache));

    // copy static geometry to GPU once
    int vert_offset = 0, face_offset = 0;
    for (int i = 0; i < engine->shapeCount; i++) {
        Shape* s = (Shape*)engine->allShapes[i].base;
        cudaMemcpy(d_vertices + vert_offset, s->vertices, s->point_count * sizeof(vertex_t), cudaMemcpyHostToDevice);
        cudaMemcpy(d_faces + face_offset, s->faces, s->face_count * sizeof(face_t), cudaMemcpyHostToDevice);
        vert_offset += s->point_count;
        face_offset += s->face_count;
    }

    // copy lights
    LightCache* host_lights = new LightCache[engine->light_count];
    for (int l = 0; l < engine->light_count; l++) {
        float lx = engine->lights[l].position.matrix.values[0];
        float ly = engine->lights[l].position.matrix.values[1];
        float lz = engine->lights[l].position.matrix.values[2];
        float llen = sqrtf(lx * lx + ly * ly + lz * lz);
        host_lights[l] = { lx / llen, ly / llen, lz / llen, engine->lights[l].intensity };
    }
    cudaMemcpy(d_lights, host_lights, engine->light_count * sizeof(LightCache), cudaMemcpyHostToDevice);
    delete[] host_lights;
}

__global__ void vertex_kernel(vertex_t* vertices, int vertex_count, mat4 mvp, float4* clip_verts) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= vertex_count) return;
    float x = vertices[i].position.matrix.values[0];
    float y = vertices[i].position.matrix.values[1];
    float z = vertices[i].position.matrix.values[2];
    clip_verts[i].x = mvp.v[0] * x + mvp.v[1] * y + mvp.v[2] * z + mvp.v[3];
    clip_verts[i].y = mvp.v[4] * x + mvp.v[5] * y + mvp.v[6] * z + mvp.v[7];
    clip_verts[i].z = mvp.v[8] * x + mvp.v[9] * y + mvp.v[10] * z + mvp.v[11];
    clip_verts[i].w = mvp.v[12] * x + mvp.v[13] * y + mvp.v[14] * z + mvp.v[15];
}

__global__ void raster_kernel(
    face_t* faces, int face_count,
    float4* clip_verts,
    vertex_t* vertices,
    uint32_t* frame_buffer,
    float* depth_buffer,
    int width, int height,
    LightCache* lights, int light_count, Engine* engine)
{
    int f = blockIdx.x * blockDim.x + threadIdx.x;
    if (f >= face_count) return;

    face_t face = faces[f];
    float4 ca = clip_verts[face.v0];
    float4 cb = clip_verts[face.v1];
    float4 cc = clip_verts[face.v2];

    if (ca.w <= 0 || cb.w <= 0 || cc.w <= 0) return;

    float ax = (ca.x / ca.w + 1.0f) * 0.5f * width;
    float ay = (1.0f - ca.y / ca.w) * 0.5f * height;
    float az = ca.z / ca.w;
    float bx = (cb.x / cb.w + 1.0f) * 0.5f * width;
    float by = (1.0f - cb.y / cb.w) * 0.5f * height;
    float bz = cb.z / cb.w;
    float cx = (cc.x / cc.w + 1.0f) * 0.5f * width;
    float cy = (1.0f - cc.y / cc.w) * 0.5f * height;
    float cz = cc.z / cc.w;

    int minX = max((int)fminf(fminf(ax, bx), cx), 0);
    int maxX = min((int)fmaxf(fmaxf(ax, bx), cx), width - 1);
    int minY = max((int)fminf(fminf(ay, by), cy), 0);
    int maxY = min((int)fmaxf(fmaxf(ay, by), cy), height - 1);
    if (minX > maxX || minY > maxY) return;

    float denom = (by - cy) * (ax - cx) + (cx - bx) * (ay - cy);
    if (denom == 0) return;

    float w0_row = ((by - cy) * (minX - cx) + (cx - bx) * (minY - cy)) / denom;
    float w1_row = ((cy - ay) * (minX - cx) + (ax - cx) * (minY - cy)) / denom;
    float w0_dx = (by - cy) / denom, w1_dx = (cy - ay) / denom;
    float w0_dy = (cx - bx) / denom, w1_dy = (ax - cx) / denom;

    vertex_t* vA = &vertices[face.v0];
    vertex_t* vB = &vertices[face.v1];
    vertex_t* vC = &vertices[face.v2];

    for (int y = minY; y <= maxY; y++) {
        float w0 = w0_row, w1 = w1_row;
        for (int x = minX; x <= maxX; x++) {
            float w2 = 1.0f - w0 - w1;
            if (w0 >= 0 && w1 >= 0 && w2 >= 0) {
                float depth = w0 * az + w1 * bz + w2 * cz;
                int idx = y * width + x;
                atomicMin((int*)&depth_buffer[idx], __float_as_int(depth));
                if (__int_as_float(atomicAdd((int*)&depth_buffer[idx], 0)) == __float_as_int(depth)) {
                    float nx = w0 * vA->normal.matrix.values[0] + w1 * vB->normal.matrix.values[0] + w2 * vC->normal.matrix.values[0];
                    float ny = w0 * vA->normal.matrix.values[1] + w1 * vB->normal.matrix.values[1] + w2 * vC->normal.matrix.values[1];
                    float nz = w0 * vA->normal.matrix.values[2] + w1 * vB->normal.matrix.values[2] + w2 * vC->normal.matrix.values[2];
                    float inv_len = rsqrtf(nx * nx + ny * ny + nz * nz);
                    nx *= inv_len; ny *= inv_len; nz *= inv_len;
                    float total = 0.05f;
                    for (int l = 0; l < light_count; l++) {
                        float diff = fmaxf(nx * lights[l].lx + ny * lights[l].ly + nz * lights[l].lz, 0.0f);
                        total += diff * lights[l].intensity;
                    }
                    total = fminf(total, 1.0f);
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
            w0 += w0_dx; w1 += w1_dx;
        }
        w0_row += w0_dy; w1_row += w1_dy;
    }
}

void gpu_render(Engine* engine) {
    int w = engine->screen_width;
    int h = engine->screen_height;

    cudaMemset(d_frame_buffer, 0, w * h * sizeof(uint32_t));
    cudaMemset(d_depth_buffer, 0x7f, w * h * sizeof(float));

    int threads = 256;

    // compute MVP per shape and run vertex kernel
    int vert_offset = 0, face_offset = 0;
    for (int i = 0; i < engine->shapeCount; i++) {
        Shape* s = (Shape*)engine->allShapes[i].base;

        mat4 mvp = build_mvp(s, &engine->camera,
            90.0f * (3.14159265f / 180.0f),
            (float)w / (float)h,
            0.1f, 1000.0f);

        int vblocks = (s->point_count + threads - 1) / threads;
        vertex_kernel <<< vblocks, threads >>> (
            d_vertices + vert_offset,
            s->point_count, mvp,
            d_clip_verts + vert_offset);

        int fblocks = (s->face_count + threads - 1) / threads;
        raster_kernel <<<fblocks, threads >>> (
            d_faces + face_offset,
            s->face_count,
            d_clip_verts + vert_offset,
            d_vertices + vert_offset,
            d_frame_buffer, d_depth_buffer,
            w, h,
            d_lights, engine->light_count,
            engine);

        vert_offset += s->point_count;
        face_offset += s->face_count;
    }

    // sync and copy back
    cudaDeviceSynchronize();
    cudaMemcpy(engine->frame_buffer, d_frame_buffer, w * h * sizeof(uint32_t), cudaMemcpyDeviceToHost);
}

void gpu_free(Engine* engine) {
    cudaFree(d_vertices);
    cudaFree(d_faces);
    cudaFree(d_clip_verts);
    cudaFree(d_frame_buffer);
    cudaFree(d_depth_buffer);
    cudaFree(d_lights);
}