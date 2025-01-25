#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include "chunk.h"
#include "marching_cubes.h"
#include <stdlib.h>
#include <math.h>

#define CHUNKS_X 4
#define CHUNKS_Z 4

// Add color gradient settings
#define LOW_HEIGHT -15.0f
#define HIGH_HEIGHT 15.0f

typedef struct
{
  Chunk chunk;
  Mesh mesh;
  bool initialized;
  float minHeight; // Track height range for this chunk
  float maxHeight;
} ChunkData;

ChunkData chunks[CHUNKS_X][CHUNKS_Z];

// Helper function to get world position
static Vector3 GetWorldPosition(int chunkX, int chunkZ, int vx, int vy, int vz)
{
  return (Vector3){
      (chunkX * (CHUNK_SIZE - 1) + vx) - ((CHUNKS_X * (CHUNK_SIZE - 1)) / 2.0f),
      (float)vy - CHUNK_SIZE / 2.0f,
      (chunkZ * (CHUNK_SIZE - 1) + vz) - ((CHUNKS_Z * (CHUNK_SIZE - 1)) / 2.0f)};
}

int main(void)
{
  // Window dimensions
  const int screenWidth = 800;
  const int screenHeight = 600;

  // Initialize window
  InitWindow(screenWidth, screenHeight, "Marching Cubes Demo");

  // Initialize camera
  Camera3D camera = {0};
  camera.position = (Vector3){50.0f, 35.0f, 50.0f};
  camera.target = (Vector3){0.0f, 0.0f, 0.0f};
  camera.up = (Vector3){0.0f, 1.0f, 0.0f};
  camera.fovy = 45.0f;
  camera.projection = CAMERA_PERSPECTIVE;

  // Initialize chunks
  float globalMinHeight = 1000.0f;
  float globalMaxHeight = -1000.0f;

  for (int x = 0; x < CHUNKS_X; x++)
  {
    for (int z = 0; z < CHUNKS_Z; z++)
    {
      chunks[x][z].initialized = false;
      chunks[x][z].minHeight = 1000.0f;
      chunks[x][z].maxHeight = -1000.0f;
      chunks[x][z].chunk.position = (Vector3){
          x * (CHUNK_SIZE - 1) - ((CHUNKS_X * (CHUNK_SIZE - 1)) / 2.0f),
          0.0f,
          z * (CHUNK_SIZE - 1) - ((CHUNKS_Z * (CHUNK_SIZE - 1)) / 2.0f)};

      // Initialize voxel data for each chunk
      for (int vx = 0; vx < CHUNK_SIZE; vx++)
      {
        for (int vy = 0; vy < CHUNK_SIZE; vy++)
        {
          for (int vz = 0; vz < CHUNK_SIZE; vz++)
          {
            Vector3 worldPos = GetWorldPosition(x, z, vx, vy, vz);

            // Multi-octave noise for more natural terrain
            float frequency = 0.03f; // Lower frequency for larger features
            float amplitude = 12.0f; // Increased amplitude for more height variation
            float height = -5.0f;    // Start below zero for more valleys
            int octaves = 5;         // More octaves for more detail

            for (int o = 0; o < octaves; o++)
            {
              float noiseX = worldPos.x * frequency;
              float noiseZ = worldPos.z * frequency;

              height += sinf(noiseX) * cosf(noiseZ) * amplitude;
              height += sinf(noiseX * 1.5f + cosf(noiseZ * 0.8f)) * amplitude * 0.5f;

              // Reduce amplitude and increase frequency for each octave
              amplitude *= 0.45f;
              frequency *= 2.2f;
            }

            // Add some random variation for smaller details
            height += sinf(worldPos.x * 0.15f + worldPos.z * 0.2f) * 4.0f;
            height += cosf(worldPos.x * 0.2f + worldPos.z * 0.15f) * 3.0f;

            // Make valleys more common by pushing down higher areas
            if (height > 0)
            {
              height *= 0.8f; // Reduce positive heights
            }
            else
            {
              height *= 1.2f; // Exaggerate negative heights
            }

            height -= 8.0f; // Lower the base level
            float density = worldPos.y - height;
            chunks[x][z].chunk.voxels[vx][vy][vz].density = density;

            // Track height range
            if (-height > chunks[x][z].maxHeight)
              chunks[x][z].maxHeight = -height;
            if (-height < chunks[x][z].minHeight)
              chunks[x][z].minHeight = -height;
          }
        }
      }

      // Update global height range
      if (chunks[x][z].maxHeight > globalMaxHeight)
        globalMaxHeight = chunks[x][z].maxHeight;
      if (chunks[x][z].minHeight < globalMinHeight)
        globalMinHeight = chunks[x][z].minHeight;

      // Generate mesh for this chunk
      chunks[x][z].mesh = GenerateChunkMesh(&chunks[x][z].chunk);
      chunks[x][z].initialized = true;
    }
  }

  // Material and shader setup
  Shader lightingShader = LoadShader("resources/shaders/lighting_shader.vs", "resources/shaders/lighting_shader.fs");
  Material material = LoadMaterialDefault();
  material.shader = lightingShader;

  // Set shader locations
  Vector3 lowColor = (Vector3){0.1f, 0.4f, 0.05f};  // Brighter green for valleys
  Vector3 midColor = (Vector3){0.25f, 0.5f, 0.1f};  // Even more vibrant green for hills
  Vector3 highColor = (Vector3){0.3f, 0.25f, 0.1f}; // Less brown for peaks

  SetShaderValue(material.shader, GetShaderLocation(material.shader, "lowColor"), (float[3]){lowColor.x, lowColor.y, lowColor.z}, SHADER_UNIFORM_VEC3);
  SetShaderValue(material.shader, GetShaderLocation(material.shader, "midColor"), (float[3]){midColor.x, midColor.y, midColor.z}, SHADER_UNIFORM_VEC3);
  SetShaderValue(material.shader, GetShaderLocation(material.shader, "highColor"), (float[3]){highColor.x, highColor.y, highColor.z}, SHADER_UNIFORM_VEC3);

  // Adjust height range to make color transitions more natural
  float heightRange = globalMaxHeight - globalMinHeight;
  float adjustedMin = globalMinHeight + (heightRange * 0.05f); // Start green colors very early
  float adjustedMax = globalMaxHeight - (heightRange * 0.4f);  // End brown colors much lower

  SetShaderValue(material.shader, GetShaderLocation(material.shader, "minHeight"), (float[1]){adjustedMin}, SHADER_UNIFORM_FLOAT);
  SetShaderValue(material.shader, GetShaderLocation(material.shader, "maxHeight"), (float[1]){adjustedMax}, SHADER_UNIFORM_FLOAT);
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

    // Draw all chunk meshes
    for (int x = 0; x < CHUNKS_X; x++)
    {
      for (int z = 0; z < CHUNKS_Z; z++)
      {
        if (chunks[x][z].initialized)
        {
          Matrix transform = MatrixTranslate(
              chunks[x][z].chunk.position.x,
              chunks[x][z].chunk.position.y,
              chunks[x][z].chunk.position.z);
          DrawMesh(chunks[x][z].mesh, material, transform);
        }
      }
    }

    EndMode3D();

    // Draw FPS
    DrawFPS(10, 10);
    EndDrawing();

    // In the draw loop, update camera position:
    SetShaderValue(material.shader, GetShaderLocation(material.shader, "viewPos"), (float[3]){camera.position.x, camera.position.y, camera.position.z}, SHADER_UNIFORM_VEC3);
  }

  // Cleanup - unload all chunk meshes
  for (int x = 0; x < CHUNKS_X; x++)
  {
    for (int z = 0; z < CHUNKS_Z; z++)
    {
      if (chunks[x][z].initialized)
      {
        UnloadMesh(chunks[x][z].mesh);
      }
    }
  }

  CloseWindow();
  return EXIT_SUCCESS;
}