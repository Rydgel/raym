#include "render.h"
#include "raymath.h"
#include "rlgl.h"
#include "chunk.h"
#include <stdlib.h>

#define CROSSHAIR_SIZE 10
#define CROSSHAIR_THICKNESS 2

// Helper function to generate random float between 0 and 1
static float RandomFloat()
{
  return (float)rand() / (float)RAND_MAX;
}

// Helper function to get camera projection matrix
static Matrix GetCameraProjection(Camera camera)
{
  float fovy = 45.0f;
  float aspect = (float)GetScreenWidth() / (float)GetScreenHeight();
  return MatrixPerspective(fovy * DEG2RAD, aspect, 0.1f, 1000.0f);
}

void DrawCrosshair(int screenWidth, int screenHeight, Color color)
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

void InitializeShader(Shader *shader)
{
  // Define more varied terrain colors for different biome-like areas
  Vector3 deepWaterColor = (Vector3){0.05f, 0.1f, 0.3f};   // Deep blue for underwater areas
  Vector3 shallowWaterColor = (Vector3){0.1f, 0.3f, 0.4f}; // Lighter blue for shallow water
  Vector3 sandColor = (Vector3){0.76f, 0.7f, 0.5f};        // Sand/beach color
  Vector3 grassColor = (Vector3){0.2f, 0.5f, 0.15f};       // Vibrant grass color
  Vector3 forestColor = (Vector3){0.1f, 0.35f, 0.05f};     // Darker green for forests
  Vector3 rockColor = (Vector3){0.5f, 0.45f, 0.4f};        // Gray-brown for rocky areas
  Vector3 snowColor = (Vector3){0.9f, 0.9f, 0.95f};        // White-blue for snow peaks

  // Send all color values to the shader
  SetShaderValue(*shader, GetShaderLocation(*shader, "deepWaterColor"),
                 (float[3]){deepWaterColor.x, deepWaterColor.y, deepWaterColor.z}, SHADER_UNIFORM_VEC3);
  SetShaderValue(*shader, GetShaderLocation(*shader, "shallowWaterColor"),
                 (float[3]){shallowWaterColor.x, shallowWaterColor.y, shallowWaterColor.z}, SHADER_UNIFORM_VEC3);
  SetShaderValue(*shader, GetShaderLocation(*shader, "sandColor"),
                 (float[3]){sandColor.x, sandColor.y, sandColor.z}, SHADER_UNIFORM_VEC3);
  SetShaderValue(*shader, GetShaderLocation(*shader, "grassColor"),
                 (float[3]){grassColor.x, grassColor.y, grassColor.z}, SHADER_UNIFORM_VEC3);
  SetShaderValue(*shader, GetShaderLocation(*shader, "forestColor"),
                 (float[3]){forestColor.x, forestColor.y, forestColor.z}, SHADER_UNIFORM_VEC3);
  SetShaderValue(*shader, GetShaderLocation(*shader, "rockColor"),
                 (float[3]){rockColor.x, rockColor.y, rockColor.z}, SHADER_UNIFORM_VEC3);
  SetShaderValue(*shader, GetShaderLocation(*shader, "snowColor"),
                 (float[3]){snowColor.x, snowColor.y, snowColor.z}, SHADER_UNIFORM_VEC3);

  // Height thresholds for different terrain types
  float waterLevel = -10.0f;
  float shallowWaterLevel = -5.0f;
  float sandLevel = -3.0f;
  float grassLevel = 0.0f;
  float forestLevel = 5.0f;
  float rockLevel = 10.0f;
  float snowLevel = 14.0f;

  // Send height thresholds to the shader
  SetShaderValue(*shader, GetShaderLocation(*shader, "waterLevel"), (float[1]){waterLevel}, SHADER_UNIFORM_FLOAT);
  SetShaderValue(*shader, GetShaderLocation(*shader, "shallowWaterLevel"), (float[1]){shallowWaterLevel}, SHADER_UNIFORM_FLOAT);
  SetShaderValue(*shader, GetShaderLocation(*shader, "sandLevel"), (float[1]){sandLevel}, SHADER_UNIFORM_FLOAT);
  SetShaderValue(*shader, GetShaderLocation(*shader, "grassLevel"), (float[1]){grassLevel}, SHADER_UNIFORM_FLOAT);
  SetShaderValue(*shader, GetShaderLocation(*shader, "forestLevel"), (float[1]){forestLevel}, SHADER_UNIFORM_FLOAT);
  SetShaderValue(*shader, GetShaderLocation(*shader, "rockLevel"), (float[1]){rockLevel}, SHADER_UNIFORM_FLOAT);
  SetShaderValue(*shader, GetShaderLocation(*shader, "snowLevel"), (float[1]){snowLevel}, SHADER_UNIFORM_FLOAT);

  // Transition widths for smooth color blending between terrain types
  float blendFactor = 1.2f;
  SetShaderValue(*shader, GetShaderLocation(*shader, "blendFactor"), (float[1]){blendFactor}, SHADER_UNIFORM_FLOAT);

  // Set lighting parameters
  SetShaderValue(*shader, GetShaderLocation(*shader, "lightColor"), (float[3]){1.0f, 1.0f, 1.0f}, SHADER_UNIFORM_VEC3);
  SetShaderValue(*shader, GetShaderLocation(*shader, "lightPos"), (float[3]){50.0f, 50.0f, 50.0f}, SHADER_UNIFORM_VEC3);

  // Add ambient and specular lighting factors
  float ambientStrength = 0.3f;
  float specularStrength = 0.5f;
  float shininess = 32.0f;

  SetShaderValue(*shader, GetShaderLocation(*shader, "ambientStrength"), (float[1]){ambientStrength}, SHADER_UNIFORM_FLOAT);
  SetShaderValue(*shader, GetShaderLocation(*shader, "specularStrength"), (float[1]){specularStrength}, SHADER_UNIFORM_FLOAT);
  SetShaderValue(*shader, GetShaderLocation(*shader, "shininess"), (float[1]){shininess}, SHADER_UNIFORM_FLOAT);
}

