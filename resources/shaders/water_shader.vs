#version 330

// Input vertex attributes
in vec3 vertexPosition;
in vec3 vertexNormal;
in vec2 vertexTexCoord;

// Output vertex attributes
out vec3 fragPosition;
out vec3 fragNormal;
out vec2 fragTexCoord;
out vec4 clipSpace;

// Uniform inputs
uniform mat4 mvp;
uniform mat4 matModel;
uniform float time;
uniform float waveHeight;

void main()
{
    // Calculate wave displacement with extremely sharp waves
    float wave1 = sin(vertexPosition.x * 0.3 + time * 1.2) * 
                 cos(vertexPosition.z * 0.3 + time) * waveHeight;
    float wave2 = sin(vertexPosition.x * 0.2 - time * 0.8) * 
                 cos(vertexPosition.z * 0.2 - time * 1.1) * waveHeight * 0.8;
    float wave3 = sin(vertexPosition.x * 0.5 + time * 0.7) * 
                 cos(vertexPosition.z * 0.5 + time * 0.9) * waveHeight * 0.6;
    
    // Apply wave displacement with extremely sharp transitions
    vec3 position = vertexPosition;
    float combinedWave = wave1 + wave2 + wave3;
    position.y += sign(combinedWave) * pow(abs(combinedWave), 0.7) * 1.5; // Even sharper peaks
    
    // Calculate normal based on wave gradient (more exaggerated for stark look)
    vec3 normal = vertexNormal;
    normal.x = -sign(combinedWave) * 2.5;
    normal.z = -sign(combinedWave) * 2.5;
    normal.y *= 0.5; // Flatten normals for more dramatic lighting
    normal = normalize(normal);
    
    // Output to fragment shader
    fragPosition = vec3(matModel * vec4(position, 1.0));
    fragNormal = normalize(vec3(matModel * vec4(normal, 0.0)));
    fragTexCoord = vertexTexCoord;
    clipSpace = mvp * vec4(position, 1.0);
    gl_Position = clipSpace;
} 