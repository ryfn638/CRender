#include "spatial.h"
#include "matrix.h"
#include <math.h>
#include "engine.h"
#include <iostream>
#include "material.h"
using namespace std;

/* ----------------------------
* ROTATION MATRIX DEFINITIONS
* --------------------------- */


matrix_t rotation_matrix_x(float angle) {
    matrix_t m = create_matrix(3, 3);
    m.values[0] = 1;  m.values[1] = 0;          m.values[2] = 0;
    m.values[3] = 0;  m.values[4] = cos(angle); m.values[5] = -sin(angle);
    m.values[6] = 0;  m.values[7] = sin(angle); m.values[8] = cos(angle);
    return m;
}

matrix_t rotation_matrix_y(float angle) {
    matrix_t m = create_matrix(3, 3);
    m.values[0] = cos(angle);  m.values[1] = 0; m.values[2] = sin(angle);
    m.values[3] = 0;           m.values[4] = 1; m.values[5] = 0;
    m.values[6] = -sin(angle); m.values[7] = 0; m.values[8] = cos(angle);
    return m;
}

matrix_t rotation_matrix_z(float angle) {
    matrix_t m = create_matrix(3, 3);
    m.values[0] = cos(angle);  m.values[1] = -sin(angle); m.values[2] = 0;
    m.values[3] = sin(angle);  m.values[4] = cos(angle);  m.values[5] = 0;
    m.values[6] = 0;           m.values[7] = 0;           m.values[8] = 1;
    return m;
}

matrix_t create_model_matrix(Shape* shape) {
    matrix_t model = create_matrix(4, 4);
    // identity matrix
    model.values[0] = 1; model.values[1] = 0; model.values[2] = 0; model.values[3] = shape->position.matrix.values[0];
    model.values[4] = 0; model.values[5] = 1; model.values[6] = 0; model.values[7] = shape->position.matrix.values[1];
    model.values[8] = 0; model.values[9] = 0; model.values[10] = 1; model.values[11] = shape->position.matrix.values[2];
    model.values[12] = 0; model.values[13] = 0; model.values[14] = 0; model.values[15] = 1;
    return model;
}

matrix_t create_view_matrix(Camera* camera) {
    matrix_t view = create_matrix(4, 4);
    matrix_t rx = rotation_matrix_x(camera->angleX);
    matrix_t ry = rotation_matrix_y(camera->angleY);
    matrix_t rz = rotation_matrix_z(camera->angleZ);
    matrix_t rot = matrix_multiply(matrix_multiply(rx, ry), rz);

    // embed rotation into 4x4 and apply camera translation
    view.values[0] = rot.values[0]; view.values[1] = rot.values[1]; view.values[2] = rot.values[2]; view.values[3] = -camera->position.matrix.values[0];
    view.values[4] = rot.values[3]; view.values[5] = rot.values[4]; view.values[6] = rot.values[5]; view.values[7] = -camera->position.matrix.values[1];
    view.values[8] = rot.values[6]; view.values[9] = rot.values[7]; view.values[10] = rot.values[8]; view.values[11] = -camera->position.matrix.values[2];
    view.values[12] = 0;             view.values[13] = 0;             view.values[14] = 0;             view.values[15] = 1;
    return view;
}

matrix_t create_projection_matrix(float fov, float aspect, float near, float far) {
    matrix_t proj = create_matrix(4, 4);
    float t = 1.0f / tan(fov / 2.0f);
    proj.values[0] = t / aspect; proj.values[1] = 0; proj.values[2] = 0;                              proj.values[3] = 0;
    proj.values[4] = 0;          proj.values[5] = t; proj.values[6] = 0;                              proj.values[7] = 0;
    proj.values[8] = 0;          proj.values[9] = 0; proj.values[10] = -(far + near) / (far - near);   proj.values[11] = -(2 * far * near) / (far - near);
    proj.values[12] = 0;          proj.values[13] = 0; proj.values[14] = -1;                             proj.values[15] = 0;
    return proj;
}

matrix_t to_4d(matrix_t m) {
    matrix_t out = create_matrix(1, 4);
    out.values[0] = m.values[0];
    out.values[1] = m.values[1];
    out.values[2] = m.values[2];
    out.values[3] = 1.0f; // w = 1
    return out;
}

matrix_t transform_vertex(vertex_t* vertex, Shape* shape, Camera* camera, float fov, float aspect, float near_plane, float far_plane) {
    matrix_t model = create_model_matrix(shape);
    matrix_t view = create_view_matrix(camera);
    matrix_t proj = create_projection_matrix(fov, aspect, near_plane, far_plane);
    matrix_t mvp = matrix_multiply(matrix_multiply(proj, view), model);
    matrix_t pos4 = to_4d(vertex->position.matrix);
    return matrix_multiply(mvp, pos4);
}

// inline 4x4 multiply, no malloc
mat4 mat4_multiply(const mat4& a, const mat4& b) {
    mat4 out = {};
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++)
            for (int k = 0; k < 4; k++)
                out.v[r * 4 + c] += a.v[r * 4 + k] * b.v[k * 4 + c];
    return out;
}

