#ifndef CHUNK_H
#define CHUNK_H

#include "raylib.h"

#define CHUNK_SIZE 64
#define VOXEL_SIZE 1.0f

typedef struct
{
  float density;
} Voxel;

typedef struct
{
  Voxel voxels[CHUNK_SIZE][CHUNK_SIZE][CHUNK_SIZE];
  Vector3 position;
} Chunk;

#endif // CHUNK_H