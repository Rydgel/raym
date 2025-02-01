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

// Cell shading functions
float cellShade(float value, int levels) {
    return floor(value * float(levels)) / float(levels - 1);
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
    
    // Three-way color interpolation
    vec3 objectColor;
    if (heightFactor < 0.5) {
        objectColor = mix(lowColor, midColor, heightFactor * 2.0);
    } else {
        objectColor = mix(midColor, highColor, (heightFactor - 0.5) * 2.0);
    }
    
    // Calculate cell-shaded lighting
    vec3 lightDir = normalize(lightPos - fragPosition);
    float diff = max(dot(normal, lightDir), 0.0);
    
    // Quantize diffuse lighting into 4 bands
    diff = cellShade(diff, 4);
    vec3 diffuse = diff * lightColor;
    
    // Get ambient occlusion factor with enhanced effect
    float ao = texture(ssaoMap, fragTexCoord).r;
    ao = pow(ao, 1.5);  // Enhance AO contrast
    
    // Ambient light with enhanced AO
    float ambientStrength = 0.6;  // Increased for better visibility of AO
    vec3 ambient = ambientStrength * lightColor * ao;
    
    // Enhanced rim lighting
    float rim = getRimLight(normal, viewDir) * ao;  // Modulate rim light by AO
    vec3 rimColor = vec3(1.0) * rim * 0.4;  // Increased rim intensity
    
    // Final color with enhanced AO effect
    vec3 result = (ambient + diffuse * mix(0.8, 1.0, ao)) * objectColor + rimColor;
    finalColor = vec4(result, 1.0);
} 