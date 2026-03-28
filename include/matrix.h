#pragma once
#include <memory>
/// <summary>
/// Type definition for a two dimensional matrix
/// </summary>
/// <param name="width">Width of the matrix</param>
/// <param name="height">Height of the matrix</param>
/// <param name="values">All values of the matrix in a list sized width*height</param>
/// 
typedef struct {
	int width;
	int height;
	float* values;
} matrix_t;


/// <summary>
/// Creates a matrix
/// </summary>
/// <param name="width">Width of the matrix</param>
/// <param name="height">Height of the matrix</param>
/// <returns>Resultant matrix</returns>
matrix_t create_matrix(int width, int height);


/// <summary>
/// Multiplies matrixA by matrixB
/// </summary>
/// <param name="matrixA">First matrix</param>
/// <param name="matrixB">Second matrix</param>
/// <returns>Resultant matrix</returns>
matrix_t matrix_multiply(matrix_t matrixA, matrix_t matrixB);

/// <summary>
/// Adds matrixA and matrixB
/// </summary>
/// <param name="matrixA">First matrix</param>
/// <param name="matrixB">Second matrix</param>
/// <returns>Resultant matrix</returns>
matrix_t matrix_addition(matrix_t matrixA, matrix_t matrixB);

/// <summary>
/// Subtracts matrixB from matrixA
/// </summary>
/// <param name="matrixA">First matrix</param>
/// <param name="matrixB">Second matrix</param>
/// <returns>Resultant matrix</returns>
matrix_t matrix_subtraction(matrix_t matrixA, matrix_t matrixB);

/// <summary>
/// Multiplies matrixA by a scalar float
/// </summary>
/// <param name="matrixA">Matrix to scale</param>
/// <param name="scalar">Scalar value to multiply by</param>
/// <returns>Resultant matrix</returns>
matrix_t matrix_scalar(matrix_t matrixA, float scalar);

/// <summary>
/// Transposes the matrix (flips rows and columns)
/// </summary>
/// <param name="matrix">Matrix to transpose</param>
/// <returns>Resultant matrix</returns>
matrix_t matrix_transpose(matrix_t matrix);

/// <summary>
/// Gets the determinant of a matrix for computing the inverse
/// </summary>
/// <param name="matrix">Matrix to get the determinant of</param>
/// <returns>Float determinant value</returns>
float matrix_determinant(matrix_t matrix);