RenderContext InitializeRenderContext(int width, int height)
{
  RenderContext context = {0};

  // Initialize G-buffer
  context.gBuffer = LoadRenderTexture(width, height);
  context.ssaoBuffer = LoadRenderTexture(width, height);

  // Load shaders
  context.ssaoShader = LoadShader("resources/shaders/ssao_shader.vs", "resources/shaders/ssao_shader.fs");
  context.lightingShader = LoadShader("resources/shaders/lighting_shader.vs", "resources/shaders/lighting_shader.fs");

  // Initialize SSAO kernel
  context.ssaoKernel = (Vector3 *)malloc(sizeof(Vector3) * SSAO_KERNEL_SIZE);
  for (int i = 0; i < SSAO_KERNEL_SIZE; i++)
  {
    Vector3 sample = {
        RandomFloat() * 2.0f - 1.0f,
        RandomFloat() * 2.0f - 1.0f,
        RandomFloat()};

    // Scale samples so they're more aligned to center of kernel
    float scale = (float)i / SSAO_KERNEL_SIZE;
    scale = 0.1f + scale * scale * (1.0f - 0.1f);
    sample = Vector3Scale(Vector3Normalize(sample), scale);
    context.ssaoKernel[i] = sample;
  }

  // Set SSAO shader uniforms
  SetShaderValue(context.ssaoShader, GetShaderLocation(context.ssaoShader, "screenSize"),
                 (float[2]){(float)width, (float)height}, SHADER_UNIFORM_VEC2);
  SetShaderValue(context.ssaoShader, GetShaderLocation(context.ssaoShader, "radius"),
                 (float[1]){SSAO_RADIUS}, SHADER_UNIFORM_FLOAT);
  SetShaderValue(context.ssaoShader, GetShaderLocation(context.ssaoShader, "samples"),
                 context.ssaoKernel, SHADER_UNIFORM_VEC3);

  // Initialize the lighting shader
  InitializeShader(&context.lightingShader);

  // Initialize more SSAO parameters
  SetShaderValue(context.ssaoShader, GetShaderLocation(context.ssaoShader, "bias"),
                 (float[1]){SSAO_BIAS}, SHADER_UNIFORM_FLOAT);

  // Initialize minimap
  context.minimapTexture = LoadRenderTexture(MINIMAP_SIZE, MINIMAP_SIZE);
  context.minimapInitialized = false;
  context.minimapUpdateCounter = 0;

  return context;
}

