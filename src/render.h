#ifndef RENDER_H
#define RENDER_H

#include "raylib.h"

// SSAO configuration
#define SSAO_KERNEL_SIZE 16
#define SSAO_RADIUS 1.0f // Increased from 0.5 for more pronounced effect
#define SSAO_BIAS 0.01f  // Reduced for more sensitive occlusion detection

typedef struct
{
  RenderTexture2D gBuffer;    // G-buffer for position, normal, and depth
  RenderTexture2D ssaoBuffer; // SSAO result buffer
  Shader ssaoShader;          // SSAO shader
  Vector3 *ssaoKernel;        // Sample kernel for SSAO
  Shader lightingShader;      // Main lighting shader
} RenderContext;

// Function declarations for rendering
void DrawCrosshair(int screenWidth, int screenHeight, Color color);
void InitializeShader(Shader *shader);
RenderContext InitializeRenderContext(int width, int height);
void CleanupRenderContext(RenderContext *context);
void RenderSceneWithSSAO(RenderContext *context, Camera camera, Model model);

#endif // RENDER_H