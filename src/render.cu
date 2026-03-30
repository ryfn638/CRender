#include "render.cuh"
#include "spatial.h"
#include <cuda_runtime.h>
#include "engine.h"

static gpu_vertex_t* d_vertices = nullptr;
static face_t* d_faces = nullptr;
static float4* d_clip_verts = nullptr;
static uint32_t* d_frame_buffer = nullptr;
static float* d_depth_buffer = nullptr;
static LightCache* d_lights = nullptr;
static gpu_material_t* d_materials = nullptr;  // moved here — allocated once in gpu_init
static int total_vertex_count = 0;
static int total_face_count = 0;

#define MAX_FACES_PER_TILE 1024  // tune for your scene
struct BinEntry {
    int    face_idx;        // original face for material/normal lookup
    float4 ca, cb, cc;     // clipped clip-space verts (may differ from original)
};


struct TileBins {
    int      count;
    BinEntry entries[MAX_FACES_PER_TILE];
};
// add to globals at top of file
static TileBins* d_bins = nullptr;
static int cached_tiles_x = 0;
static int cached_tiles_y = 0;
using namespace std;


// helper inline — call before binning
#define NEAR_W 0.1f

__device__ float4 intersect_near(float4 a, float4 b) {
    float t = (a.w - NEAR_W) / (a.w - b.w);
    return {
        a.x + t * (b.x - a.x),
        a.y + t * (b.y - a.y),
        a.z + t * (b.z - a.z),
        NEAR_W
    };
}

__device__ int clip_triangle(float4 a, float4 b, float4 c, float4 out[2][3]) {
    bool ia = a.w > NEAR_W;
    bool ib = b.w > NEAR_W;
    bool ic = c.w > NEAR_W;
    int inside = (int)ia + (int)ib + (int)ic;

    if (inside == 0) return 0;

    if (inside == 3) {
        out[0][0] = a; out[0][1] = b; out[0][2] = c;
        return 1;
    }

    if (inside == 1) {
        // rotate so the single inside vertex is always 'a'
        if (ib) { float4 t = a; a = b; b = c; c = t; }
        else if (ic) { float4 t = a; a = c; c = b; b = t; }
        // now a is inside, b and c are outside
        out[0][0] = a;
        out[0][1] = intersect_near(a, b);
        out[0][2] = intersect_near(a, c);
        return 1;
    }

    // inside == 2 — rotate so the single outside vertex is always 'c'
    if (!ia) { float4 t = c; c = a; a = b; b = t; }  // a is out → rotate to c
    else if (!ib) { float4 t = c; c = b; b = a; a = t; }  // b is out → rotate to c
    // now c is outside, a and b are inside
    float4 ac = intersect_near(c, a);
    float4 bc = intersect_near(c, b);
    // split into 2 triangles
    out[0][0] = a;  out[0][1] = b;  out[0][2] = ac;
    out[1][0] = b;  out[1][1] = bc; out[1][2] = ac;
    return 2;
}

__global__ void vertex_kernel(gpu_vertex_t* vertices, int vertex_count,
    mat4 mvp, float4* clip_verts)
{
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

#define TILE_SIZE 16  // single definition — removed the duplicate #ifndef block

// ── Pass 1: Binning kernel ───────────────────────────────────────────────
// One thread per face. Tests the face AABB against every tile and writes
// the face index into that tile's bin. Runs in ~microseconds.


__global__ void bin_kernel(
    const face_t* __restrict__ faces, int face_count,
    const float4* __restrict__ clip_verts,
    TileBins* __restrict__ bins,
    int width, int height,
    int tiles_x, int tiles_y)
{
    int f = blockIdx.x * blockDim.x + threadIdx.x;
    if (f >= face_count) return;

    const face_t face = faces[f];
    const float4 ca = __ldg(&clip_verts[face.v0]);
    const float4 cb = __ldg(&clip_verts[face.v1]);
    const float4 cc = __ldg(&clip_verts[face.v2]);

  

    float4 clipped[2][3];
    int ntris = clip_triangle(ca, cb, cc, clipped);
    if (ntris == 0) return;

    for (int t = 0; t < ntris; t++) {
        float4 pca = clipped[t][0];
        float4 pcb = clipped[t][1];
        float4 pcc = clipped[t][2];

        const float rwa = 1.f / pca.w, rwb = 1.f / pcb.w, rwc = 1.f / pcc.w;
        const float ax = (pca.x * rwa + 1.f) * (.5f * width);
        const float ay = (1.f - pca.y * rwa) * (.5f * height);
        const float bx = (pcb.x * rwb + 1.f) * (.5f * width);
        const float by = (1.f - pcb.y * rwb) * (.5f * height);
        const float cx = (pcc.x * rwc + 1.f) * (.5f * width);
        const float cy = (1.f - pcc.y * rwc) * (.5f * height);


        int tx0 = max(0, (int)floorf(fminf(fminf(ax, bx), cx)) / TILE_SIZE);
        int tx1 = min(tiles_x - 1, (int)floorf(fmaxf(fmaxf(ax, bx), cx)) / TILE_SIZE);
        int ty0 = max(0, (int)floorf(fminf(fminf(ay, by), cy)) / TILE_SIZE);
        int ty1 = min(tiles_y - 1, (int)floorf(fmaxf(fmaxf(ay, by), cy)) / TILE_SIZE);

        for (int ty = ty0; ty <= ty1; ty++) {
            for (int tx = tx0; tx <= tx1; tx++) {
                TileBins* bin = &bins[ty * tiles_x + tx];
                int slot = atomicAdd(&bin->count, 1);
                if (slot < MAX_FACES_PER_TILE) {
                    bin->entries[slot].face_idx = f;
                    bin->entries[slot].ca = pca;  // clipped verts, not original
                    bin->entries[slot].cb = pcb;
                    bin->entries[slot].cc = pcc;
                }
            }
        }
    }

}

__global__ void debug_bins_kernel(TileBins* bins, int tiles_x, int tiles_y) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= tiles_x * tiles_y) return;
    if (bins[i].count > MAX_FACES_PER_TILE)
        printf("tile %d overflowed: %d faces (dropped %d)\n",
            i, bins[i].count, bins[i].count - MAX_FACES_PER_TILE);
}


