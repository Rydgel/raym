#version 330

// Input vertex attributes (from vertex shader)
in vec3 fragPosition;
in vec3 fragNormal;
in vec2 fragTexCoord;

// Input uniform values
// Biome color values
uniform vec3 deepWaterColor;
uniform vec3 shallowWaterColor;
uniform vec3 sandColor;
uniform vec3 grassColor;
uniform vec3 forestColor;
uniform vec3 rockColor;
uniform vec3 snowColor;

// Height thresholds for different terrain types
uniform float waterLevel;
uniform float shallowWaterLevel;
uniform float sandLevel;
uniform float grassLevel;
uniform float forestLevel;
uniform float rockLevel;
uniform float snowLevel;

// Blending factor for transitions
uniform float blendFactor;

// Lighting parameters
uniform vec3 lightPos;
uniform vec3 viewPos;
uniform vec3 lightColor;
uniform float ambientStrength;
uniform float specularStrength;
uniform float shininess;
uniform sampler2D ssaoMap;  // SSAO texture

// Output fragment color
out vec4 finalColor;

// Improved noise functions
vec2 grad(vec2 p) {
    float h = fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123);
    float s = sin(h * 6.283185);
    float c = cos(h * 6.283185);
    return vec2(c, s);
}

float noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    
    // Cubic Hermite interpolation
    vec2 u = f * f * (3.0 - 2.0 * f);
    
    float v00 = dot(grad(i + vec2(0, 0)), f - vec2(0, 0));
    float v10 = dot(grad(i + vec2(1, 0)), f - vec2(1, 0));
    float v01 = dot(grad(i + vec2(0, 1)), f - vec2(0, 1));
    float v11 = dot(grad(i + vec2(1, 1)), f - vec2(1, 1));
    
    float v0 = mix(v00, v10, u.x);
    float v1 = mix(v01, v11, u.x);
    return mix(v0, v1, u.y) * 0.5 + 0.5;
}

// FBM (Fractal Brownian Motion) for more natural looking noise
float fbm(vec2 p) {
    float value = 0.0;
    float amplitude = 0.5;
    float frequency = 1.0;
    
    // Add multiple octaves of noise
    for (int i = 0; i < 4; i++) {
        value += amplitude * noise(p * frequency);
        frequency *= 2.0;
        amplitude *= 0.5;
    }
    
    return value;
}

// Add variation to colors based on position
vec3 applyVariation(vec3 baseColor, vec2 pos, float variationAmount) {
    float n = fbm(pos * 3.0);
    return mix(baseColor * (1.0 - variationAmount * 0.5), baseColor * (1.0 + variationAmount * 0.5), n);
}

// Smooth transition between two colors based on a parameter t
vec3 smoothBlend(vec3 color1, vec3 color2, float t, float transitionWidth) {
    float smoothT = smoothstep(0.0, transitionWidth, t);
    return mix(color1, color2, smoothT);
}

