#include "render.cuh"
#include "spatial.h"
#include <cuda_runtime.h>
#include "engine.h"

// GPU side pointers — stored globally in this file
static gpu_vertex_t* d_vertices = nullptr;
static face_t* d_faces = nullptr;
static float4* d_clip_verts = nullptr;
static uint32_t* d_frame_buffer = nullptr;
static float* d_depth_buffer = nullptr;
static LightCache* d_lights = nullptr;
static int total_vertex_count = 0;
static int total_face_count = 0;
static gpu_material_t* d_materials = nullptr;
using namespace std;

__global__ void vertex_kernel(gpu_vertex_t* vertices, int vertex_count, mat4 mvp, float4* clip_verts) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= vertex_count) return;
    float x = vertices[i].px;
    float y = vertices[i].py;
    float z = vertices[i].pz;
    clip_verts[i].x = mvp.v[0] * x + mvp.v[1] * y + mvp.v[2] * z + mvp.v[3];
    clip_verts[i].y = mvp.v[4] * x + mvp.v[5] * y + mvp.v[6] * z + mvp.v[7];
    clip_verts[i].z = mvp.v[8] * x + mvp.v[9] * y + mvp.v[10] * z + mvp.v[11];
    clip_verts[i].w = mvp.v[12] * x + mvp.v[13] * y + mvp.v[14] * z + mvp.v[15];
}

#define TILE_SIZE 16

__global__ void tile_kernel(
    face_t* faces, int face_count,
    float4* clip_verts,
    gpu_vertex_t* vertices,
    uint32_t* frame_buffer,
    float* depth_buffer,
    int width, int height,
    LightCache* lights, int light_count,
    gpu_material_t* materials)
{
    // each block = one tile
    int tile_x = blockIdx.x * TILE_SIZE;
    int tile_y = blockIdx.y * TILE_SIZE;

    // each thread = one pixel within the tile
    int px = tile_x + threadIdx.x;
    int py = tile_y + threadIdx.y;

    // shared memory for depth within tile
    __shared__ float tile_depth[TILE_SIZE][TILE_SIZE];
    __shared__ int   tile_face[TILE_SIZE][TILE_SIZE];
    __shared__ float tile_w0[TILE_SIZE][TILE_SIZE];
    __shared__ float tile_w1[TILE_SIZE][TILE_SIZE];

    tile_depth[threadIdx.y][threadIdx.x] = FLT_MAX;
    tile_face[threadIdx.y][threadIdx.x] = -1;
    __syncthreads();

    // iterate faces — all threads in block do this together
    for (int f = 0; f < face_count; f++) {
        face_t face = faces[f];
        float4 ca = clip_verts[face.v0];
        float4 cb = clip_verts[face.v1];
        float4 cc = clip_verts[face.v2];

        if (ca.w <= 0 || cb.w <= 0 || cc.w <= 0) continue;

        float ax = (ca.x / ca.w + 1.0f) * 0.5f * width;
        float ay = (1.0f - ca.y / ca.w) * 0.5f * height;
        float az = ca.z / ca.w;
        float bx = (cb.x / cb.w + 1.0f) * 0.5f * width;
        float by = (1.0f - cb.y / cb.w) * 0.5f * height;
        float bz = cb.z / cb.w;
        float cx = (cc.x / cc.w + 1.0f) * 0.5f * width;
        float cy = (1.0f - cc.y / cc.w) * 0.5f * height;
        float cz = cc.z / cc.w;

        // quick tile overlap test — skip if face doesn't touch this tile
        if (fmaxf(fmaxf(ax, bx), cx) < tile_x) continue;
        if (fminf(fminf(ax, bx), cx) > tile_x + TILE_SIZE) continue;
        if (fmaxf(fmaxf(ay, by), cy) < tile_y) continue;
        if (fminf(fminf(ay, by), cy) > tile_y + TILE_SIZE) continue;

        // each thread tests its own pixel
        if (px < width && py < height) {
            float denom = (by - cy) * (ax - cx) + (cx - bx) * (ay - cy);
            if (denom == 0) continue;

            float w0 = ((by - cy) * ((float)px - cx) + (cx - bx) * ((float)py - cy)) / denom;
            float w1 = ((cy - ay) * ((float)px - cx) + (ax - cx) * ((float)py - cy)) / denom;
            float w2 = 1.0f - w0 - w1;

            if (w0 >= 0 && w1 >= 0 && w2 >= 0) {
                float depth = w0 * az + w1 * bz + w2 * cz;
                if (depth < tile_depth[threadIdx.y][threadIdx.x]) {
                    tile_depth[threadIdx.y][threadIdx.x] = depth;
                    tile_face[threadIdx.y][threadIdx.x] = f;
                    tile_w0[threadIdx.y][threadIdx.x] = w0;
                    tile_w1[threadIdx.y][threadIdx.x] = w1;
                }
            }
        }
    }

    __syncthreads();

    // shade pass — each thread shades its own pixel
    if (px >= width || py >= height) return;
    int best_f = tile_face[threadIdx.y][threadIdx.x];
    if (best_f < 0) return;

    float w0 = tile_w0[threadIdx.y][threadIdx.x];
    float w1 = tile_w1[threadIdx.y][threadIdx.x];
    float w2 = 1.0f - w0 - w1;

    face_t face = faces[best_f];
    gpu_vertex_t* vA = &vertices[face.v0];
    gpu_vertex_t* vB = &vertices[face.v1];
    gpu_vertex_t* vC = &vertices[face.v2];

    float nx = w0 * vA->nx + w1 * vB->nx + w2 * vC->nx;
    float ny = w0 * vA->ny + w1 * vB->ny + w2 * vC->ny;
    float nz = w0 * vA->nz + w1 * vB->nz + w2 * vC->nz;
    float inv_len = rsqrtf(nx * nx + ny * ny + nz * nz);
    nx *= inv_len; ny *= inv_len; nz *= inv_len;

    float total = 0.05f;
    for (int l = 0; l < light_count; l++) {
        float diff = fmaxf(nx * lights[l].lx + ny * lights[l].ly + nz * lights[l].lz, 0.0f);
        total += diff * lights[l].intensity;
    }
    total = fminf(total, 1.0f);
    uint8_t shade = (uint8_t)(total * 255);

    uint8_t mr = 255, mg = 255, mb = 255;
    if (face.materialIndex >= 0) {
        mr = materials[face.materialIndex].r;
        mg = materials[face.materialIndex].g;
        mb = materials[face.materialIndex].b;
    }

    int idx = py * width + px;
    frame_buffer[idx] = (0xFF << 24) |
        ((uint8_t)(mr * shade / 255) << 16) |
        ((uint8_t)(mg * shade / 255) << 8) |
        ((uint8_t)(mb * shade / 255));
}

