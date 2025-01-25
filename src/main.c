#include "raylib.h"
#include "raymath.h"
#include "chunk.h"
#include "marching_cubes.h"
#include <stdlib.h>
#include <math.h>

#define CHUNK_SIZE 16
#define VOXEL_SIZE 1.0f

int main(void)
{
  // Window dimensions
  const int screenWidth = 800;
  const int screenHeight = 600;

  // Initialize window
  InitWindow(screenWidth, screenHeight, "Marching Cubes Demo");

  // Initialize camera
  Camera3D camera = {0};
  camera.position = (Vector3){10.0f, 10.0f, 10.0f};
  camera.target = (Vector3){0.0f, 0.0f, 0.0f};
  camera.up = (Vector3){0.0f, 1.0f, 0.0f};
  camera.fovy = 45.0f;
  camera.projection = CAMERA_PERSPECTIVE;

  // Create a chunk
  Chunk chunk = {0};
  chunk.position = (Vector3){0.0f, 0.0f, 0.0f};

  // Initialize voxel data with a simple sphere
  for (int x = 0; x < CHUNK_SIZE; x++)
  {
    for (int y = 0; y < CHUNK_SIZE; y++)
    {
      for (int z = 0; z < CHUNK_SIZE; z++)
      {
        Vector3 pos = {
            (float)x - CHUNK_SIZE / 2.0f,
            (float)y - CHUNK_SIZE / 2.0f,
            (float)z - CHUNK_SIZE / 2.0f};
        float distance = Vector3Length(pos);
        chunk.voxels[x][y][z].density = 5.0f - distance;
      }
    }
  }

  // Generate mesh from chunk
  Mesh mesh = GenerateChunkMesh(&chunk);

  // Create material for the mesh
  Material material = LoadMaterialDefault();
  material.maps[MATERIAL_MAP_DIFFUSE].color = BLUE;

  SetTargetFPS(60);

  // Main game loop
  while (!WindowShouldClose())
  {
    // Update camera
    UpdateCamera(&camera, CAMERA_FREE);

    // Draw
    BeginDrawing();
    ClearBackground(RAYWHITE);

    BeginMode3D(camera);
    // Draw grid for reference
    DrawGrid(20, 1.0f);

    // Draw the marching cubes mesh
    DrawMesh(mesh, material, MatrixIdentity());

    EndMode3D();

    // Draw FPS
    DrawFPS(10, 10);
    EndDrawing();
  }

  // Cleanup
  UnloadMesh(mesh);
  CloseWindow();
  return EXIT_SUCCESS;
}