mat4 build_mvp(Shape* shape, Camera* camera, float fov, float aspect, float near_plane, float far_plane) {
    mat4 model = {
        1,0,0,shape->position.matrix.values[0],
        0,1,0,shape->position.matrix.values[1],
        0,0,1,shape->position.matrix.values[2],
        0,0,0,1
    };

    float cx = cos(camera->angleX), sx = sin(camera->angleX);
    float cy = cos(camera->angleY), sy = sin(camera->angleY);
    float cz = cos(camera->angleZ), sz = sin(camera->angleZ);

    float px = camera->position.matrix.values[0];
    float py = camera->position.matrix.values[1];
    float pz = camera->position.matrix.values[2];

    // rotation first, then translate by -pos rotated into view space
    mat4 view = {
        cy * cz,              cy * sz,             -sy,    -(cy * cz * px + cy * sz * py + -sy * pz),
        sx * sy * cz - cx * sz,     sx * sy * sz + cx * cz,    sx * cy, -((sx * sy * cz - cx * sz) * px + (sx * sy * sz + cx * cz) * py + sx * cy * pz),
        cx * sy * cz + sx * sz,     cx * sy * sz - sx * cz,    cx * cy, -((cx * sy * cz + sx * sz) * px + (cx * sy * sz - sx * cz) * py + cx * cy * pz),
        0,                  0,                  0,      1
    };

    float t = 1.0f / tan(fov / 2.0f);
    float fn = far_plane + near_plane;
    float fd = far_plane - near_plane;
    mat4 proj = {
        t / aspect, 0,  0,                    0,
        0,        t,  0,                    0,
        0,        0, -fn / fd,               -(2 * far_plane * near_plane) / fd,
        0,        0, -1,                    0
    };

    return mat4_multiply(mat4_multiply(proj, view), model);
}

void project_vertex(vertex_t* vertex, const mat4& mvp, int width, int height, float& ox, float& oy, float& oz) {
    float x = vertex->position.matrix.values[0];
    float y = vertex->position.matrix.values[1];
    float z = vertex->position.matrix.values[2];

    float cx = mvp.v[0] * x + mvp.v[1] * y + mvp.v[2] * z + mvp.v[3];
    float cy = mvp.v[4] * x + mvp.v[5] * y + mvp.v[6] * z + mvp.v[7];
    float cz = mvp.v[8] * x + mvp.v[9] * y + mvp.v[10] * z + mvp.v[11];
    float cw = mvp.v[12] * x + mvp.v[13] * y + mvp.v[14] * z + mvp.v[15];

    float inv_w = 1.0f / cw;
    ox = (cx * inv_w + 1.0f) * 0.5f * width;
    oy = (1.0f - cy * inv_w) * 0.5f * height;
    oz = cz * inv_w;
}



void Shape::moveShape(float moveX, float moveY, float moveZ) {
    matrix_t movementMatrix = create_matrix(1, 3);
    movementMatrix.values[0] = moveX;
    movementMatrix.values[1] = moveY;
    movementMatrix.values[2] = moveZ;
    this->position.matrix = matrix_addition(this->position.matrix, movementMatrix);
}

void Shape::rotateShape(float angleX, float angleY, float angleZ) {
    for (int i = 0; i < this->point_count; i++) {
        this->vertices[i].position.matrix = matrix_multiply(rotation_matrix_x(angleX), this->vertices[i].position.matrix);
        this->vertices[i].position.matrix = matrix_multiply(rotation_matrix_y(angleY), this->vertices[i].position.matrix);
        this->vertices[i].position.matrix = matrix_multiply(rotation_matrix_z(angleZ), this->vertices[i].position.matrix);
    }
}

void Shape::scaleShape(float scale) {
    for (int i = 0; i < this->point_count; i++) {
        this->vertices[i].position.matrix = matrix_scalar(this->vertices[i].position.matrix, scale);
    }
}

point_t create_point(float x, float y, float z) {
    point_t p;
    p.matrix.width = 1;
    p.matrix.height = 3;
    p.matrix.values = (float*)malloc(3 * sizeof(float));
    p.matrix.values[0] = x;
    p.matrix.values[1] = y;
    p.matrix.values[2] = z;
    return p;
}