void CleanupRenderContext(RenderContext *context)
{
  // Clean up SSAO resources
  UnloadRenderTexture(context->gBuffer);
  UnloadRenderTexture(context->ssaoBuffer);
  UnloadShader(context->ssaoShader);
  UnloadShader(context->lightingShader);
  free(context->ssaoKernel);

  // Clean up minimap resources
  if (context->minimapInitialized)
  {
    UnloadRenderTexture(context->minimapTexture);
  }
}

void RenderSceneWithSSAO(RenderContext *context, Camera camera, Model model)
{
  // 1. Render scene to G-buffer
  BeginTextureMode(context->gBuffer);
  ClearBackground(RAYWHITE);
  BeginMode3D(camera);
  DrawModel(model, (Vector3){0}, 1.0f, WHITE);
  EndMode3D();
  EndTextureMode();

  // 2. Generate SSAO
  BeginTextureMode(context->ssaoBuffer);
  ClearBackground(WHITE);
  BeginShaderMode(context->ssaoShader);
  // Update view-dependent uniforms
  Matrix projection = GetCameraProjection(camera);
  SetShaderValueMatrix(context->ssaoShader, GetShaderLocation(context->ssaoShader, "projection"),
                       projection);

  // Draw full-screen quad with SSAO shader
  DrawTextureRec(context->gBuffer.texture,
                 (Rectangle){0, 0, (float)context->gBuffer.texture.width, (float)-context->gBuffer.texture.height},
                 (Vector2){0, 0}, WHITE);
  EndShaderMode();
  EndTextureMode();

  // 3. Final render with lighting and SSAO
  BeginShaderMode(context->lightingShader);
  // Bind SSAO texture
  SetShaderValueTexture(context->lightingShader,
                        GetShaderLocation(context->lightingShader, "ssaoMap"),
                        context->ssaoBuffer.texture);

  // Draw scene with lighting and SSAO
  BeginMode3D(camera);
  DrawModel(model, (Vector3){0}, 1.0f, WHITE);
  EndMode3D();
  EndShaderMode();
}

void InitializeWaterMesh(RenderContext *context)
{
  // Create a plane mesh for water
  Mesh mesh = GenMeshPlane(WATER_SIZE, WATER_SIZE,
                           WATER_VERTICES_PER_SIDE, WATER_VERTICES_PER_SIDE);

  // Load the water shader
  context->waterShader = LoadShader("resources/shaders/water_shader.vs",
                                    "resources/shaders/water_shader.fs");

  // Get shader locations
  int mvpLoc = GetShaderLocation(context->waterShader, "mvp");
  int modelLoc = GetShaderLocation(context->waterShader, "matModel");
  int timeLoc = GetShaderLocation(context->waterShader, "time");
  int moveFactorLoc = GetShaderLocation(context->waterShader, "moveFactor");
  int lightPosLoc = GetShaderLocation(context->waterShader, "lightPos");
  int viewPosLoc = GetShaderLocation(context->waterShader, "viewPos");
  int lightColorLoc = GetShaderLocation(context->waterShader, "lightColor");
  int waveHeightLoc = GetShaderLocation(context->waterShader, "waveHeight");

  // Set initial uniform values
  float lightPos[3] = {100.0f, 100.0f, 100.0f};
  float lightColor[3] = {1.0f, 1.0f, 0.9f};
  float waveHeight = 5.0f; // Increased wave height
  SetShaderValue(context->waterShader, lightPosLoc, lightPos, SHADER_UNIFORM_VEC3);
  SetShaderValue(context->waterShader, lightColorLoc, lightColor, SHADER_UNIFORM_VEC3);
  SetShaderValue(context->waterShader, waveHeightLoc, &waveHeight, SHADER_UNIFORM_FLOAT);

  // Load water textures
  context->waterNormalMap = LoadTexture("resources/textures/water_normal.png");
  context->waterDuDvMap = LoadTexture("resources/textures/water_dudv.png");
  SetTextureFilter(context->waterNormalMap, TEXTURE_FILTER_BILINEAR);
  SetTextureFilter(context->waterDuDvMap, TEXTURE_FILTER_BILINEAR);

  // Create the water model
  context->waterMesh = LoadModelFromMesh(mesh);
  context->waterMesh.materials[0].shader = context->waterShader;

  // Initialize reflection and refraction buffers
  context->reflectionBuffer = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());
  context->refractionBuffer = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());

  // Initialize water movement factor
  context->waterMoveFactor = 0.0f;
}

