#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include "chunk.h"
#include "marching_cubes.h"
#include <stdlib.h>
#include <math.h>

int main(void)
{
  // Window dimensions
  const int screenWidth = 800;
  const int screenHeight = 600;

  // Initialize window
  InitWindow(screenWidth, screenHeight, "Marching Cubes Demo");

  // Initialize camera
  Camera3D camera = {0};
  camera.position = (Vector3){35.0f, 25.0f, 35.0f};
  camera.target = (Vector3){0.0f, 10.0f, 0.0f};
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

        // Simple terrain noise function
        float height = 8.0f +
                       sinf(pos.x * 0.3f) * 2.5f +
                       cosf(pos.z * 0.3f) * 2.5f +
                       sinf(pos.x * 0.1f) * cosf(pos.z * 0.1f) * 3.0f -
                       5.0f;

        chunk.voxels[x][y][z].density = pos.y - height;
      }
    }
  }

  // Generate mesh from chunk
  Mesh mesh = GenerateChunkMesh(&chunk);

  // Replace material setup with:
  Shader lightingShader = LoadShader("resources/shaders/lighting_shader.vs", "resources/shaders/lighting_shader.fs");
  Material material = LoadMaterialDefault();
  material.shader = lightingShader;

  // Set shader locations
  SetShaderValue(material.shader, GetShaderLocation(material.shader, "objectColor"), (float[3]){0.0f, 0.4f, 0.0f}, SHADER_UNIFORM_VEC3);
  SetShaderValue(material.shader, GetShaderLocation(material.shader, "lightColor"), (float[3]){1.0f, 1.0f, 1.0f}, SHADER_UNIFORM_VEC3);
  SetShaderValue(material.shader, GetShaderLocation(material.shader, "lightPos"), (float[3]){50.0f, 50.0f, 50.0f}, SHADER_UNIFORM_VEC3);

  SetTargetFPS(60);

  // Main game loop
  while (!WindowShouldClose())
  {
    // Update camera
    UpdateCamera(&camera, CAMERA_FREE);

    // Draw
    BeginDrawing();
    ClearBackground(RAYWHITE);

    DrawRectangleGradientV(0, 0, screenWidth, screenHeight, SKYBLUE, DARKBLUE);

    BeginMode3D(camera);
    // Draw grid for reference
    rlEnableDepthMask();        // Enable depth buffer writing
    rlDisableBackfaceCulling(); // Disable backface culling for better terrain visibility
    rlEnableDepthTest();        // Enable depth testing
    DrawGrid(20, 1.0f);

    // Draw the marching cubes mesh
    DrawMesh(mesh, material, MatrixIdentity());

    EndMode3D();

    // Draw FPS
    DrawFPS(10, 10);
    EndDrawing();

    // In the draw loop, update camera position:
    SetShaderValue(material.shader, GetShaderLocation(material.shader, "viewPos"), (float[3]){camera.position.x, camera.position.y, camera.position.z}, SHADER_UNIFORM_VEC3);
  }

  // Cleanup
  UnloadMesh(mesh);
  CloseWindow();
  return EXIT_SUCCESS;
}