// Get terrain color based on height and slope
vec3 getTerrainColor(float height, float slope, vec2 pos) {
    // Add some noise to the height to create more variation
    float heightNoise = fbm(pos * 0.05) * 1.5;
    float adjustedHeight = height + heightNoise;
    
    // Apply slope factor (steeper areas tend to be more rocky)
    float slopeFactor = 1.0 - slope; // 0 for flat terrain, 1 for vertical
    
    // Blend between different terrain types based on height
    vec3 color;
    
    // Apply variation to each base color
    vec3 deepWater = applyVariation(deepWaterColor, pos, 0.1);
    vec3 shallowWater = applyVariation(shallowWaterColor, pos, 0.2);
    vec3 sand = applyVariation(sandColor, pos, 0.3);
    vec3 grass = applyVariation(grassColor, pos, 0.4);
    vec3 forest = applyVariation(forestColor, pos, 0.3);
    vec3 rock = applyVariation(rockColor, pos, 0.5);
    vec3 snow = applyVariation(snowColor, pos, 0.1);

    // Calculate transition zones with smooth blending
    if (adjustedHeight < waterLevel) {
        // Deep water
        color = deepWater;
    } 
    else if (adjustedHeight < shallowWaterLevel) {
        // Transition from deep to shallow water
        float t = (adjustedHeight - waterLevel) / (shallowWaterLevel - waterLevel);
        color = smoothBlend(deepWater, shallowWater, t, blendFactor);
    }
    else if (adjustedHeight < sandLevel) {
        // Transition from shallow water to sand
        float t = (adjustedHeight - shallowWaterLevel) / (sandLevel - shallowWaterLevel);
        color = smoothBlend(shallowWater, sand, t, blendFactor);
    }
    else if (adjustedHeight < grassLevel) {
        // Transition from sand to grass
        float t = (adjustedHeight - sandLevel) / (grassLevel - sandLevel);
        color = smoothBlend(sand, grass, t, blendFactor);
    }
    else if (adjustedHeight < forestLevel) {
        // Transition from grass to forest
        float t = (adjustedHeight - grassLevel) / (forestLevel - grassLevel);
        color = smoothBlend(grass, forest, t, blendFactor);
    }
    else if (adjustedHeight < rockLevel) {
        // Transition from forest to rock
        float t = (adjustedHeight - forestLevel) / (rockLevel - forestLevel);
        
        // Rocky areas appear more on steeper slopes
        float rockInfluence = mix(t, 1.0, slopeFactor * 0.7);
        color = smoothBlend(forest, rock, rockInfluence, blendFactor);
    }
    else if (adjustedHeight < snowLevel) {
        // Transition from rock to snow
        float t = (adjustedHeight - rockLevel) / (snowLevel - rockLevel);
        
        // Snow appears less on very steep slopes
        float snowInfluence = mix(t, t * 0.3, slopeFactor * 0.9);
        color = smoothBlend(rock, snow, snowInfluence, blendFactor);
    }
    else {
        // Snow on peaks
        color = snow;
    }
    
    return color;
}

// Cell shading functions
float cellShade(float value, int levels) {
    float cel = floor(value * float(levels)) / float(levels - 1);
    return max(0.2, cel);  // Slightly darker minimum light level
}

float getRimLight(vec3 normal, vec3 viewDir) {
    float rimDot = 1.0 - dot(viewDir, normal);
    return smoothstep(0.5, 0.8, rimDot);
}

void main()
{
    // Normalize vectors
    vec3 normal = normalize(fragNormal);
    vec3 viewDir = normalize(viewPos - fragPosition);
    
    // Calculate slope factor (1.0 for flat terrain, 0.0 for vertical)
    float slopeFactor = max(0.0, dot(normal, vec3(0.0, 1.0, 0.0)));
    
    // Get terrain color based on height, slope, and position
    vec3 objectColor = getTerrainColor(fragPosition.y, slopeFactor, fragPosition.xz);
    
    // Calculate lighting vectors
    vec3 lightDir = normalize(lightPos - fragPosition);
    float diff = max(dot(normal, lightDir), 0.0);
    
    // Calculate specular reflection (Blinn-Phong)
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), shininess);
    
    // Apply cell shading to diffuse lighting (4 bands for better definition)
    diff = cellShade(diff, 4);
    
    // Calculate final lighting components
    vec3 ambient = ambientStrength * lightColor;
    vec3 diffuse = diff * lightColor;
    vec3 specular = specularStrength * spec * lightColor;
    
    // Add rim lighting effect
    float rim = getRimLight(normal, viewDir);
    vec3 rimColor = vec3(0.8, 0.8, 1.0) * rim * 0.2;
    
    // Get ambient occlusion factor 
    float ao = texture(ssaoMap, fragTexCoord).r;
    
    // Apply ambient occlusion to ambient light
    ambient *= mix(0.5, 1.0, ao);
    
    // Calculate time of day impact (handled by lightColor from application)
    
    // Final color calculation
    vec3 result = (ambient + diffuse) * objectColor + specular + rimColor;
    
    // Apply a subtle atmosphere fog effect for distant objects
    float fogFactor = exp(-0.0005 * length(fragPosition - viewPos));
    vec3 fogColor = lightColor * 0.5; // Adjusts with time of day
    
    result = mix(fogColor, result, fogFactor);
    
    finalColor = vec4(result, 1.0);
} 