#include "terrain.h"
#include "chunk.h"
#include "raymath.h"
#include <math.h>

ChunkData chunks[CHUNKS_X][CHUNKS_Z];

Vector3 GetWorldPosition(int chunkX, int chunkZ, int vx, int vy, int vz)
{
  return (Vector3){
      (chunkX * (CHUNK_SIZE - 1) + vx) - ((CHUNKS_X * (CHUNK_SIZE - 1)) / 2.0f),
      (float)vy - CHUNK_SIZE / 2.0f,
      (chunkZ * (CHUNK_SIZE - 1) + vz) - ((CHUNKS_Z * (CHUNK_SIZE - 1)) / 2.0f)};
}

bool GetChunkCoords(Vector3 worldPos, int *chunkX, int *chunkZ, int *vx, int *vy, int *vz)
{
  // Offset position to make (0,0,0) be at the center of the world
  float offsetX = worldPos.x + ((CHUNKS_X * (CHUNK_SIZE - 1)) / 2.0f);
  float offsetZ = worldPos.z + ((CHUNKS_Z * (CHUNK_SIZE - 1)) / 2.0f);
  float offsetY = worldPos.y + CHUNK_SIZE / 2.0f;

  // Calculate chunk coordinates
  *chunkX = (int)(offsetX / (CHUNK_SIZE - 1));
  *chunkZ = (int)(offsetZ / (CHUNK_SIZE - 1));

  // Calculate voxel coordinates within chunk
  *vx = (int)(offsetX) % (CHUNK_SIZE - 1);
  *vy = (int)(offsetY);
  *vz = (int)(offsetZ) % (CHUNK_SIZE - 1);

  // Check if coordinates are within bounds
  if (*chunkX < 0 || *chunkX >= CHUNKS_X || *chunkZ < 0 || *chunkZ >= CHUNKS_Z ||
      *vy < 0 || *vy >= CHUNK_SIZE)
  {
    return false;
  }

  // Handle negative coordinates
  if (*vx < 0)
    *vx += CHUNK_SIZE - 1;
  if (*vz < 0)
    *vz += CHUNK_SIZE - 1;

  return true;
}

