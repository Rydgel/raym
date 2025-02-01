#ifndef CHUNK_H
#define CHUNK_H

#include "raylib.h"

#define CHUNK_SIZE 64
#define VOXEL_SIZE 1.0f
#define CHUNKS_X 4
#define CHUNKS_Z 4

typedef struct
{
  float density;
} Voxel;

typedef struct
{
  Vector3 position;
  Voxel voxels[CHUNK_SIZE][CHUNK_SIZE][CHUNK_SIZE];
} Chunk;

typedef struct
{
  Chunk chunk;
  Mesh mesh;
  Model model;
  bool initialized;
  bool needsUpdate;
  float updateTimer;
  float minHeight;
  float maxHeight;
} ChunkData;

#endif // CHUNK_H