// ── Pass 2: Tile kernel — now only loops over THIS tile's faces ──────────
__global__ void tile_kernel(
    const face_t* __restrict__ faces,
    const float4* __restrict__ clip_verts,
    const gpu_vertex_t* __restrict__ vertices,
    uint32_t* __restrict__ frame_buffer,
    float* __restrict__ depth_buffer,
    int width, int height,
    const LightCache* __restrict__ lights, int light_count,
    const gpu_material_t* __restrict__ materials,
    const TileBins* __restrict__ bins,    // ← new
    int tiles_x)
{
    const int tile_x = blockIdx.x * TILE_SIZE;
    const int tile_y = blockIdx.y * TILE_SIZE;
    const int px = tile_x + threadIdx.x;
    const int py = tile_y + threadIdx.y;
    const int tx = threadIdx.x;
    const int ty = threadIdx.y;

    __shared__ float tile_depth[TILE_SIZE][TILE_SIZE];
    __shared__ int   tile_face[TILE_SIZE][TILE_SIZE];
    __shared__ float tile_w0[TILE_SIZE][TILE_SIZE];
    __shared__ float tile_w1[TILE_SIZE][TILE_SIZE];

    const bool in_bounds = (px < width && py < height);
    tile_depth[ty][tx] = in_bounds ? depth_buffer[py * width + px] : FLT_MAX;
    tile_face[ty][tx] = -1;
    __syncthreads();

    // ── KEY CHANGE: only iterate faces in this tile's bin ────────────────
    const TileBins& bin = bins[blockIdx.y * tiles_x + blockIdx.x];
    const int       nfaces = min(bin.count, MAX_FACES_PER_TILE);

    const float tile_xf = (float)tile_x, tile_x1f = tile_xf + TILE_SIZE;
    const float tile_yf = (float)tile_y, tile_y1f = tile_yf + TILE_SIZE;
    const float pxf = (float)px, pyf = (float)py;

    for (int fi = 0; fi < nfaces; fi++) {
        const BinEntry& entry = bin.entries[fi];
        const float4 ca = entry.ca;   // already clipped — no ldg needed
        const float4 cb = entry.cb;
        const float4 cc = entry.cc;

        float4 clipped[2][3];
        int ntris = clip_triangle(ca, cb, cc, clipped);
        if (ntris == 0) {
            printf("face %d rejected — wa=%.2f wb=%.2f wc=%.2f\n", fi, ca.w, cb.w, cc.w);
            return;
        }
        //if (ca.w <= 0.f | cb.w <= 0.f | cc.w <= 0.f) continue;

        const float rwa = 1.f / ca.w, rwb = 1.f / cb.w, rwc = 1.f / cc.w;
        const float ax = (ca.x * rwa + 1.f) * (.5f * width);
        const float ay = (1.f - ca.y * rwa) * (.5f * height);
        const float az = ca.z * rwa;
        const float bx = (cb.x * rwb + 1.f) * (.5f * width);
        const float by = (1.f - cb.y * rwb) * (.5f * height);
        const float bz = cb.z * rwb;
        const float cx = (cc.x * rwc + 1.f) * (.5f * width);
        const float cy = (1.f - cc.y * rwc) * (.5f * height);
        const float cz = cc.z * rwc;

        const float denom = (by - cy) * (ax - cx) + (cx - bx) * (ay - cy);
        if (denom >= 0.f) continue;
        const float inv_denom = 1.f / denom;



        if (in_bounds) {
            const float dpx = pxf - cx, dpy = pyf - cy;
            const float w0 = ((by - cy) * dpx + (cx - bx) * dpy) * inv_denom;
            const float w1 = ((cy - ay) * dpx + (ax - cx) * dpy) * inv_denom;
            const float w2 = 1.f - w0 - w1;

            if ((w0 >= 0.f) & (w1 >= 0.f) & (w2 >= 0.f)) {
                const float depth = w0 * az + w1 * bz + w2 * cz;
                if (depth < tile_depth[ty][tx]) {
                    tile_depth[ty][tx] = depth;
                    tile_face[ty][tx] = entry.face_idx;  // still need original for normals/material
                    tile_w0[ty][tx] = w0;
                    tile_w1[ty][tx] = w1;
                }
            }
        }
    }

    __syncthreads();
    if (!in_bounds) return;

    const int best_f = tile_face[ty][tx];
    if (best_f < 0) return;

    const float w0 = tile_w0[ty][tx];
    const float w1 = tile_w1[ty][tx];
    const float w2 = 1.f - w0 - w1;

    const face_t       best_face = faces[best_f];
    const gpu_vertex_t vA = vertices[best_face.v0];
    const gpu_vertex_t vB = vertices[best_face.v1];
    const gpu_vertex_t vC = vertices[best_face.v2];

    float nx = w0 * vA.nx + w1 * vB.nx + w2 * vC.nx;
    float ny = w0 * vA.ny + w1 * vB.ny + w2 * vC.ny;
    float nz = w0 * vA.nz + w1 * vB.nz + w2 * vC.nz;
    const float inv_len = rsqrtf(nx * nx + ny * ny + nz * nz + 1e-9f);
    nx *= inv_len; ny *= inv_len; nz *= inv_len;

    float total = 0.05f;
    for (int l = 0; l < light_count; l++) {
        const LightCache li = lights[l];
        const float diff = fmaxf(fmaf(nx, li.lx, fmaf(ny, li.ly, nz * li.lz)), 0.f);
        total = fminf(fmaf(diff, li.intensity, total), 1.f);
    }

    uint8_t mr = 255, mg = 255, mb = 255;
    if (best_face.materialIndex >= 0) {
        const gpu_material_t mat = materials[best_face.materialIndex];
        mr = mat.r; mg = mat.g; mb = mat.b;
    }

    const int idx = py * width + px;
    depth_buffer[idx] = tile_depth[ty][tx];
    frame_buffer[idx] =
        (0xFFu << 24) |
        ((uint8_t)(mr * total) << 16) |
        ((uint8_t)(mg * total) << 8) |
        ((uint8_t)(mb * total));
}

