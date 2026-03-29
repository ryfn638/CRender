// render.cuh
#pragma once

// forward declare instead of including engine.h
class Engine;

struct LightCache { float lx, ly, lz, intensity; };

struct gpu_vertex_t {
    float px, py, pz;
    float nx, ny, nz;
};

void gpu_init(Engine* engine);
void gpu_render(Engine* engine);
void gpu_free(Engine* engine);