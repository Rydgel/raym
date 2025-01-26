#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include "chunk.h"
#include "marching_cubes.h"
#include "terrain.h"
#include "render.h"
#include <stdlib.h>
#include <math.h>

// Add color gradient settings
#define LOW_HEIGHT -15.0f
#define HIGH_HEIGHT 15.0f
#define EDIT_RADIUS 15.0f
#define EDIT_STRENGTH 1.0f
#define RAY_STEP 0.1f
#define MAX_RAY_DISTANCE 100.0f
#define CROSSHAIR_SIZE 10
#define CROSSHAIR_THICKNESS 2
#define MESH_UPDATE_DELAY 0.01f

extern ChunkData chunks[CHUNKS_X][CHUNKS_Z];

int main(void)
{
  // Window dimensions
  const int screenWidth = 800;
  const int screenHeight = 600;

  // Set logging level to see debug messages
  SetTraceLogLevel(LOG_INFO);

  // Initialize window
  InitWindow(screenWidth, screenHeight, "Marching Cubes Demo");

  // Enable mouse cursor lock for camera control
  DisableCursor();
  bool cursorLocked = true;

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
      chunks[x][z].needsUpdate = false;
      chunks[x][z].updateTimer = 0.0f;
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

  // Main game loop
  while (!WindowShouldClose())
  {
    float deltaTime = GetFrameTime(); // Get time between frames

    // Toggle cursor lock with Tab key
    if (IsKeyPressed(KEY_TAB))
    {
      cursorLocked = !cursorLocked;
      if (cursorLocked)
        DisableCursor();
      else
        EnableCursor();
    }

    // Update camera only when cursor is locked
    if (cursorLocked)
      UpdateCamera(&camera, CAMERA_FREE);

    // Handle terrain modification
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) || IsMouseButtonDown(MOUSE_BUTTON_RIGHT))
    {
      Vector2 screenCenter = {screenWidth / 2.0f, screenHeight / 2.0f};
      Ray ray = GetScreenToWorldRay(screenCenter, camera);

      Vector3 hitPoint = {0};
      bool hit = false;
      float nearestDistance = MAX_RAY_DISTANCE;

      // Calculate which chunks to check based on ray direction
      Vector3 rayEnd = Vector3Add(ray.position, Vector3Scale(ray.direction, MAX_RAY_DISTANCE));
      int startX = (int)((fminf(ray.position.x, rayEnd.x) - EDIT_RADIUS + ((CHUNKS_X * (CHUNK_SIZE - 1)) / 2.0f)) / (CHUNK_SIZE - 1));
      int endX = (int)((fmaxf(ray.position.x, rayEnd.x) + EDIT_RADIUS + ((CHUNKS_X * (CHUNK_SIZE - 1)) / 2.0f)) / (CHUNK_SIZE - 1));
      int startZ = (int)((fminf(ray.position.z, rayEnd.z) - EDIT_RADIUS + ((CHUNKS_Z * (CHUNK_SIZE - 1)) / 2.0f)) / (CHUNK_SIZE - 1));
      int endZ = (int)((fmaxf(ray.position.z, rayEnd.z) + EDIT_RADIUS + ((CHUNKS_Z * (CHUNK_SIZE - 1)) / 2.0f)) / (CHUNK_SIZE - 1));

      // Clamp to valid chunk range
      startX = startX < 0 ? 0 : startX;
      endX = endX >= CHUNKS_X ? CHUNKS_X - 1 : endX;
      startZ = startZ < 0 ? 0 : startZ;
      endZ = endZ >= CHUNKS_Z ? CHUNKS_Z - 1 : endZ;

      // Only check chunks that the ray might intersect
      for (int x = startX; x <= endX; x++)
      {
        for (int z = startZ; z <= endZ; z++)
        {
          if (!chunks[x][z].initialized)
            continue;

          Matrix transform = MatrixTranslate(
              chunks[x][z].chunk.position.x,
              chunks[x][z].chunk.position.y,
              chunks[x][z].chunk.position.z);

          RayCollision collision = GetRayCollisionMesh(ray, chunks[x][z].mesh, transform);

          if (collision.hit && collision.distance < nearestDistance)
          {
            hit = true;
            nearestDistance = collision.distance;
            hitPoint = collision.point;
          }
        }
      }

      if (hit)
      {
        float strength = IsMouseButtonDown(MOUSE_BUTTON_LEFT) ? -EDIT_STRENGTH : EDIT_STRENGTH;
        ModifyTerrain(hitPoint, EDIT_RADIUS, strength);
      }
    }

    // Update meshes for modified chunks
    bool isModifying = IsMouseButtonDown(MOUSE_BUTTON_LEFT) || IsMouseButtonDown(MOUSE_BUTTON_RIGHT);

    for (int x = 0; x < CHUNKS_X; x++)
    {
      for (int z = 0; z < CHUNKS_Z; z++)
      {
        if (chunks[x][z].needsUpdate)
        {
          // If we're still modifying, only update after delay
          if (isModifying)
          {
            chunks[x][z].updateTimer += deltaTime;
            if (chunks[x][z].updateTimer < MESH_UPDATE_DELAY)
              continue;
          }

          UnloadMesh(chunks[x][z].mesh);
          chunks[x][z].mesh = GenerateChunkMesh(&chunks[x][z].chunk);
          UploadMesh(&chunks[x][z].mesh, false);
          chunks[x][z].needsUpdate = false;
          chunks[x][z].updateTimer = 0.0f;
        }
      }
    }

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

    // Draw crosshair
    DrawCrosshair(screenWidth, screenHeight, WHITE);

    // Draw UI with corrected text
    DrawText("Right click to dig, Left click to build", 10, 40, 20, WHITE);
    DrawFPS(10, 10);
    EndDrawing();

    // Update shader view position
    SetShaderValue(material.shader, GetShaderLocation(material.shader, "viewPos"),
                   (float[3]){camera.position.x, camera.position.y, camera.position.z}, SHADER_UNIFORM_VEC3);
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