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
#define EDIT_RADIUS 3.0f
#define EDIT_STRENGTH 0.5f
#define RAY_STEP 0.1f           // Smaller steps for better precision
#define MAX_RAY_DISTANCE 100.0f // Maximum ray distance
#define CROSSHAIR_SIZE 10       // Size of the crosshair in pixels
#define CROSSHAIR_THICKNESS 2   // Thickness of the crosshair lines

typedef struct
{
  Chunk chunk;
  Mesh mesh;
  bool initialized;
  bool needsUpdate; // Flag to track if mesh needs regeneration
  float minHeight;  // Track height range for this chunk
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

// Helper function to get chunk coordinates from world position
static bool GetChunkCoords(Vector3 worldPos, int *chunkX, int *chunkZ, int *vx, int *vy, int *vz)
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

// Function to modify terrain (optimized)
static void ModifyTerrain(Vector3 position, float radius, float strength)
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

      // Calculate affected voxel range within chunk
      int minX = (int)((position.x - radius - chunkPos.x) / VOXEL_SIZE);
      int maxX = (int)((position.x + radius - chunkPos.x) / VOXEL_SIZE) + 1;
      int minY = (int)((position.y - radius + CHUNK_SIZE / 2) / VOXEL_SIZE);
      int maxY = (int)((position.y + radius + CHUNK_SIZE / 2) / VOXEL_SIZE) + 1;
      int minZ = (int)((position.z - radius - chunkPos.z) / VOXEL_SIZE);
      int maxZ = (int)((position.z + radius - chunkPos.z) / VOXEL_SIZE) + 1;

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
            Vector3 voxelPos = GetWorldPosition(cx, cz, x, y, z);
            float distSq = Vector3DistanceSqr(position, voxelPos);

            if (distSq <= radiusSq)
            {
              float influence = (1.0f - sqrtf(distSq) / radius) * strength;
              chunks[cx][cz].chunk.voxels[x][y][z].density += influence;
              chunkModified = true;
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

// Function to check if a point is inside terrain
static bool IsInsideTerrain(Vector3 pos)
{
  int chunkX, chunkZ, vx, vy, vz;
  if (GetChunkCoords(pos, &chunkX, &chunkZ, &vx, &vy, &vz))
  {
    return chunks[chunkX][chunkZ].chunk.voxels[vx][vy][vz].density <= 0.0f;
  }
  return false;
}

// Function to get density at any world position
static float GetDensityAtPosition(Vector3 pos)
{
  int chunkX, chunkZ, vx, vy, vz;
  if (GetChunkCoords(pos, &chunkX, &chunkZ, &vx, &vy, &vz))
  {
    return chunks[chunkX][chunkZ].chunk.voxels[vx][vy][vz].density;
  }
  return 1000.0f; // Return high density for out of bounds
}

// Function to draw crosshair
static void DrawCrosshair(int screenWidth, int screenHeight, Color color)
{
  int centerX = screenWidth / 2;
  int centerY = screenHeight / 2;

  // Draw horizontal line
  DrawRectangle(centerX - CROSSHAIR_SIZE, centerY - CROSSHAIR_THICKNESS / 2,
                CROSSHAIR_SIZE * 2, CROSSHAIR_THICKNESS, color);

  // Draw vertical line
  DrawRectangle(centerX - CROSSHAIR_THICKNESS / 2, centerY - CROSSHAIR_SIZE,
                CROSSHAIR_THICKNESS, CROSSHAIR_SIZE * 2, color);
}

int main(void)
{
  // Window dimensions
  const int screenWidth = 800;
  const int screenHeight = 600;

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
      // Get the ray from screen center using proper camera projection
      Vector2 screenCenter = {screenWidth / 2.0f, screenHeight / 2.0f};
      Ray ray = GetScreenToWorldRay(screenCenter, camera);

      Vector3 hitPoint = {0};
      bool hit = false;

      // Binary search refinement after initial hit
      float tMin = 0.01f; // Changed from 0.1f to start closer to camera
      float tMax = MAX_RAY_DISTANCE;
      float currentT = tMin;

      // First pass: larger steps to find approximate hit
      while (currentT < tMax && !hit)
      {
        Vector3 pos = Vector3Add(ray.position, Vector3Scale(ray.direction, currentT));
        float density = GetDensityAtPosition(pos);

        if (density <= 0.0f) // We've hit the surface
        {
          hit = true;
          // Binary search refinement with more steps
          float low = currentT - RAY_STEP;
          float high = currentT;
          for (int i = 0; i < 10; i++) // Increased from 8 to 10 refinement steps
          {
            float mid = (low + high) / 2.0f;
            pos = Vector3Add(ray.position, Vector3Scale(ray.direction, mid));
            density = GetDensityAtPosition(pos);

            if (density <= 0.0f)
              high = mid;
            else
              low = mid;
          }
          hitPoint = Vector3Add(ray.position, Vector3Scale(ray.direction, high)); // Use high instead of average
          break;
        }
        currentT += RAY_STEP;
      }

      if (hit)
      {
        float strength = IsMouseButtonDown(MOUSE_BUTTON_LEFT) ? -EDIT_STRENGTH : EDIT_STRENGTH;
        ModifyTerrain(hitPoint, EDIT_RADIUS, strength);

        // Debug visualization of hit point
        DrawSphere(hitPoint, 0.2f, RED);
      }

      // Debug visualization of ray in front of and behind camera
      DrawLine3D(Vector3Add(ray.position, Vector3Scale(ray.direction, -10.0f)),
                 Vector3Add(ray.position, Vector3Scale(ray.direction, 10.0f)),
                 YELLOW);

      // Debug visualization of camera position
      DrawSphere(camera.position, 0.2f, GREEN);
    }

    // Update meshes for modified chunks
    for (int x = 0; x < CHUNKS_X; x++)
    {
      for (int z = 0; z < CHUNKS_Z; z++)
      {
        if (chunks[x][z].needsUpdate)
        {
          UnloadMesh(chunks[x][z].mesh);
          chunks[x][z].mesh = GenerateChunkMesh(&chunks[x][z].chunk);
          UploadMesh(&chunks[x][z].mesh, false);
          chunks[x][z].needsUpdate = false;
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
    DrawText("Left click to dig, Right click to build", 10, 40, 20, WHITE);
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