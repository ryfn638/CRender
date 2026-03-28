/*
Basic math library for which contains all essential tools for matrix operation
Vectors are defined in math.h
operations to add

TODO:

Matrix determinant — a scalar value needed to compute the inverse

DONE
Matrix addition — add two matrices element by element
Matrix subtraction — subtract two matrices element by element
Matrix multiplication — dot product of rows and columns
Scalar multiplication — multiply every element by a single number
Matrix transpose — flip rows and columns
Matrix inverse — find the matrix that undoes another matrix
*/

#include "matrix.h"
#include <iostream>
using namespace std;

matrix_t create_matrix(int width, int height) {
	matrix_t m;
	m.width = width;
	m.height = height;
	m.values = (float*)malloc(width * height * sizeof(float));
	return m;
}

matrix_t matrix_multiply(matrix_t matrixA, matrix_t matrixB) {
	if (matrixA.width != matrixB.height) {
		std::cout << "Invalid Vector Shape for multiplication" << std::endl;
	}

	// This copies the params of matrix B
	matrix_t output_matrix;
	output_matrix.width = matrixB.width;
	output_matrix.height = matrixB.height;
	output_matrix.values = create_matrix(output_matrix.width ,output_matrix.height).values;

	for (int row = 0; row < matrixA.height; row++) {
		for (int col = 0; col < matrixB.width; col++) {
			float sum = 0;
			for (int k = 0; k < matrixA.width; k++) {
				sum += matrixA.values[row * matrixA.width + k] * matrixB.values[k * matrixB.width + col];
			}
			output_matrix.values[row * matrixB.width + col] = sum;
		}
	}

	return output_matrix;
}

matrix_t matrix_addition(matrix_t matrixA, matrix_t matrixB)
{
	if (matrixA.width != matrixB.width || matrixA.height != matrixB.height) {
		std::cout << "Invalid Matrix Shapes, Matrices must be same shape for addition" << std::endl;
	}
	matrix_t output_matrix = matrixA;
	output_matrix.values = create_matrix(output_matrix.width, output_matrix.height).values;

	for (int row = 0; row < matrixA.height; row++) {
		for (int col = 0; col < matrixA.width; col++) {
			output_matrix.values[row * matrixB.width + col] = (matrixA.values[row * matrixB.width + col] + matrixB.values[row * matrixB.width + col]);
		}
	}

	return output_matrix;
}

matrix_t matrix_subtraction(matrix_t matrixA, matrix_t matrixB)
{
	if (matrixA.width != matrixB.width || matrixA.height != matrixB.height)
	{
		std::cout << "Invalid Matrix Shapes, Matrices must be same shape for addition" << std::endl;
	}
	matrix_t output_matrix = matrixA;
	output_matrix.values = create_matrix(output_matrix.width, output_matrix.height).values;

	for (int row = 0; row < matrixA.height; row++) {
		for (int col = 0; col < matrixA.width; col++) {
			output_matrix.values[row * matrixB.width + col] = (matrixA.values[row * matrixB.width + col] - matrixB.values[row * matrixB.width + col]);
		}
	}

	return output_matrix;
}

matrix_t matrix_scalar(matrix_t matrix, float scalar)
{
	matrix_t output_matrix = matrix;
	output_matrix.values = create_matrix(output_matrix.width, output_matrix.height).values;

	for (int row = 0; row < matrix.height; row++) {
		for (int col = 0; col < matrix.width; col++) {
			output_matrix.values[row * matrix.width + col] = (matrix.values[row * matrix.width + col] * scalar);
		}
	}

	return output_matrix;
}

matrix_t matrix_transpose(matrix_t matrix) {

	matrix_t output_matrix = matrix;
	output_matrix.values = create_matrix(output_matrix.width, output_matrix.height).values;

	for (int row = 0; row < matrix.height; row++) {
		for (int col = 0; col < matrix.width; col++) {
			output_matrix.values[col * matrix.height + row] = matrix.values[row * matrix.width + col];
		}
	}

	return output_matrix;

}


/// <summary>
/// Generates a submatrix if given a matrix and the rows it is removing. Helper function for recursively finding the determinant
/// </summary>
/// <param name="matrix">matrix we're subdividing</param>
/// <param name="remove_row">The row being removed</param>
/// <param name="remove_col">The column being removed</param>
/// <returns>Resultant matrix</returns>
matrix_t matrix_submatrix(matrix_t matrix, int remove_row, int remove_col) {
	matrix_t output;
	output.width = matrix.width - 1;
	output.height = matrix.height - 1;
	output.values = (float*)malloc(output.width * output.height * sizeof(float));

	int i = 0;
	for (int row = 1; row < matrix.height; row++) {
		for (int col = 0; col < matrix.width; col++) {
			if (col == remove_col) continue;
			output.values[i++] = matrix.values[row * matrix.width + col];
		}
	}
	return output;
}

float two_dim_determinant(matrix_t matrix)
{
	// ad - bc
	return matrix.values[0] * matrix.values[3] - matrix.values[2] * matrix.values[1];
}

float matrix_subdeterminant(matrix_t matrix)
{
	float determinant = 0;
	matrix_t submatrix;
	for (int col = 0; col < matrix.width; col++) {
		submatrix = matrix_submatrix(matrix, 0, col);
		if (submatrix.width == 2){
			determinant += matrix.values[col] * two_dim_determinant(submatrix) * (col % 2 == 0 ? 1 : -1);
		}
		else {
			determinant += matrix.values[col] * (col % 2 == 0 ? 1 : -1) * matrix_subdeterminant(submatrix);
		}
	}

	return determinant;
}


float matrix_determinant(matrix_t matrix)
{
	float determinant = 0;
	int removed_row = 0;
	matrix_subdeterminant(matrix);
	
	return determinant;
}
