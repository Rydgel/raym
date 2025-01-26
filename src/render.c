#include "render.h"

#define CROSSHAIR_SIZE 10
#define CROSSHAIR_THICKNESS 2

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

  // Adjust height range to make color transitions more natural
  float heightRange = 30.0f;                          // Approximate range based on terrain generation
  float adjustedMin = -15.0f + (heightRange * 0.05f); // Start green colors very early
  float adjustedMax = 15.0f - (heightRange * 0.4f);   // End brown colors much lower

  SetShaderValue(*shader, GetShaderLocation(*shader, "minHeight"), (float[1]){adjustedMin}, SHADER_UNIFORM_FLOAT);
  SetShaderValue(*shader, GetShaderLocation(*shader, "maxHeight"), (float[1]){adjustedMax}, SHADER_UNIFORM_FLOAT);
  SetShaderValue(*shader, GetShaderLocation(*shader, "lightColor"), (float[3]){1.0f, 1.0f, 1.0f}, SHADER_UNIFORM_VEC3);
  SetShaderValue(*shader, GetShaderLocation(*shader, "lightPos"), (float[3]){50.0f, 50.0f, 50.0f}, SHADER_UNIFORM_VEC3);
}