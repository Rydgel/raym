#ifndef TERRAIN_H
#define TERRAIN_H

#include "raylib.h"
#include "chunk.h"

// Function declarations for terrain modification
void ModifyTerrain(Vector3 position, float radius, float strength);
bool IsInsideTerrain(Vector3 pos);
float GetDensityAtPosition(Vector3 pos);
bool GetChunkCoords(Vector3 worldPos, int *chunkX, int *chunkZ, int *vx, int *vy, int *vz);
Vector3 GetWorldPosition(int chunkX, int chunkZ, int vx, int vy, int vz);

#endif // TERRAIN_H