void gpu_init(Engine* engine) {
    total_vertex_count = 0;
    total_face_count = 0;
    for (int i = 0; i < engine->shapeCount; i++) {
        Shape* s = (Shape*)engine->allShapes[i].base;
        total_vertex_count += s->point_count;
        total_face_count += s->face_count;
    }

    cudaMalloc(&d_vertices, total_vertex_count * sizeof(gpu_vertex_t));
    cudaMalloc(&d_faces, total_face_count * sizeof(face_t));
    cudaMalloc(&d_clip_verts, total_vertex_count * sizeof(float4));
    cudaMalloc(&d_frame_buffer, engine->screen_width * engine->screen_height * sizeof(uint32_t));
    cudaMalloc(&d_depth_buffer, engine->screen_width * engine->screen_height * sizeof(float));
    cudaMalloc(&d_lights, engine->light_count * sizeof(LightCache));

    int tiles_x = (engine->screen_width + TILE_SIZE - 1) / TILE_SIZE;
    int tiles_y = (engine->screen_height + TILE_SIZE - 1) / TILE_SIZE;
    cached_tiles_x = tiles_x;
    cached_tiles_y = tiles_y;
    cudaMalloc(&d_bins, tiles_x * tiles_y * sizeof(TileBins));

    // FIX: allocate materials once here instead of every frame in gpu_render
    int mat_count = engine->lib ? engine->lib->count : 0;
    if (mat_count > 0) {
        cudaMalloc(&d_materials, mat_count * sizeof(gpu_material_t));
        gpu_material_t* host_mats = new gpu_material_t[mat_count];
        for (int i = 0; i < mat_count; i++) {
            uint32_t diff = engine->lib->materials[i].diffuse;
            host_mats[i] = {
                (uint8_t)((diff >> 16) & 0xFF),
                (uint8_t)((diff >> 8) & 0xFF),
                (uint8_t)(diff & 0xFF)
            };
        }
        cudaMemcpy(d_materials, host_mats, mat_count * sizeof(gpu_material_t),
            cudaMemcpyHostToDevice);
        delete[] host_mats;
    }

    int vert_offset = 0, face_offset = 0;
    for (int i = 0; i < engine->shapeCount; i++) {
        Shape* s = (Shape*)engine->allShapes[i].base;
        printf("%i", s->face_count);
        if (s->face_count > 0) {
            printf("shape %d: vert_offset=%d, point_count=%d, max_face_idx=%d\n",
                i, vert_offset, s->point_count,
                max(s->faces[0].v0, max(s->faces[0].v1, s->faces[0].v2)));
        }

        face_t* offset_faces = new face_t[s->face_count];
        for (int f = 0; f < s->face_count; f++) {
            offset_faces[f] = {
                s->faces[f].v0 + vert_offset,
                s->faces[f].v1 + vert_offset,
                s->faces[f].v2 + vert_offset,
                s->faces[f].materialIndex
            };
        }
        cudaMemcpy(d_faces + face_offset, offset_faces,
            s->face_count * sizeof(face_t), cudaMemcpyHostToDevice);
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
        cudaMemcpy(d_vertices + vert_offset, offset_vertices,
            s->point_count * sizeof(gpu_vertex_t), cudaMemcpyHostToDevice);
        delete[] offset_vertices;

        vert_offset += s->point_count;
        face_offset += s->face_count;
    }

    LightCache* host_lights = new LightCache[engine->light_count];
    for (int l = 0; l < engine->light_count; l++) {
        float lx = engine->lights[l].position.matrix.values[0];
        float ly = engine->lights[l].position.matrix.values[1];
        float lz = engine->lights[l].position.matrix.values[2];
        float len = sqrtf(lx * lx + ly * ly + lz * lz);
        host_lights[l] = { lx / len, ly / len, lz / len, engine->lights[l].intensity };
    }
    cudaMemcpy(d_lights, host_lights, engine->light_count * sizeof(LightCache),
        cudaMemcpyHostToDevice);
    delete[] host_lights;
}

