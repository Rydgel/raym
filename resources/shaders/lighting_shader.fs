#version 330

// Input vertex attributes (from vertex shader)
in vec3 fragPosition;
in vec3 fragNormal;
in vec2 fragTexCoord;

// Input uniform values
uniform vec3 lowColor;    // For valleys
uniform vec3 midColor;    // For hills
uniform vec3 highColor;   // For peaks
uniform float minHeight;
uniform float maxHeight;
uniform vec3 lightPos;
uniform vec3 viewPos;
uniform vec3 lightColor;
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

vec3 getGrassColor(vec2 pos, float height) {
    float n = fbm(pos * 3.0);  // Reduced scale for smoother variation
    vec3 grassDark = vec3(0.2, 0.5, 0.1);
    vec3 grassLight = vec3(0.45, 0.75, 0.2);
    return mix(grassDark, grassLight, n);
}

vec3 getRockColor(vec2 pos, float height) {
    float n = fbm(pos * 4.0);  // Base variation
    float detail = fbm(pos * 8.0) * 0.5;  // Finer detail
    vec3 rockDark = vec3(0.35, 0.25, 0.15);   // Darker brown
    vec3 rockLight = vec3(0.6, 0.45, 0.35);    // Lighter brown
    return mix(rockDark, rockLight, n + detail * 0.3);
}

vec3 getSnowColor(vec2 pos) {
    float n = fbm(pos * 5.0) * 0.15 + 0.85;  // Even more subtle variation for snow
    return vec3(n);
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
    
    // Calculate height factor (0.0 to 1.0)
    float heightFactor = clamp((fragPosition.y - minHeight) / (maxHeight - minHeight), 0.0, 1.0);
    
    // Calculate slope factor
    float slopeFactor = dot(normal, vec3(0.0, 1.0, 0.0));
    
    // Get procedural colors
    vec3 grassTex = getGrassColor(fragPosition.xz, heightFactor);
    vec3 rockTex = getRockColor(fragPosition.xz, heightFactor);
    vec3 snowTex = getSnowColor(fragPosition.xz);
    
    // Blend based on height and slope
    vec3 objectColor;
    if (heightFactor > 0.85) {  // Increased from 0.75 to 0.85
        // Snow only on very high peaks
        float snowBlend = smoothstep(0.85, 0.95, heightFactor) * smoothstep(0.4, 0.8, slopeFactor);  // Steeper slope requirement
        objectColor = mix(rockTex, snowTex, snowBlend);
    } else if (heightFactor > 0.45) {
        // Rock on steep slopes and higher elevations
        float rockBlend = smoothstep(0.45, 0.75, heightFactor) + (1.0 - smoothstep(0.3, 0.6, slopeFactor));
        rockBlend = clamp(rockBlend, 0.0, 1.0);
        objectColor = mix(grassTex, rockTex, rockBlend);
    } else {
        // Grass in lower areas
        objectColor = grassTex;
    }
    
    // Calculate cell-shaded lighting
    vec3 lightDir = normalize(lightPos - fragPosition);
    float diff = max(dot(normal, lightDir), 0.0);
    
    // Quantize diffuse lighting into 3 bands instead of 4 for softer transitions
    diff = cellShade(diff, 3);
    vec3 diffuse = diff * lightColor;
    
    // Get ambient occlusion factor with softer effect
    float ao = texture(ssaoMap, fragTexCoord).r;
    ao = pow(ao, 1.1);  // Slightly increased contrast
    ao = max(0.35, ao);  // Slightly darker minimum AO
    
    // Reduced ambient light
    float ambientStrength = 0.55;  // Decreased from 0.65
    vec3 ambient = ambientStrength * lightColor * ao;
    
    // Softer rim lighting
    float rim = getRimLight(normal, viewDir) * ao;
    vec3 rimColor = vec3(1.0) * rim * 0.2;  // Reduced from 0.25
    
    // Final color with slightly darker result
    vec3 result = (ambient + diffuse * mix(0.9, 1.0, ao)) * objectColor + rimColor;
    finalColor = vec4(result, 1.0);
} 