void gpu_init(Engine* engine) {
    total_vertex_count = 0;
    total_face_count = 0;
    for (int i = 0; i < engine->shapeCount; i++) {
        Shape* s = (Shape*)engine->allShapes[i].base;
        total_vertex_count += s->point_count;
        total_face_count += s->face_count;
    }

    // allocate GPU buffers
    cudaMalloc(&d_vertices, total_vertex_count * sizeof(gpu_vertex_t));
    cudaMalloc(&d_faces, total_face_count * sizeof(face_t));
    cudaMalloc(&d_clip_verts, total_vertex_count * sizeof(float4));
    cudaMalloc(&d_frame_buffer, engine->screen_width * engine->screen_height * sizeof(uint32_t));
    cudaMalloc(&d_depth_buffer, engine->screen_width * engine->screen_height * sizeof(float));
    cudaMalloc(&d_lights, engine->light_count * sizeof(LightCache));

    // copy static geometry to GPU once
    int vert_offset = 0, face_offset = 0;
    for (int i = 0; i < engine->shapeCount; i++) {
        Shape* s = (Shape*)engine->allShapes[i].base;

        // print first vertex to verify data
        if (s->point_count > 0) {
                s->vertices[0].position.matrix.values[0],
                s->vertices[0].position.matrix.values[1],
                s->vertices[0].position.matrix.values[2];
        }

        // print first face to verify indices
        if (s->face_count > 0) {
            printf("  first face: v0=%d v1=%d v2=%d\n",
                s->faces[0].v0, s->faces[0].v1, s->faces[0].v2);
        }
        // offset the location in memory for all shapes
        face_t* offset_faces = new face_t[s->face_count];
        for (int f = 0; f < s->face_count; f++) {
            offset_faces[f] = {
                s->faces[f].v0 + vert_offset,
                s->faces[f].v1 + vert_offset,
                s->faces[f].v2 + vert_offset,
                s->faces[f].materialIndex
            };
        }
        cudaMemcpy(d_faces + face_offset, offset_faces, s->face_count * sizeof(face_t), cudaMemcpyHostToDevice);
        delete[] offset_faces;

        gpu_vertex_t* offset_vertices = new gpu_vertex_t[s->point_count];
        for (int f = 0; f < s->point_count; f++) {
            offset_vertices[f] = {
                s->vertices[f].position.matrix.values[0],
                s->vertices[f].position.matrix.values[1],
                s->vertices[f].position.matrix.values[2],
                s->vertices[f].normal.matrix.values[0],
                s->vertices[f].normal.matrix.values[1],
                s->vertices[f].normal.matrix.values[2]

            };
        }
        cudaMemcpy(d_vertices + vert_offset, offset_vertices, s->point_count * sizeof(gpu_vertex_t), cudaMemcpyHostToDevice);
        delete[] offset_vertices;

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

// fill entire screen red first
__global__ void fill_kernel(uint32_t* fb, int size) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= size) return;
    fb[i] = 0xFF0000FF;
}



