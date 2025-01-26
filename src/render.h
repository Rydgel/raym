#ifndef RENDER_H
#define RENDER_H

#include "raylib.h"

// Function declarations for rendering
void DrawCrosshair(int screenWidth, int screenHeight, Color color);
void InitializeShader(Shader *shader);

#endif // RENDER_H