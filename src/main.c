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
#define EDIT_RADIUS 10.0f
#define EDIT_STRENGTH 0.3f
#define RAY_STEP 0.1f
#define MAX_RAY_DISTANCE 100.0f
#define CROSSHAIR_SIZE 10
#define CROSSHAIR_THICKNESS 2
#define MESH_UPDATE_DELAY 0.01f

// Camera settings
#define CAMERA_MOVE_SPEED 30.0f
#define CAMERA_FAST_MOVE_SPEED 100.0f
#define CAMERA_MOUSE_SENSITIVITY 0.003f

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

  // Initialize render context with SSAO
  RenderContext renderContext = InitializeRenderContext(screenWidth, screenHeight);

  // Material setup (simplified as most settings moved to RenderContext)
  Material material = LoadMaterialDefault();
  material.shader = renderContext.lightingShader; // Use the shader from render context

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
            // Make density binary (-1 or 1) for blocky terrain
            density = density <= 0.0f ? -1.0f : 1.0f;
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

      // Generate mesh and create model for this chunk
      chunks[x][z].mesh = GenerateChunkMesh(&chunks[x][z].chunk);
      chunks[x][z].model = LoadModelFromMesh(chunks[x][z].mesh);
      chunks[x][z].model.materials[0] = material;
      chunks[x][z].initialized = true;
    }
  }

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

    // Custom camera update with speed controls
    if (cursorLocked)
    {
      Vector2 mouseDelta = GetMouseDelta();
      Vector3 moveVec = {0};
      float moveSpeed = IsKeyDown(KEY_LEFT_SHIFT) ? CAMERA_FAST_MOVE_SPEED : CAMERA_MOVE_SPEED;
      moveSpeed *= deltaTime;

      // Apply mouse movement
      UpdateCameraPro(&camera,
                      (Vector3){0, 0, 0},
                      (Vector3){mouseDelta.x * CAMERA_MOUSE_SENSITIVITY * RAD2DEG,
                                mouseDelta.y * CAMERA_MOUSE_SENSITIVITY * RAD2DEG,
                                0},
                      0);

      // Calculate movement vector
      if (IsKeyDown(KEY_W))
        moveVec.x += moveSpeed;
      if (IsKeyDown(KEY_S))
        moveVec.x -= moveSpeed;
      if (IsKeyDown(KEY_D))
        moveVec.y += moveSpeed;
      if (IsKeyDown(KEY_A))
        moveVec.y -= moveSpeed;
      if (IsKeyDown(KEY_SPACE))
        moveVec.z += moveSpeed;
      if (IsKeyDown(KEY_LEFT_CONTROL))
        moveVec.z -= moveSpeed;

      // Apply movement
      UpdateCameraPro(&camera, moveVec, (Vector3){0, 0, 0}, 0);
    }

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
          if (isModifying)
          {
            chunks[x][z].updateTimer += deltaTime;
            if (chunks[x][z].updateTimer < MESH_UPDATE_DELAY)
              continue;
          }

          // Update mesh and model
          UnloadMesh(chunks[x][z].mesh);
          chunks[x][z].mesh = GenerateChunkMesh(&chunks[x][z].chunk);

          // Update the model's mesh data directly
          chunks[x][z].model.meshes[0].vertexCount = chunks[x][z].mesh.vertexCount;
          chunks[x][z].model.meshes[0].triangleCount = chunks[x][z].mesh.triangleCount;
          chunks[x][z].model.meshes[0].vertices = chunks[x][z].mesh.vertices;
          chunks[x][z].model.meshes[0].indices = chunks[x][z].mesh.indices;
          chunks[x][z].model.meshes[0].normals = chunks[x][z].mesh.normals;

          // Upload the new mesh data to GPU
          UpdateMeshBuffer(chunks[x][z].model.meshes[0], 0, chunks[x][z].mesh.vertices,
                           chunks[x][z].mesh.vertexCount * 3 * sizeof(float), 0);
          UpdateMeshBuffer(chunks[x][z].model.meshes[0], 1, chunks[x][z].mesh.normals,
                           chunks[x][z].mesh.vertexCount * 3 * sizeof(float), 0);
          UpdateMeshBuffer(chunks[x][z].model.meshes[0], 6, chunks[x][z].mesh.indices,
                           chunks[x][z].mesh.triangleCount * 3 * sizeof(unsigned short), 0);

          chunks[x][z].needsUpdate = false;
          chunks[x][z].updateTimer = 0.0f;
        }
      }
    }

    // Draw
    BeginDrawing();
    ClearBackground(RAYWHITE);

    DrawRectangleGradientV(0, 0, screenWidth, screenHeight, SKYBLUE, DARKBLUE);

    // Draw grid for reference
    BeginMode3D(camera);
    rlEnableDepthMask();
    rlDisableBackfaceCulling();
    rlEnableDepthTest();
    DrawGrid(20, 1.0f);
    EndMode3D();

    // Render all chunks with SSAO
    for (int x = 0; x < CHUNKS_X; x++)
    {
      for (int z = 0; z < CHUNKS_Z; z++)
      {
        if (chunks[x][z].initialized)
        {
          chunks[x][z].model.transform = MatrixTranslate(
              chunks[x][z].chunk.position.x,
              chunks[x][z].chunk.position.y,
              chunks[x][z].chunk.position.z);
          RenderSceneWithSSAO(&renderContext, camera, chunks[x][z].model);
        }
      }
    }

    // Draw UI elements
    DrawCrosshair(screenWidth, screenHeight, WHITE);
    DrawText("Right click to dig, Left click to build", 10, 40, 20, WHITE);
    DrawFPS(10, 10);

    EndDrawing();

    // Update shader view position
    SetShaderValue(renderContext.lightingShader, GetShaderLocation(renderContext.lightingShader, "viewPos"),
                   (float[3]){camera.position.x, camera.position.y, camera.position.z}, SHADER_UNIFORM_VEC3);
  }

  // Cleanup
  CleanupRenderContext(&renderContext);

  // Cleanup - unload all chunk meshes and models
  for (int x = 0; x < CHUNKS_X; x++)
  {
    for (int z = 0; z < CHUNKS_Z; z++)
    {
      if (chunks[x][z].initialized)
      {
        UnloadMesh(chunks[x][z].mesh);
        chunks[x][z].model.materials[0] = (Material){0}; // Clear material before unload
        UnloadModel(chunks[x][z].model);
      }
    }
  }

  // Unload shared material last
  UnloadMaterial(material);

  CloseWindow();
  return EXIT_SUCCESS;
}