void UpdateWater(RenderContext *context, float deltaTime)
{
  static float time = 0.0f;
  time += deltaTime * 3.0f; // Increased animation speed

  // Update water movement factor for wave animation
  context->waterMoveFactor += 0.1f * deltaTime; // Increased movement speed
  if (context->waterMoveFactor >= 1.0f)
  {
    context->waterMoveFactor = 0.0f;
  }

  // Update shader uniforms
  SetShaderValue(context->waterShader, GetShaderLocation(context->waterShader, "moveFactor"),
                 &context->waterMoveFactor, SHADER_UNIFORM_FLOAT);
  SetShaderValue(context->waterShader, GetShaderLocation(context->waterShader, "time"),
                 &time, SHADER_UNIFORM_FLOAT);
}

void RenderWater(RenderContext *context, Camera camera, Model terrain)
{
  // Store current camera position
  Vector3 cameraPos = camera.position;

  // Render reflection (camera below water plane)
  camera.position.y = -camera.position.y + 2.0f * WATER_HEIGHT;
  camera.target.y = -camera.target.y + 2.0f * WATER_HEIGHT;
  camera.up.y = -camera.up.y;

  BeginTextureMode(context->reflectionBuffer);
  ClearBackground(SKYBLUE); // Changed from RAYWHITE to match sky color
  BeginMode3D(camera);
  DrawModel(terrain, (Vector3){0, 0, 0}, 1.0f, WHITE);
  EndMode3D();
  EndTextureMode();

  // Reset camera
  camera.position = cameraPos;
  camera.target.y = -camera.target.y + 2.0f * WATER_HEIGHT;
  camera.up.y = -camera.up.y;

  // Render refraction
  BeginTextureMode(context->refractionBuffer);
  ClearBackground(SKYBLUE); // Changed from RAYWHITE to match sky color
  BeginMode3D(camera);
  DrawModel(terrain, (Vector3){0, 0, 0}, 1.0f, WHITE);
  EndMode3D();
  EndTextureMode();

  // Update view position in shader
  float viewPos[3] = {camera.position.x, camera.position.y, camera.position.z};
  SetShaderValue(context->waterShader, GetShaderLocation(context->waterShader, "viewPos"),
                 viewPos, SHADER_UNIFORM_VEC3);

  // Calculate and set MVP matrix
  Matrix matProjection = MatrixPerspective(camera.fovy * DEG2RAD,
                                           (float)GetScreenWidth() / (float)GetScreenHeight(),
                                           0.1f, 1000.0f);
  Matrix matView = MatrixLookAt(camera.position, camera.target, camera.up);
  Matrix matModel = MatrixTranslate(0.0f, WATER_HEIGHT, 0.0f);
  Matrix mvp = MatrixMultiply(MatrixMultiply(matModel, matView), matProjection);

  SetShaderValueMatrix(context->waterShader, GetShaderLocation(context->waterShader, "mvp"), mvp);
  SetShaderValueMatrix(context->waterShader, GetShaderLocation(context->waterShader, "matModel"), matModel);

  // Bind textures
  SetShaderValueTexture(context->waterShader,
                        GetShaderLocation(context->waterShader, "reflectionTexture"),
                        context->reflectionBuffer.texture);
  SetShaderValueTexture(context->waterShader,
                        GetShaderLocation(context->waterShader, "refractionTexture"),
                        context->refractionBuffer.texture);
  SetShaderValueTexture(context->waterShader,
                        GetShaderLocation(context->waterShader, "normalMap"),
                        context->waterNormalMap);
  SetShaderValueTexture(context->waterShader,
                        GetShaderLocation(context->waterShader, "dudvMap"),
                        context->waterDuDvMap);

  // Set up blending for water transparency
  rlEnableDepthTest();
  BeginBlendMode(BLEND_ALPHA);

  // Draw water plane
  BeginMode3D(camera);
  DrawModel(context->waterMesh, (Vector3){0, WATER_HEIGHT, 0}, 1.0f, WHITE);
  EndMode3D();

  // Reset blend mode
  EndBlendMode();
}

