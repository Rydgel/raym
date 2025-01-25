#include "raylib.h"
#include <stdlib.h>

int main(void)
{
  // Window dimensions
  const int screenWidth = 800;
  const int screenHeight = 600;

  // Initialize window
  InitWindow(screenWidth, screenHeight, "Raymarching Demo");

  // Set target FPS (60)
  SetTargetFPS(60);

  // Main game loop
  while (!WindowShouldClose()) // Detect window close button or ESC key
  {
    // Draw
    BeginDrawing();
    ClearBackground(SKYBLUE); // Clear background with solid color
    EndDrawing();
  }

  // Cleanup
  CloseWindow();

  return EXIT_SUCCESS;
}