void Shape::loadShape(std::string filepath, MTLLibrary* lib) {
    FILE* file;
    fopen_s(&file, filepath.c_str(), "r");
    if (!file) {
        printf("Failed to open file: %s\n", filepath.c_str());
        return;
    }

    // first pass — count vertices, normals, texture coords and faces
    int vertex_count = 0, normal_count = 0, uv_count = 0, face_count = 0;
    char line[256];
    while (fgets(line, sizeof(line), file)) {
        if (line[0] == 'v' && line[1] == ' ')  vertex_count++;
        else if (line[0] == 'v' && line[1] == 'n')  normal_count++;
        else if (line[0] == 'v' && line[1] == 't')  uv_count++;
        else if (line[0] == 'f' && line[1] == ' ')  face_count++;
    }

    this->vertices = (vertex_t*)malloc(vertex_count * sizeof(vertex_t));
    this->point_count = vertex_count;

    matrix_t* normals = (matrix_t*)malloc(normal_count * sizeof(matrix_t));
    float* us = (float*)malloc(uv_count * sizeof(float));
    float* vs = (float*)malloc(uv_count * sizeof(float));
    this->faces = (face_t*)malloc(face_count * 2 * sizeof(face_t));
    this->face_count = 0;

    // second pass — read all data
    rewind(file);
    int vi = 0, ni = 0, uvi = 0;
    int currentMaterialIndex = -1;  // -1 = no material assigned yet

    while (fgets(line, sizeof(line), file)) {

        // track current material
        if (strncmp(line, "usemtl", 6) == 0) {
            currentMaterialIndex = -1;

            if (lib != nullptr) {
                char matName[256];
                sscanf_s(line, "usemtl %s", matName, (unsigned)sizeof(matName));
                matName[strcspn(matName, "\r\n")] = '\0';

                for (int i = 0; i < lib->count; i++) {
                    if (lib->materials[i].name == matName) {
                        currentMaterialIndex = i;
                        break;
                    }
                }
            }
        }
        else if (line[0] == 'v' && line[1] == ' ') {
            float x, y, z;
            sscanf_s(line, "v %f %f %f", &x, &y, &z);
            this->vertices[vi].position.matrix = create_matrix(1, 3);
            this->vertices[vi].position.matrix.values[0] = x;
            this->vertices[vi].position.matrix.values[1] = y;
            this->vertices[vi].position.matrix.values[2] = z;
            this->vertices[vi].u = 0;
            this->vertices[vi].v = 0;
            vi++;
        }
        else if (line[0] == 'v' && line[1] == 'n') {
            float x, y, z;
            sscanf_s(line, "vn %f %f %f", &x, &y, &z);
            normals[ni] = create_matrix(1, 3);
            normals[ni].values[0] = x;
            normals[ni].values[1] = y;
            normals[ni].values[2] = z;
            ni++;
        }
        else if (line[0] == 'v' && line[1] == 't') {
            float u, v;
            sscanf_s(line, "vt %f %f", &u, &v);
            us[uvi] = u;
            vs[uvi] = v;
            uvi++;
        }
        else if (line[0] == 'f' && line[1] == ' ') {
            // parse all vertices in the face generically
            int vIdx[8], nIdx[8], tIdx[8];
            int vertCount = 0;

            // fill with -1
            memset(vIdx, -1, sizeof(vIdx));
            memset(nIdx, -1, sizeof(nIdx));
            memset(tIdx, -1, sizeof(tIdx));

            const char* ptr = line + 2; // skip "f "
            while (*ptr && *ptr != '\n' && vertCount < 8) {
                int v = -1, t = -1, n = -1;
                if (sscanf_s(ptr, "%d//%d", &v, &n) == 2) {}
                else if (sscanf_s(ptr, "%d/%d/%d", &v, &t, &n) == 3) {}
                else if (sscanf_s(ptr, "%d/%d", &v, &t) == 2) {}
                else if (sscanf_s(ptr, "%d", &v) == 1) {}

                vIdx[vertCount] = v;
                nIdx[vertCount] = n;
                tIdx[vertCount] = t;
                vertCount++;

                // advance ptr to next space
                while (*ptr && *ptr != ' ' && *ptr != '\n') ptr++;
                while (*ptr == ' ') ptr++;
            }

            // assign normals and uvs to all verts
            for (int i = 0; i < vertCount; i++) {
                if (nIdx[i] > 0)
                    this->vertices[vIdx[i] - 1].normal.matrix = normals[nIdx[i] - 1];
                if (tIdx[i] > 0) {
                    this->vertices[vIdx[i] - 1].u = us[tIdx[i] - 1];
                    this->vertices[vIdx[i] - 1].v = vs[tIdx[i] - 1];
                }
            }

            // triangle fan from v0 — works for tri, quad, pentagon, etc.
            #pragma omp parallel for schedule(dynamic, 64)
            for (int i = 1; i < vertCount - 1; i++) {
                this->faces[this->face_count++] = {
                    vIdx[0] - 1,
                    vIdx[i] - 1,
                    vIdx[i + 1] - 1,
                    currentMaterialIndex
                };
            }
        }
    }

    free(normals);
    free(us);
    free(vs);
    fclose(file);
}

void Shape::initShape(point_t position, int width, int height) {
    this->position = position;
    this->width = width;
    this->height = height;
    this->vertices = nullptr;
    this->faces = nullptr;
    this->point_count = 0;
}

// Conversion from rgb struct to uint32_t (more efficient)

uint32_t convert_colour(std::array<uint8_t, 3> rgb) {
    uint32_t out_colour = 0xFF000000; // full alpha
    for (int i = 0; i < 3; i++) {
        uint32_t mask = (uint32_t)rgb[i];
        out_colour |= mask << (i * 8);
    }
    return out_colour;
}