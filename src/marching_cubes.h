#ifndef MARCHING_CUBES_H
#define MARCHING_CUBES_H

#include "raylib.h"
#include "raymath.h"
#include "chunk.h"

// Vertex interpolation threshold
#define SURFACE_THRESHOLD 0.0f

// Function to get interpolated position between two vertices
Vector3 VertexInterp(float isolevel, Vector3 p1, Vector3 p2, float v1, float v2);

// Function to generate triangles for a cube
int GenerateTriangles(Vector3 *vertices, Vector3 *triangles, float *cornerValues, Vector3 *cornerPositions);

// Function to generate mesh for a chunk
Mesh GenerateChunkMesh(const Chunk *chunk);

#endif // MARCHING_CUBES_H
