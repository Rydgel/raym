#version 330

// Input vertex attributes
in vec3 vertexPosition;
in vec2 vertexTexCoord;

// Output vertex attributes
out vec2 fragTexCoord;

void main()
{
    // Pass texture coordinates to fragment shader
    fragTexCoord = vertexTexCoord;
    
    // Calculate final vertex position
    gl_Position = vec4(vertexPosition, 1.0);
} 