void ModifyTerrain(Vector3 position, float radius, float strength)
{
  // Calculate affected chunk range
  int minChunkX = (int)((position.x - radius + ((CHUNKS_X * (CHUNK_SIZE - 1)) / 2.0f)) / (CHUNK_SIZE - 1));
  int maxChunkX = (int)((position.x + radius + ((CHUNKS_X * (CHUNK_SIZE - 1)) / 2.0f)) / (CHUNK_SIZE - 1));
  int minChunkZ = (int)((position.z - radius + ((CHUNKS_Z * (CHUNK_SIZE - 1)) / 2.0f)) / (CHUNK_SIZE - 1));
  int maxChunkZ = (int)((position.z + radius + ((CHUNKS_Z * (CHUNK_SIZE - 1)) / 2.0f)) / (CHUNK_SIZE - 1));

  // Clamp to valid chunk range
  minChunkX = minChunkX < 0 ? 0 : minChunkX;
  maxChunkX = maxChunkX >= CHUNKS_X ? CHUNKS_X - 1 : maxChunkX;
  minChunkZ = minChunkZ < 0 ? 0 : minChunkZ;
  maxChunkZ = maxChunkZ >= CHUNKS_Z ? CHUNKS_Z - 1 : maxChunkZ;

  float radiusSq = radius * radius;

  // Only iterate over chunks that could be affected
  for (int cx = minChunkX; cx <= maxChunkX; cx++)
  {
    for (int cz = minChunkZ; cz <= maxChunkZ; cz++)
    {
      bool chunkModified = false;
      Vector3 chunkPos = chunks[cx][cz].chunk.position;

      // Convert world position to local chunk space
      Vector3 localPos = {
          position.x - chunkPos.x,
          position.y - chunkPos.y,
          position.z - chunkPos.z};

      // Calculate affected voxel range within chunk
      int minX = (int)((localPos.x - radius) / VOXEL_SIZE);
      int maxX = (int)((localPos.x + radius) / VOXEL_SIZE) + 1;
      int minY = (int)((localPos.y - radius) / VOXEL_SIZE);
      int maxY = (int)((localPos.y + radius) / VOXEL_SIZE) + 1;
      int minZ = (int)((localPos.z - radius) / VOXEL_SIZE);
      int maxZ = (int)((localPos.z + radius) / VOXEL_SIZE) + 1;

      // Clamp to chunk bounds
      minX = minX < 0 ? 0 : minX;
      maxX = maxX >= CHUNK_SIZE ? CHUNK_SIZE - 1 : maxX;
      minY = minY < 0 ? 0 : minY;
      maxY = maxY >= CHUNK_SIZE ? CHUNK_SIZE - 1 : maxY;
      minZ = minZ < 0 ? 0 : minZ;
      maxZ = maxZ >= CHUNK_SIZE ? CHUNK_SIZE - 1 : maxZ;

      // Only check voxels within the calculated range
      for (int x = minX; x <= maxX; x++)
      {
        for (int y = minY; y <= maxY; y++)
        {
          for (int z = minZ; z <= maxZ; z++)
          {
            // Convert voxel position to world space for distance check
            Vector3 voxelPos = {
                chunkPos.x + x * VOXEL_SIZE,
                chunkPos.y + y * VOXEL_SIZE,
                chunkPos.z + z * VOXEL_SIZE};

            float distSq = Vector3DistanceSqr(position, voxelPos);

            if (distSq <= radiusSq)
            {
              // Calculate horizontal distance (ignoring Y component)
              Vector3 horizontalDiff = {
                  voxelPos.x - position.x,
                  0,
                  voxelPos.z - position.z};
              float horizontalDistSq = Vector3LengthSqr(horizontalDiff);

              // Calculate vertical distance and direction
              float verticalDist = voxelPos.y - position.y;
              float verticalFactor = 1.0f - (fabsf(verticalDist) / radius);

              // Calculate horizontal falloff (smoother transition at edges)
              float horizontalFactor = 1.0f - (sqrtf(horizontalDistSq) / radius);

              // Calculate base influence that only affects vertical movement
              float influence = 0;

              if (strength > 0)
              {
                // Pull up: stronger effect above the point, weaker below
                influence = verticalFactor * horizontalFactor * 0.05f * strength;
                influence *= (verticalDist >= 0) ? 1.2f : 0.8f;
              }
              else
              {
                // Push down: stronger effect below the point, weaker above
                influence = verticalFactor * horizontalFactor * 0.05f * strength;
                influence *= (verticalDist <= 0) ? 1.2f : 0.8f;
              }

              // Apply influence only if it's significant enough
              if (fabsf(influence) > 0.001f)
              {
                chunks[cx][cz].chunk.voxels[x][y][z].density += influence;
                chunkModified = true;
              }
            }
          }
        }
      }

      if (chunkModified)
      {
        chunks[cx][cz].needsUpdate = true;
      }
    }
  }
}

bool IsInsideTerrain(Vector3 pos)
{
  int chunkX, chunkZ, vx, vy, vz;
  if (GetChunkCoords(pos, &chunkX, &chunkZ, &vx, &vy, &vz))
  {
    return chunks[chunkX][chunkZ].chunk.voxels[vx][vy][vz].density <= 0.0f;
  }
  return false;
}

float GetDensityAtPosition(Vector3 pos)
{
  int chunkX, chunkZ, vx, vy, vz;
  if (GetChunkCoords(pos, &chunkX, &chunkZ, &vx, &vy, &vz))
  {
    return chunks[chunkX][chunkZ].chunk.voxels[vx][vy][vz].density;
  }
  return 1000.0f; // Return high density for out of bounds
}