void CleanupWater(RenderContext *context)
{
  UnloadTexture(context->waterNormalMap);
  UnloadTexture(context->waterDuDvMap);
  UnloadShader(context->waterShader);
  UnloadModel(context->waterMesh);
  UnloadRenderTexture(context->reflectionBuffer);
  UnloadRenderTexture(context->refractionBuffer);
}

// Implementation of minimap functions
void GenerateMinimap(RenderContext *context)
{
  extern ChunkData chunks[CHUNKS_X][CHUNKS_Z];

  // Set render target to minimap texture
  BeginTextureMode(context->minimapTexture);
  ClearBackground(BLANK);

  // Draw a black background
  DrawRectangle(0, 0, MINIMAP_SIZE, MINIMAP_SIZE, BLACK);

  // Calculate the minimap bounds
  float worldMinX = chunks[0][0].chunk.position.x;
  float worldMaxX = chunks[CHUNKS_X - 1][CHUNKS_Z - 1].chunk.position.x + CHUNK_SIZE;
  float worldMinZ = chunks[0][0].chunk.position.z;
  float worldMaxZ = chunks[CHUNKS_X - 1][CHUNKS_Z - 1].chunk.position.z + CHUNK_SIZE;

  float worldWidth = worldMaxX - worldMinX;
  float worldDepth = worldMaxZ - worldMinZ;

  // Calculate the minimap scale
  float scaleX = (float)(MINIMAP_SIZE) / worldWidth;
  float scaleZ = (float)(MINIMAP_SIZE) / worldDepth;

  // Draw terrain height map
  for (int x = 0; x < CHUNKS_X; x++)
  {
    for (int z = 0; z < CHUNKS_Z; z++)
    {
      if (!chunks[x][z].initialized)
        continue;

      // Get chunk world position
      Vector3 chunkPos = chunks[x][z].chunk.position;

      // Calculate chunk position on minimap
      int mapX = (int)((chunkPos.x - worldMinX) * scaleX);
      int mapZ = (int)((chunkPos.z - worldMinZ) * scaleZ);
      int mapWidth = (int)(CHUNK_SIZE * scaleX);
      int mapHeight = (int)(CHUNK_SIZE * scaleZ);

      // Get height range for this chunk
      float minHeight = chunks[x][z].minHeight;
      float maxHeight = chunks[x][z].maxHeight;

      // Draw chunk on minimap with color based on height
      for (int xi = 0; xi < mapWidth; xi++)
      {
        for (int zi = 0; zi < mapHeight; zi++)
        {
          // Sample height from corresponding position in chunk
          float sampleX = xi / (float)mapWidth;
          float sampleZ = zi / (float)mapHeight;

          int vx = (int)(sampleX * CHUNK_SIZE);
          int vz = (int)(sampleZ * CHUNK_SIZE);
          vx = Clamp(vx, 0, CHUNK_SIZE - 1);
          vz = Clamp(vz, 0, CHUNK_SIZE - 1);

          // Find surface height at this point
          float height = 0;
          for (int vy = CHUNK_SIZE - 1; vy >= 0; vy--)
          {
            if (chunks[x][z].chunk.voxels[vx][vy][vz].density <= 0)
            {
              height = vy;
              break;
            }
          }

          // Calculate a height-based color
          float heightFactor = (height - minHeight) / (maxHeight - minHeight + 0.1f);
          Color color;

          if (height <= 5)
          {
            // Water (blue)
            color = (Color){30, 50, 150, 255};
          }
          else if (height <= 10)
          {
            // Sand (yellow)
            color = (Color){194, 178, 128, 255};
          }
          else if (height <= 20)
          {
            // Grass (green)
            color = (Color){50, 150, 50, 255};
          }
          else if (height <= 30)
          {
            // Forest (dark green)
            color = (Color){25, 100, 25, 255};
          }
          else if (height <= 40)
          {
            // Rock (brown)
            color = (Color){110, 85, 65, 255};
          }
          else
          {
            // Snow (white)
            color = (Color){220, 220, 255, 255};
          }

          // Draw pixel
          DrawPixel(mapX + xi, mapZ + zi, color);
        }
      }
    }
  }

  EndTextureMode();
  context->minimapInitialized = true;
}