void gpu_render(Engine* engine) {
    int deviceCount = 0;
    cudaGetDeviceCount(&deviceCount);
    printf("CUDA devices: %d\n", deviceCount);
    cudaSetDevice(0);
    int w = engine->screen_width;
    int h = engine->screen_height;

    cudaMemset(d_frame_buffer, 0, w * h * sizeof(uint32_t));
    cudaMemset(d_depth_buffer, 0x7f, w * h * sizeof(float));
    int mat_count = engine->lib ? engine->lib->count : 0;
    cudaMalloc(&d_materials, mat_count * sizeof(gpu_material_t));

    if (mat_count > 0) {
        gpu_material_t* host_mats = new gpu_material_t[mat_count];
        for (int i = 0; i < mat_count; i++) {
            uint32_t diff = engine->lib->materials[i].diffuse;
            host_mats[i] = {
                (uint8_t)((diff >> 16) & 0xFF),
                (uint8_t)((diff >> 8) & 0xFF),
                (uint8_t)((diff) & 0xFF)
            };
        }
        cudaMemcpy(d_materials, host_mats, mat_count * sizeof(gpu_material_t), cudaMemcpyHostToDevice);
        delete[] host_mats;
    }


    int threads = 512;
    cudaEvent_t start, stop;
    cudaEventCreate(&start);
    cudaEventCreate(&stop);
    float ms = 0;

    // vertex
    int vert_offset = 0;
    cudaEventRecord(start);
    for (int i = 0; i < engine->shapeCount; i++) {
        Shape* s = (Shape*)engine->allShapes[i].base;
        mat4 mvp = build_mvp(s, &engine->camera,
            90.0f * (3.14159265f / 180.0f),
            (float)w / (float)h, 0.1f, 1000.0f);
        int vblocks = (s->point_count + 255) / 256;
        vertex_kernel << <vblocks, 256 >> > (d_vertices + vert_offset, s->point_count, mvp, d_clip_verts + vert_offset);
        vert_offset += s->point_count;
    }
    cudaEventRecord(stop);
    cudaEventSynchronize(stop);  // wait for stop event
    cudaEventElapsedTime(&ms, start, stop);
    printf("vertex: %.2fms\n", ms);

    // tile
    cudaEventRecord(start);
    dim3 tile_threads(TILE_SIZE, TILE_SIZE);
    dim3 tile_blocks((w + TILE_SIZE - 1) / TILE_SIZE, (h + TILE_SIZE - 1) / TILE_SIZE);
    tile_kernel << <tile_blocks, tile_threads >> > (
        d_faces, total_face_count,
        d_clip_verts, d_vertices,
        d_frame_buffer, d_depth_buffer,
        w, h, d_lights, engine->light_count, d_materials);
    cudaEventRecord(stop);
    cudaEventSynchronize(stop);  // wait for stop event
    cudaEventElapsedTime(&ms, start, stop);
    printf("tile: %.2fms\n", ms);

    // memcpy
    cudaEventRecord(start);
    cudaDeviceSynchronize();
    cudaMemcpy(engine->frame_buffer, d_frame_buffer, w * h * sizeof(uint32_t), cudaMemcpyDeviceToHost);
    cudaEventRecord(stop);
    cudaEventSynchronize(stop);  // wait for stop event
    cudaEventElapsedTime(&ms, start, stop);
    printf("memcpy: %.2fms\n", ms);
}

void gpu_free(Engine* engine) {
    cudaFree(d_vertices);
    cudaFree(d_faces);
    cudaFree(d_clip_verts);
    cudaFree(d_frame_buffer);
    cudaFree(d_depth_buffer);
    cudaFree(d_lights);
    cudaFree(d_materials);
}