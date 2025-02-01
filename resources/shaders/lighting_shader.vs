#version 330

// Input vertex attributes
in vec3 vertexPosition;
in vec3 vertexNormal;
in vec2 vertexTexCoord;

// Input uniform values
uniform mat4 mvp;
uniform mat4 matModel;
uniform mat4 matNormal;

// Output vertex attributes (to fragment shader)
out vec3 fragPosition;
out vec3 fragNormal;
out vec2 fragTexCoord;

void main()
{
    // Calculate fragment position in world space
    fragPosition = vec3(matModel * vec4(vertexPosition, 1.0));
    
    // Calculate fragment normal in world space
    fragNormal = normalize(vec3(matNormal * vec4(vertexNormal, 0.0)));
    
    // Pass texture coordinates to fragment shader
    fragTexCoord = vertexTexCoord;
    
    // Calculate final vertex position
    gl_Position = mvp * vec4(vertexPosition, 1.0);
} 