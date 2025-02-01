#ifndef RENDER_H
#define RENDER_H

#include "raylib.h"

// SSAO configuration
#define SSAO_KERNEL_SIZE 16
#define SSAO_RADIUS 1.0f
#define SSAO_BIAS 0.01f

// Water configuration
#define WATER_TILE_SIZE 32.0f
#define WATER_VERTICES_PER_SIDE 16 // Reduced for low-poly look
#define WATER_HEIGHT 5.0f
#define WATER_SIZE 500.0f

typedef struct
{
  RenderTexture2D gBuffer;          // G-buffer for position, normal, and depth
  RenderTexture2D ssaoBuffer;       // SSAO result buffer
  RenderTexture2D reflectionBuffer; // Water reflection
  RenderTexture2D refractionBuffer; // Water refraction
  Shader ssaoShader;                // SSAO shader
  Vector3 *ssaoKernel;              // Sample kernel for SSAO
  Shader lightingShader;            // Main lighting shader
  Shader waterShader;               // Water shader
  Model waterMesh;                  // Water plane mesh
  Texture2D waterNormalMap;         // Normal map for water
  Texture2D waterDuDvMap;           // Distortion map for water
  float waterMoveFactor;            // Water movement factor
} RenderContext;

// Function declarations for rendering
void DrawCrosshair(int screenWidth, int screenHeight, Color color);
void InitializeShader(Shader *shader);
RenderContext InitializeRenderContext(int width, int height);
void CleanupRenderContext(RenderContext *context);
void RenderSceneWithSSAO(RenderContext *context, Camera camera, Model model);

// Water-related functions
void InitializeWaterMesh(RenderContext *context);
void UpdateWater(RenderContext *context, float deltaTime);
void RenderWater(RenderContext *context, Camera camera, Model terrain);
void CleanupWater(RenderContext *context);

#endif // RENDER_H