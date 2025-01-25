#version 330

in vec3 fragPosition;
in vec3 fragNormal;

out vec4 finalColor;

uniform vec3 lightPos;
uniform vec3 lightColor;
uniform vec3 objectColor;
uniform vec3 viewPos;
uniform float ambientStrength = 0.2;

void main()
{
    // Ambient lighting
    vec3 ambient = ambientStrength * lightColor;
    
    // Diffuse lighting
    vec3 norm = normalize(fragNormal);
    vec3 lightDir = normalize(lightPos - fragPosition);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;
    
    // Combine results
    vec3 result = (ambient + diffuse) * objectColor;
    finalColor = vec4(result, 1.0);
} 