void UpdateMinimap(RenderContext *context, Vector3 playerPos)
{
  // Only regenerate the minimap periodically or when not initialized
  context->minimapUpdateCounter++;
  if (!context->minimapInitialized || context->minimapUpdateCounter >= 60)
  {
    GenerateMinimap(context);
    context->minimapUpdateCounter = 0;
  }
}

void DrawMinimap(RenderContext *context, Vector3 playerPos, float playerAngle, Vector3 lightPos, int screenWidth, int screenHeight)
{
  if (!context->minimapInitialized)
  {
    GenerateMinimap(context);
  }

  // Calculate position for minimap (top-right corner)
  int mapX = screenWidth - MINIMAP_SIZE - 10;
  int mapY = 10;

  // Draw minimap background and border
  DrawRectangle(mapX - MINIMAP_BORDER, mapY - MINIMAP_BORDER,
                MINIMAP_SIZE + MINIMAP_BORDER * 2, MINIMAP_SIZE + MINIMAP_BORDER * 2,
                (Color){30, 30, 30, 200});

  // Draw the minimap texture
  DrawTexture(context->minimapTexture.texture, mapX, mapY, WHITE);

  // Calculate player position on minimap
  extern ChunkData chunks[CHUNKS_X][CHUNKS_Z];
  float worldMinX = chunks[0][0].chunk.position.x;
  float worldMaxX = chunks[CHUNKS_X - 1][CHUNKS_Z - 1].chunk.position.x + CHUNK_SIZE;
  float worldMinZ = chunks[0][0].chunk.position.z;
  float worldMaxZ = chunks[CHUNKS_X - 1][CHUNKS_Z - 1].chunk.position.z + CHUNK_SIZE;

  float worldWidth = worldMaxX - worldMinX;
  float worldDepth = worldMaxZ - worldMinZ;

  // Calculate the minimap scale
  float scaleX = (float)(MINIMAP_SIZE) / worldWidth;
  float scaleZ = (float)(MINIMAP_SIZE) / worldDepth;

  // Calculate player position on minimap
  int playerMapX = mapX + (int)((playerPos.x - worldMinX) * scaleX);
  int playerMapY = mapY + (int)((playerPos.z - worldMinZ) * scaleZ);

  // Draw player marker (with direction indicator)
  int markerSize = 4;

  // Draw player position as a circle
  DrawCircle(playerMapX, playerMapY, markerSize, RED);

  // Draw player direction as a small line
  float dirX = cosf(playerAngle);
  float dirZ = sinf(playerAngle);

  DrawLine(playerMapX, playerMapY,
           playerMapX + (int)(dirX * markerSize * 2),
           playerMapY + (int)(dirZ * markerSize * 2),
           RED);

  // Draw sun/moon position
  if (lightPos.y > 0)
  {
    // Only show on minimap if above horizon
    // Calculate position relative to world center
    Vector3 lightDir = Vector3Normalize(lightPos);
    int lightMapX = mapX + MINIMAP_SIZE / 2 + (int)(lightDir.x * MINIMAP_SIZE / 3);
    int lightMapY = mapY + MINIMAP_SIZE / 2 + (int)(lightDir.z * MINIMAP_SIZE / 3);

    // Only draw if within minimap bounds
    if (lightMapX >= mapX && lightMapX < mapX + MINIMAP_SIZE &&
        lightMapY >= mapY && lightMapY < mapY + MINIMAP_SIZE)
    {
      DrawCircle(lightMapX, lightMapY, 3, YELLOW);
    }
  }

  // Draw minimap label
  DrawText("MAP", mapX + 5, mapY + 5, 10, (Color){200, 200, 200, 255});
}