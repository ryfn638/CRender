#include "spatial.h"
#include "matrix.h"
#include <math.h>
#include "engine.h"
#include <iostream>

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

void Shape::loadShape(std::string filepath) {
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

    // allocate vertices, normals, uvs and faces
    this->vertices = (vertex_t*)malloc(vertex_count * sizeof(vertex_t));
    this->point_count = vertex_count;

    matrix_t* normals = (matrix_t*)malloc(normal_count * sizeof(matrix_t));
    float* us = (float*)malloc(uv_count * sizeof(float));
    float* vs = (float*)malloc(uv_count * sizeof(float));
    this->faces = (face_t*)malloc(face_count * 2 * sizeof(face_t)); // *2 for quad splitting
    this->face_count = 0;

    // second pass — read all data
    rewind(file);
    int vi = 0, ni = 0, uvi = 0;
    while (fgets(line, sizeof(line), file)) {
        if (line[0] == 'v' && line[1] == ' ') {
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
            int v0, v1, v2, v3 = -1;
            int n0, n1, n2, n3 = -1;
            int t0, t1, t2, t3 = -1;

            // try quad first then triangle
            int matches = sscanf_s(line, "f %d//%d %d//%d %d//%d %d//%d",
                &v0, &n0, &v1, &n1, &v2, &n2, &v3, &n3);

            if (matches < 6) {
                sscanf_s(line, "f %d/%d/%d %d/%d/%d %d/%d/%d %d/%d/%d",
                    &v0, &t0, &n0, &v1, &t1, &n1, &v2, &t2, &n2, &v3, &t3, &n3);
            }

            // OBJ indices are 1-based so subtract 1
            // assign normals to vertices
            this->vertices[v0 - 1].normal.matrix = normals[n0 - 1];
            this->vertices[v1 - 1].normal.matrix = normals[n1 - 1];
            this->vertices[v2 - 1].normal.matrix = normals[n2 - 1];

            // assign uvs if present
            if (t0 != -1) {
                this->vertices[v0 - 1].u = us[t0 - 1];
                this->vertices[v0 - 1].v = vs[t0 - 1];
                this->vertices[v1 - 1].u = us[t1 - 1];
                this->vertices[v1 - 1].v = vs[t1 - 1];
                this->vertices[v2 - 1].u = us[t2 - 1];
                this->vertices[v2 - 1].v = vs[t2 - 1];
            }

            // first triangle
            this->faces[this->face_count++] = { v0 - 1, v1 - 1, v2 - 1 };

            // if quad split into second triangle
            if (v3 != -1) {
                this->vertices[v3 - 1].normal.matrix = normals[n3 - 1];
                if (t3 != -1) {
                    this->vertices[v3 - 1].u = us[t3 - 1];
                    this->vertices[v3 - 1].v = vs[t3 - 1];
                }
                this->faces[this->face_count++] = { v0 - 1, v2 - 1, v3 - 1 };
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

uint32_t convert_colour(uint8_t* rgba) {
    uint32_t out_colour = 0x00000000;
    for (int i = 0; i < 4; i++) {
        uint32_t mask = (uint32_t)rgba[i];
        out_colour |= mask << i * 8;
    }

    return out_colour;
}