void gpu_render(Engine* engine) {
    int w = engine->screen_width;
    int h = engine->screen_height;

    cudaMemset(d_frame_buffer, 0, w * h * sizeof(uint32_t));
    cudaMemset(d_depth_buffer, 0x7f, w * h * sizeof(float));

    // FIX: removed cudaMalloc for d_materials here — it now lives in gpu_init


    float ms = 0;

    // vertex pass
    int vert_offset = 0;
    for (int i = 0; i < engine->shapeCount; i++) {
        Shape* s = (Shape*)engine->allShapes[i].base;
        mat4 mvp = build_mvp(s, &engine->camera,
            90.0f * (3.14159265f / 180.0f),
            (float)w / (float)h, 0.1f, 1000.0f);

        int vblocks = (s->point_count + 255) / 256;
        vertex_kernel << <vblocks, 256 >> > (
            d_vertices + vert_offset, s->point_count, mvp,
            d_clip_verts + vert_offset);
        vert_offset += s->point_count;
    }

    // clear bins — count must be 0 before binning
    cudaMemset(d_bins, 0, cached_tiles_x * cached_tiles_y * sizeof(TileBins));

    // pass 1: bin faces into tiles
    int bin_blocks = (total_face_count + 255) / 256;
    bin_kernel << <bin_blocks, 256 >> > (
        d_faces, total_face_count, d_clip_verts,
        d_bins, w, h, cached_tiles_x, cached_tiles_y);

    // in gpu_render after bin_kernel launch:
    cudaDeviceSynchronize();


    // pass 2: rasterise — each tile only sees its own faces
    dim3 tile_threads(TILE_SIZE, TILE_SIZE);
    dim3 tile_blocks(cached_tiles_x, cached_tiles_y);
    tile_kernel << <tile_blocks, tile_threads >> > (
        d_faces, d_clip_verts, d_vertices,
        d_frame_buffer, d_depth_buffer,
        w, h, d_lights, engine->light_count, d_materials,
        d_bins, cached_tiles_x);

    cudaDeviceSynchronize();

    cudaMemcpy(engine->frame_buffer, d_frame_buffer,
        w * h * sizeof(uint32_t), cudaMemcpyDeviceToHost);

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