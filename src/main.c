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

// Day-night cycle
#define DAY_LENGTH 60.0f   // Length of a full day in seconds
#define MORNING_TIME 0.25f // Morning occurs at 25% of the day
#define NOON_TIME 0.5f     // Noon occurs at 50% of the day
#define EVENING_TIME 0.75f // Evening occurs at 75% of the day

// Weather settings
#define MAX_PARTICLES 1000
#define PARTICLE_AREA_SIZE 100.0f

typedef struct
{
  Vector3 position;
  Vector3 velocity;
  Color color;
  float size;
  float lifetime;
  float age;
  bool active;
} Particle;

extern ChunkData chunks[CHUNKS_X][CHUNKS_Z];

int main(void)
{
  // Window dimensions
  const int screenWidth = 800;
  const int screenHeight = 600;

  // Set logging level to see debug messages
  SetTraceLogLevel(LOG_INFO);

  // Initialize window
  InitWindow(screenWidth, screenHeight, "Enhanced Marching Cubes Demo");

  // Enable mouse cursor lock for camera control
  DisableCursor();
  bool cursorLocked = true;

  // Help screen flag
  bool showHelp = false;

  // Initialize camera
  Camera3D camera = {0};
  camera.position = (Vector3){50.0f, 35.0f, 50.0f};
  camera.target = (Vector3){0.0f, 0.0f, 0.0f};
  camera.up = (Vector3){0.0f, 1.0f, 0.0f};
  camera.fovy = 45.0f;
  camera.projection = CAMERA_PERSPECTIVE;

  // Initialize render context with SSAO
  RenderContext renderContext = InitializeRenderContext(screenWidth, screenHeight);

  // Initialize water
  InitializeWaterMesh(&renderContext);

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
            float amplitude = 14.0f; // Increased amplitude for more height variation
            float height = -5.0f;    // Start below zero for more valleys
            int octaves = 5;         // More octaves for more detail

            // Add large-scale mountain ranges
            float mountainNoise = sin(worldPos.x * 0.01f) * cos(worldPos.z * 0.01f) * 15.0f;

            // Create some ridges and valleys
            float ridgeNoise = fabs(sin(worldPos.x * 0.03f + worldPos.z * 0.02f)) * 10.0f;

            // Basic Perlin-like noise for the terrain
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

            // Create some crater-like formations
            float craterNoise = 0.0f;
            int numCraters = 5;
            for (int c = 0; c < numCraters; c++)
            {
              // Create fixed crater positions
              float craterX = sin(c * 1.1f) * 80.0f;
              float craterZ = cos(c * 1.1f) * 80.0f;
              float craterRadius = 20.0f + c * 4.0f;

              // Calculate distance to crater center
              float dx = worldPos.x - craterX;
              float dz = worldPos.z - craterZ;
              float distToCrater = sqrt(dx * dx + dz * dz);

              // Apply crater depression based on distance
              if (distToCrater < craterRadius)
              {
                float craterDepth = 12.0f;
                float normalizedDist = distToCrater / craterRadius;
                float craterShape = sin(normalizedDist * 3.14159f) * craterDepth;
                craterNoise -= craterShape;
              }
            }

            // Add some random variation for smaller details
            height += sinf(worldPos.x * 0.15f + worldPos.z * 0.2f) * 4.0f;
            height += cosf(worldPos.x * 0.2f + worldPos.z * 0.15f) * 3.0f;

            // Combine all terrain features
            height += mountainNoise;
            height += ridgeNoise * 0.5f;
            height += craterNoise;

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

  // Day-night cycle variables
  float timeOfDay = 0.0f; // 0.0 to 1.0 representing time of day
  bool pauseTime = false; // Pause the day-night cycle
  float timeScale = 1.0f; // Time speed multiplier

  // Weather system - particle system for rain and snow
  Particle particles[MAX_PARTICLES] = {0};
  bool weatherActive = true;
  float weatherIntensity = 1.0f; // Start with maximum intensity
  int weatherType = 1;           // Force rain (1 = rain, 0 = clear, 2 = snow)
  float weatherChangeTimer = 0.0f;

  // Initialize particles
  for (int i = 0; i < MAX_PARTICLES; i++)
  {
    particles[i].active = false;
  }

  // Main game loop
  while (!WindowShouldClose())
  {
    float deltaTime = GetFrameTime(); // Get time between frames

    // Update time of day
    if (!pauseTime)
    {
      timeOfDay += deltaTime / DAY_LENGTH * timeScale;
      if (timeOfDay >= 1.0f)
        timeOfDay -= 1.0f;
    }

    // Update lighting based on time of day
    Vector3 lightPos;
    Vector3 lightColor;

    // Calculate sun position (circular path in sky)
    float sunAngle = (timeOfDay * 2.0f * PI) - PI / 2.0f;
    float sunHeight = sinf(sunAngle);
    float sunDistance = 100.0f;

    lightPos.x = cosf(sunAngle) * sunDistance;
    lightPos.y = sunHeight * sunDistance;
    lightPos.z = 0.0f;

    // Calculate light color based on time of day
    if (timeOfDay < MORNING_TIME)
    {
      // Night to dawn (blue to orange)
      float t = timeOfDay / MORNING_TIME;
      lightColor.x = 0.1f + t * 0.8f; // R: dark to bright
      lightColor.y = 0.1f + t * 0.5f; // G: dark to medium
      lightColor.z = 0.3f - t * 0.1f; // B: medium blue to slight blue
    }
    else if (timeOfDay < NOON_TIME)
    {
      // Morning to noon (orange to white)
      float t = (timeOfDay - MORNING_TIME) / (NOON_TIME - MORNING_TIME);
      lightColor.x = 0.9f + t * 0.1f; // R: bright to full
      lightColor.y = 0.6f + t * 0.4f; // G: medium to full
      lightColor.z = 0.2f + t * 0.8f; // B: slight to full
    }
    else if (timeOfDay < EVENING_TIME)
    {
      // Noon to evening (white to orange)
      float t = (timeOfDay - NOON_TIME) / (EVENING_TIME - NOON_TIME);
      lightColor.x = 1.0f;            // R: stay full
      lightColor.y = 1.0f - t * 0.4f; // G: full to medium
      lightColor.z = 1.0f - t * 0.8f; // B: full to slight
    }
    else
    {
      // Evening to night (orange to blue)
      float t = (timeOfDay - EVENING_TIME) / (1.0f - EVENING_TIME);
      lightColor.x = 1.0f - t * 0.9f; // R: full to dark
      lightColor.y = 0.6f - t * 0.5f; // G: medium to dark
      lightColor.z = 0.2f + t * 0.1f; // B: slight to medium blue
    }

    // Scale light intensity based on sun height (darker at night)
    float intensity = fmaxf(0.05f, fmaxf(0.0f, sunHeight) * 0.8f + 0.2f);
    lightColor = Vector3Scale(lightColor, intensity);

    // Set shader uniforms for lighting
    SetShaderValue(renderContext.lightingShader,
                   GetShaderLocation(renderContext.lightingShader, "lightPos"),
                   (float[3]){lightPos.x, lightPos.y, lightPos.z},
                   SHADER_UNIFORM_VEC3);

    SetShaderValue(renderContext.lightingShader,
                   GetShaderLocation(renderContext.lightingShader, "lightColor"),
                   (float[3]){lightColor.x, lightColor.y, lightColor.z},
                   SHADER_UNIFORM_VEC3);

    // Toggle cursor lock with Tab key
    if (IsKeyPressed(KEY_TAB))
    {
      cursorLocked = !cursorLocked;
      if (cursorLocked)
        DisableCursor();
      else
        EnableCursor();
    }

    // Toggle day-night cycle pause with P key
    if (IsKeyPressed(KEY_P))
    {
      pauseTime = !pauseTime;
    }

    // Adjust time scale with [ and ] keys
    if (IsKeyPressed(KEY_LEFT_BRACKET) && timeScale > 0.1f)
    {
      timeScale -= 0.5f;
    }
    if (IsKeyPressed(KEY_RIGHT_BRACKET))
    {
      timeScale += 0.5f;
    }

    // Toggle weather with K key
    if (IsKeyPressed(KEY_K))
    {
      weatherActive = !weatherActive;

      // If weather is being activated, ensure there's some weather effect
      if (weatherActive && weatherType == 0)
      {
        weatherType = 1;         // Set to rain when activating if currently clear
        weatherIntensity = 0.5f; // Start with medium intensity
      }
    }

    // Cycle through weather types with L key
    if (IsKeyPressed(KEY_L) && weatherActive)
    {
      weatherType = (weatherType + 1) % 3;                 // Cycle between 0 (clear), 1 (rain), and 2 (snow)
      weatherIntensity = (weatherType == 0) ? 0.0f : 0.7f; // Set appropriate intensity
    }

    // Toggle help screen with H key
    if (IsKeyPressed(KEY_H))
    {
      showHelp = !showHelp;
    }

    // Update weather
    weatherChangeTimer += deltaTime;

    // Decide whether to change weather every ~20 seconds
    if (weatherChangeTimer > 20.0f)
    {
      weatherChangeTimer = 0.0f;
      // Randomly change weather
      if (GetRandomValue(0, 100) < 40)
      { // 40% chance to change weather
        int newWeatherType = GetRandomValue(0, 2);
        // Don't change to the same weather type
        if (newWeatherType != weatherType)
        {
          weatherType = newWeatherType;
          weatherIntensity = 0.0f; // Start with low intensity
        }
      }
    }

    // Change weather intensity gradually
    float targetIntensity = 0.0f;
    if (weatherActive)
    {
      if (weatherType == 0)
      {
        targetIntensity = 0.0f; // Clear weather
      }
      else
      {
        targetIntensity = (float)GetRandomValue(50, 100) / 100.0f; // 0.5 to 1.0 intensity for rain/snow
      }
    }

    // Gradually adjust intensity
    if (weatherIntensity < targetIntensity)
    {
      weatherIntensity += deltaTime * 0.2f;
      if (weatherIntensity > targetIntensity)
        weatherIntensity = targetIntensity;
    }
    else if (weatherIntensity > targetIntensity)
    {
      weatherIntensity -= deltaTime * 0.2f;
      if (weatherIntensity < targetIntensity)
        weatherIntensity = targetIntensity;
    }

    // Update existing particles
    for (int i = 0; i < MAX_PARTICLES; i++)
    {
      if (particles[i].active)
      {
        // Update position based on velocity
        particles[i].position = Vector3Add(particles[i].position,
                                           Vector3Scale(particles[i].velocity, deltaTime));

        // Age the particle
        particles[i].age += deltaTime;

        // Check if the particle has reached its lifetime or fallen below ground
        if (particles[i].age >= particles[i].lifetime ||
            particles[i].position.y < 0.0f)
        {
          particles[i].active = false;
        }
      }
    }

    // Create new particles based on weather type and intensity
    if (weatherIntensity > 0.0f && weatherType > 0)
    {
      // Force a higher number of particles per frame for better visibility
      int particlesPerFrame = 20; // Fixed number instead of based on intensity

      for (int i = 0; i < particlesPerFrame; i++)
      {
        // Find an inactive particle
        for (int j = 0; j < MAX_PARTICLES; j++)
        {
          if (!particles[j].active)
          {
            // Position the particle randomly around the camera
            float offsetX = ((float)GetRandomValue(0, 1000) / 1000.0f - 0.5f) * PARTICLE_AREA_SIZE;
            float offsetZ = ((float)GetRandomValue(0, 1000) / 1000.0f - 0.5f) * PARTICLE_AREA_SIZE;
            float height = 30.0f; // Start closer to the camera (was 50.0f)

            particles[j].position = (Vector3){
                camera.position.x + offsetX,
                camera.position.y + height,
                camera.position.z + offsetZ};

            // Set velocity based on weather type
            if (weatherType == 1)
            {
              // Rain - falls straight down quickly
              particles[j].velocity = (Vector3){0.0f, -25.0f, 0.0f};
              particles[j].color = (Color){100, 100, 255, 255}; // Bright blue with full opacity
              particles[j].size = 0.5f;                         // Much larger size
              particles[j].lifetime = 3.0f;                     // Longer lifetime

              // Activate the particle
              particles[j].age = 0.0f;
              particles[j].active = true;
              break;
            }
            else if (weatherType == 2)
            {
              // Snow - falls slowly and drifts
              float driftX = ((float)GetRandomValue(0, 1000) / 1000.0f - 0.5f) * 3.0f;
              float driftZ = ((float)GetRandomValue(0, 1000) / 1000.0f - 0.5f) * 3.0f;
              particles[j].velocity = (Vector3){driftX, -5.0f, driftZ};
              particles[j].color = (Color){230, 230, 255, 200};
              particles[j].size = 0.3f;
              particles[j].lifetime = 10.0f;
            }

            particles[j].age = 0.0f;
            particles[j].active = true;
            break;
          }
        }
      }
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

    // Update water movement
    UpdateWater(&renderContext, deltaTime);

    // Update minimap
    UpdateMinimap(&renderContext, camera.position);

    // Calculate sky colors based on time of day
    Color skyTop, skyBottom;

    if (timeOfDay < MORNING_TIME)
    {
      // Night to dawn
      float t = timeOfDay / MORNING_TIME;
      skyTop = (Color){5 + t * 20, 5 + t * 30, 20 + t * 60, 255};
      skyBottom = (Color){10 + t * 60, 10 + t * 40, 30 + t * 20, 255};
    }
    else if (timeOfDay < NOON_TIME)
    {
      // Morning to noon
      float t = (timeOfDay - MORNING_TIME) / (NOON_TIME - MORNING_TIME);
      skyTop = (Color){25 + t * 75, 35 + t * 125, 80 + t * 140, 255};
      skyBottom = (Color){70 + t * 130, 50 + t * 150, 50 + t * 150, 255};
    }
    else if (timeOfDay < EVENING_TIME)
    {
      // Noon to evening
      float t = (timeOfDay - NOON_TIME) / (EVENING_TIME - NOON_TIME);
      skyTop = (Color){100 - t * 20, 160 - t * 120, 220 - t * 100, 255};
      skyBottom = (Color){200 - t * 100, 200 - t * 100, 200 - t * 50, 255};
    }
    else
    {
      // Evening to night
      float t = (timeOfDay - EVENING_TIME) / (1.0f - EVENING_TIME);
      skyTop = (Color){80 - t * 75, 40 - t * 35, 120 - t * 100, 255};
      skyBottom = (Color){100 - t * 90, 100 - t * 90, 150 - t * 120, 255};
    }

    // Make sky darker if it's raining
    if (weatherType == 1 && weatherIntensity > 0.0f)
    {
      float darkFactor = 1.0f - weatherIntensity * 0.5f;
      skyTop.r = (unsigned char)(skyTop.r * darkFactor);
      skyTop.g = (unsigned char)(skyTop.g * darkFactor);
      skyTop.b = (unsigned char)(skyTop.b * darkFactor);
      skyBottom.r = (unsigned char)(skyBottom.r * darkFactor);
      skyBottom.g = (unsigned char)(skyBottom.g * darkFactor);
      skyBottom.b = (unsigned char)(skyBottom.b * darkFactor);
    }

    // Draw
    BeginDrawing();
    ClearBackground(RAYWHITE);

    // Draw sky gradient
    DrawRectangleGradientV(0, 0, screenWidth, screenHeight, skyTop, skyBottom);

    BeginMode3D(camera);
    // Draw grid for reference
    rlEnableDepthMask();
    rlDisableBackfaceCulling();
    rlEnableDepthTest();
    DrawGrid(20, 1.0f);

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

    // Draw sun/moon in the sky
    Vector3 celestialBodyPos = Vector3Scale(Vector3Normalize(lightPos), 50.0f);
    celestialBodyPos = Vector3Add(camera.position, celestialBodyPos);

    if (sunHeight > 0)
    {
      // Draw sun during day
      DrawSphere(celestialBodyPos, 3.0f, (Color){255, 255, 200, 255});
    }
    else
    {
      // Draw moon during night
      DrawSphere(celestialBodyPos, 2.0f, (Color){220, 220, 255, 255});
    }

    // Render weather particles
    if (weatherIntensity > 0.0f)
    {
      // Set up rendering state for particles
      rlDisableDepthMask();           // Disable depth writes
      rlDisableBackfaceCulling();     // Disable backface culling
      rlSetBlendMode(RL_BLEND_ALPHA); // Enable alpha blending

      for (int i = 0; i < MAX_PARTICLES; i++)
      {
        if (particles[i].active)
        {
          if (weatherType == 1)
          {
            // Rain - draw as cubes instead of lines
            Vector3 rainEnd = Vector3Add(
                particles[i].position,
                Vector3Scale(particles[i].velocity, 0.2f));

            // Draw a cube for each rain particle
            DrawCube(particles[i].position, 0.5f, 0.5f, 0.5f, particles[i].color);
            DrawCube(rainEnd, 0.5f, 0.5f, 0.5f, particles[i].color);
          }
          else if (weatherType == 2)
          {
            // Snow - draw as cubes
            DrawCube(particles[i].position, 0.5f, 0.5f, 0.5f, particles[i].color);
          }
        }
      }

      // Restore rendering state
      rlEnableDepthMask();                        // Re-enable depth writes
      rlEnableBackfaceCulling();                  // Re-enable backface culling
      rlSetBlendMode(RL_BLEND_ALPHA_PREMULTIPLY); // Restore default blend mode
    }

    // Render water last for proper transparency
    RenderWater(&renderContext, camera, chunks[0][0].model);
    EndMode3D();

    // Update minimap
    UpdateMinimap(&renderContext, camera.position);

    // Draw 2D rain effect on screen if weather is active
    if (weatherActive && weatherType == 1 && weatherIntensity > 0.0f)
    {
      // Draw rain as 2D rectangles
      for (int i = 0; i < 200; i++) // Draw 200 rain drops
      {
        // Calculate position based on time
        float x = (float)GetRandomValue(0, screenWidth);
        int timeOffset = (int)(GetTime() * 500) + i * 10;
        float y = (float)(timeOffset % screenHeight);

        // Draw rain drop
        DrawRectangle((int)x, (int)y, 2, 20, (Color){150, 150, 255, 200});
      }
    }
    // Draw 2D snow effect
    else if (weatherActive && weatherType == 2 && weatherIntensity > 0.0f)
    {
      // Draw snow as 2D circles
      for (int i = 0; i < 100; i++) // Draw 100 snowflakes
      {
        // Calculate position based on time with some horizontal drift
        int timeOffset = (int)(GetTime() * 200) + i * 20;
        float xOffset = sinf((float)timeOffset / 100.0f) * 50.0f;
        int x = (int)(((i * 37) % screenWidth) + xOffset) % screenWidth;
        int y = (timeOffset % screenHeight);

        // Draw snowflake
        DrawCircle(x, y, 3, (Color){230, 230, 255, 200});
      }
    }

    // Draw UI elements
    DrawCrosshair(screenWidth, screenHeight, WHITE);
    DrawText("Right click to dig, Left click to build", 10, 40, 20, WHITE);
    DrawText(TextFormat("Time: %s (%.2f)",
                        timeOfDay < 0.25f ? "Night" : timeOfDay < 0.5f ? "Morning"
                                                  : timeOfDay < 0.75f  ? "Day"
                                                                       : "Evening",
                        timeOfDay),
             10, 70, 20, WHITE);
    DrawText(TextFormat("Time Speed: %.1fx %s", timeScale, pauseTime ? "[PAUSED]" : ""), 10, 100, 20, WHITE);
    DrawText("P: Pause time  [ ]: Adjust speed", 10, 130, 20, WHITE);

    // Display weather information
    const char *weatherNames[] = {"Clear", "Rain", "Snow"};
    DrawText(TextFormat("Weather: %s (%.0f%%)",
                        weatherNames[weatherType],
                        weatherIntensity * 100),
             10, 160, 20, WHITE);
    DrawText("K: Toggle weather  L: Change weather type", 10, 190, 20, WHITE);

    // Add debug information
    int activeParticles = 0;
    for (int i = 0; i < MAX_PARTICLES; i++)
    {
      if (particles[i].active)
        activeParticles++;
    }
    DrawText(TextFormat("Active Particles: %d", activeParticles), 10, 220, 20, RED);

    // Draw the minimap
    // Calculate player facing angle from camera direction
    Vector3 forward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
    float playerAngle = atan2f(forward.z, forward.x);
    DrawMinimap(&renderContext, camera.position, playerAngle, lightPos, screenWidth, screenHeight);

    // Display help screen if active
    if (showHelp)
    {
      // Draw semi-transparent background
      DrawRectangle(screenWidth / 2 - 300, screenHeight / 2 - 250, 600, 500, (Color){0, 0, 0, 200});

      // Draw help title
      DrawText("ENHANCED MARCHING CUBES DEMO - CONTROLS", screenWidth / 2 - 280, screenHeight / 2 - 230, 20, WHITE);

      // Draw control information
      int y = screenHeight / 2 - 190;
      int spacing = 25;

      DrawText("Movement:", screenWidth / 2 - 280, y, 18, YELLOW);
      y += spacing;
      DrawText("- WASD: Move camera", screenWidth / 2 - 260, y, 16, WHITE);
      y += spacing;
      DrawText("- SPACE/CTRL: Move up/down", screenWidth / 2 - 260, y, 16, WHITE);
      y += spacing;
      DrawText("- SHIFT: Move faster", screenWidth / 2 - 260, y, 16, WHITE);
      y += spacing;
      DrawText("- TAB: Toggle mouse lock", screenWidth / 2 - 260, y, 16, WHITE);
      y += 2 * spacing;

      DrawText("Terrain Editing:", screenWidth / 2 - 280, y, 18, YELLOW);
      y += spacing;
      DrawText("- LEFT MOUSE: Dig terrain", screenWidth / 2 - 260, y, 16, WHITE);
      y += spacing;
      DrawText("- RIGHT MOUSE: Build terrain", screenWidth / 2 - 260, y, 16, WHITE);
      y += 2 * spacing;

      DrawText("Time & Weather:", screenWidth / 2 - 280, y, 18, YELLOW);
      y += spacing;
      DrawText("- P: Pause time cycle", screenWidth / 2 - 260, y, 16, WHITE);
      y += spacing;
      DrawText("- [ ]: Adjust time speed", screenWidth / 2 - 260, y, 16, WHITE);
      y += spacing;
      DrawText("- K: Toggle weather effects", screenWidth / 2 - 260, y, 16, WHITE);
      y += 2 * spacing;

      DrawText("Features:", screenWidth / 2 - 280, y, 18, YELLOW);
      y += spacing;
      DrawText("- Dynamic day/night cycle", screenWidth / 2 - 260, y, 16, WHITE);
      y += spacing;
      DrawText("- Weather system (rain, snow)", screenWidth / 2 - 260, y, 16, WHITE);
      y += spacing;
      DrawText("- Realistic water with reflections", screenWidth / 2 - 260, y, 16, WHITE);
      y += spacing;
      DrawText("- Minimap navigation", screenWidth / 2 - 260, y, 16, WHITE);
      y += spacing;
      DrawText("- Terrain editing", screenWidth / 2 - 260, y, 16, WHITE);
      y += 2 * spacing;

      DrawText("Press H to close this help screen", screenWidth / 2 - 150, y, 16, GREEN);
    }
    else
    {
      // Show help hint
      DrawText("Press H for help", 10, screenHeight - 30, 20, GREEN);
    }

    DrawFPS(10, 10);

    EndDrawing();

    // Update shader view position
    SetShaderValue(renderContext.lightingShader, GetShaderLocation(renderContext.lightingShader, "viewPos"),
                   (float[3]){camera.position.x, camera.position.y, camera.position.z}, SHADER_UNIFORM_VEC3);
  }

  // Cleanup
  CleanupWater(&renderContext);
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