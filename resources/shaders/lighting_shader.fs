#version 330

// Input vertex attributes (from vertex shader)
in vec3 fragPosition;
in vec3 fragNormal;

// Input uniform values
uniform vec3 lowColor;    // For valleys
uniform vec3 midColor;    // For hills
uniform vec3 highColor;   // For peaks
uniform float minHeight;
uniform float maxHeight;
uniform vec3 lightPos;
uniform vec3 viewPos;
uniform vec3 lightColor;

// Output fragment color
out vec4 finalColor;

void main()
{
    // Normalize normal vector
    vec3 normal = normalize(fragNormal);
    
    // Calculate height factor (0.0 to 1.0)
    float heightFactor = clamp((fragPosition.y - minHeight) / (maxHeight - minHeight), 0.0, 1.0);
    
    // Three-way color interpolation
    vec3 objectColor;
    if (heightFactor < 0.5) {
        // Interpolate between low and mid colors
        objectColor = mix(lowColor, midColor, heightFactor * 2.0);
    } else {
        // Interpolate between mid and high colors
        objectColor = mix(midColor, highColor, (heightFactor - 0.5) * 2.0);
    }
    
    // Calculate lighting
    vec3 lightDir = normalize(lightPos - fragPosition);
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;
    
    // Ambient light
    float ambientStrength = 0.3;
    vec3 ambient = ambientStrength * lightColor;
    
    // Specular lighting
    float specularStrength = 0.5;
    vec3 viewDir = normalize(viewPos - fragPosition);
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
    vec3 specular = specularStrength * spec * lightColor;
    
    // Final color
    vec3 result = (ambient + diffuse + specular) * objectColor;
    finalColor = vec4(result, 1.0);
} 