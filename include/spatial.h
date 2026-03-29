#pragma once
#include "matrix.h"
#include "arena.h"
#include <string>
#include "material.h"


// Technically dont need to but this makes things infinitely more readable
typedef struct {
	matrix_t matrix;
} point_t;


// Vertex class, obj files store as vertexes so having this class makes it easier to interpret
typedef struct {
	point_t position;
	point_t normal;
	float u, v;
} vertex_t;

typedef struct {
	int v0, v1, v2;
	int materialIndex;
} face_t;


/// <summary>
/// Contains the position and orientation of the camera in 3D space
/// </summary>
class Camera {
public:
	/// <summary>
	/// 3 Dimensional Position Matrix;
	/// </summary>
	point_t position;


	/// <summary>
	/// Rotation angle around the X axis in radians
	/// </summary>
	float angleX;
	/// <summary>
	/// Rotation angle around the Y axis in radians
	/// </summary>
	float angleY;
	/// <summary>
	/// Rotation angle around the Z axis in radians
	/// </summary>
	float angleZ;
};

/// <summary>
/// Creates a point in space object
/// </summary>
/// <param name="x">X location of point</param>
/// <param name="y">Y location of point</param>
/// <param name="z">Z location of point</param>
/// <returns>Resultant point_t object</returns>
point_t create_point(float x, float y, float z);

/// <summary>
/// Shape object defined by a centroid position, width, height and a set of points
/// </summary>
class Shape {
public:
	int index;
	point_t position; // centroid
	int width;
	int height;
	Arena arena; // Memory arena for easy allocation and deallocation
	/// <summary>
	/// Loads a .obj file from a defined filepath
	/// </summary>
	/// <param name="filepath">Path to the obj file</param>
	void loadShape(std::string filepath, MTLLibrary* lib);

	void initShape(point_t position, int width, int height);

	/// <summary>
	/// Moves the shape by a given offset in each axis
	/// </summary>
	/// <param name="moveX">Offset in X axis</param>
	/// <param name="moveY">Offset in Y axis</param>
	/// <param name="moveZ">Offset in Z axis</param>
	void moveShape(float moveX, float moveY, float moveZ);

	/// <summary>
	/// Rotates the shape by a given angle in each axis
	/// </summary>
	/// <param name="angleX">Angle to rotate around X axis</param>
	/// <param name="angleY">Angle to rotate around Y axis</param>
	void rotateShape(float angleX, float angleY, float angleZ);

	/// <summary>
	/// Scales the shape uniformly by a given scalar
	/// </summary>
	/// <param name="scale">Scalar value to scale the shape by</param>
	void scaleShape(float scale);

	vertex_t* vertices;
	int point_count;

	face_t* faces;
	int face_count;
};

/// <summary>
/// Applies the Transformation vertex to convert the shape into 2D space
/// </summary>
/// <param name="vertex">The applied vertex</param>
/// <param name="shape">The shape pointer</param>
/// <param name="camera">The Camera Object</param>
/// <param name="fov">fov paramter</param>
/// <param name="aspect">aspect ratio</param>
/// <param name="near">near</param>
/// <param name="far">far, wherever you are</param>
/// <returns></returns>
matrix_t transform_vertex(
	vertex_t* vertex,
	Shape* shape,
	Camera* camera,
	float fov,
	float aspect,
	float near_plane,
	float far_plane
);

struct mat4 {
	float v[16];
};

mat4 build_mvp(Shape* shape, Camera* camera, float fov, float aspect, float near_plane, float far_plane);
void project_vertex(vertex_t* vertex, const mat4& mvp, int width, int height, float& ax, float& ay, float& az);