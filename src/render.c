#include "render.h"
#include "raymath.h"
#include "rlgl.h"
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
  Vector3 lowColor = (Vector3){0.1f, 0.4f, 0.05f};  // Brighter green for valleys
  Vector3 midColor = (Vector3){0.25f, 0.5f, 0.1f};  // Even more vibrant green for hills
  Vector3 highColor = (Vector3){0.3f, 0.25f, 0.1f}; // Less brown for peaks

  SetShaderValue(*shader, GetShaderLocation(*shader, "lowColor"), (float[3]){lowColor.x, lowColor.y, lowColor.z}, SHADER_UNIFORM_VEC3);
  SetShaderValue(*shader, GetShaderLocation(*shader, "midColor"), (float[3]){midColor.x, midColor.y, midColor.z}, SHADER_UNIFORM_VEC3);
  SetShaderValue(*shader, GetShaderLocation(*shader, "highColor"), (float[3]){highColor.x, highColor.y, highColor.z}, SHADER_UNIFORM_VEC3);

  float heightRange = 30.0f;
  float adjustedMin = -15.0f + (heightRange * 0.05f);
  float adjustedMax = 15.0f - (heightRange * 0.4f);

  SetShaderValue(*shader, GetShaderLocation(*shader, "minHeight"), (float[1]){adjustedMin}, SHADER_UNIFORM_FLOAT);
  SetShaderValue(*shader, GetShaderLocation(*shader, "maxHeight"), (float[1]){adjustedMax}, SHADER_UNIFORM_FLOAT);
  SetShaderValue(*shader, GetShaderLocation(*shader, "lightColor"), (float[3]){1.0f, 1.0f, 1.0f}, SHADER_UNIFORM_VEC3);
  SetShaderValue(*shader, GetShaderLocation(*shader, "lightPos"), (float[3]){50.0f, 50.0f, 50.0f}, SHADER_UNIFORM_VEC3);
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

  // Set lighting shader uniforms
  Vector3 lowColor = (Vector3){0.1f, 0.4f, 0.05f};  // Brighter green for valleys
  Vector3 midColor = (Vector3){0.25f, 0.5f, 0.1f};  // Even more vibrant green for hills
  Vector3 highColor = (Vector3){0.3f, 0.25f, 0.1f}; // Less brown for peaks

  SetShaderValue(context.lightingShader, GetShaderLocation(context.lightingShader, "lowColor"),
                 (float[3]){lowColor.x, lowColor.y, lowColor.z}, SHADER_UNIFORM_VEC3);
  SetShaderValue(context.lightingShader, GetShaderLocation(context.lightingShader, "midColor"),
                 (float[3]){midColor.x, midColor.y, midColor.z}, SHADER_UNIFORM_VEC3);
  SetShaderValue(context.lightingShader, GetShaderLocation(context.lightingShader, "highColor"),
                 (float[3]){highColor.x, highColor.y, highColor.z}, SHADER_UNIFORM_VEC3);

  float minHeight = -15.0f; // These values match your terrain generation
  float maxHeight = 15.0f;
  SetShaderValue(context.lightingShader, GetShaderLocation(context.lightingShader, "minHeight"),
                 (float[1]){minHeight}, SHADER_UNIFORM_FLOAT);
  SetShaderValue(context.lightingShader, GetShaderLocation(context.lightingShader, "maxHeight"),
                 (float[1]){maxHeight}, SHADER_UNIFORM_FLOAT);

  // Light settings
  SetShaderValue(context.lightingShader, GetShaderLocation(context.lightingShader, "lightColor"),
                 (float[3]){1.0f, 1.0f, 1.0f}, SHADER_UNIFORM_VEC3);
  SetShaderValue(context.lightingShader, GetShaderLocation(context.lightingShader, "lightPos"),
                 (float[3]){50.0f, 50.0f, 50.0f}, SHADER_UNIFORM_VEC3);

  return context;
}

void CleanupRenderContext(RenderContext *context)
{
  UnloadRenderTexture(context->gBuffer);
  UnloadRenderTexture(context->ssaoBuffer);
  UnloadShader(context->ssaoShader);
  UnloadShader(context->lightingShader);
  free(context->ssaoKernel);
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