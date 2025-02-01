#version 330

// Input vertex attributes
in vec2 fragTexCoord;

// Input uniforms
uniform sampler2D depthMap;
uniform sampler2D normalMap;
uniform vec2 screenSize;
uniform vec3 samples[16];  // Sample kernel
uniform float radius;
uniform mat4 projection;

// Output
out float fragColor;

const float bias = 0.025;

void main()
{
    // Get view space position from depth
    float depth = texture(depthMap, fragTexCoord).r;
    vec3 normal = normalize(texture(normalMap, fragTexCoord).rgb * 2.0 - 1.0);
    
    // Reconstruct position from depth
    vec2 texelSize = 1.0 / screenSize;
    vec3 position = vec3(fragTexCoord, depth);
    
    // Calculate occlusion factor with increased intensity
    float occlusion = 0.0;
    float totalWeight = 0.0;
    
    for(int i = 0; i < 16; i++)
    {
        // Get sample position with increased radius
        vec3 samplePos = position + (samples[i] * radius * 2.0);  // Doubled radius for more pronounced effect
        
        // Project sample position
        vec4 offset = vec4(samplePos, 1.0);
        offset = projection * offset;
        offset.xyz /= offset.w;
        offset.xyz = offset.xyz * 0.5 + 0.5;
        
        // Get sample depth
        float sampleDepth = texture(depthMap, offset.xy).r;
        
        // Improved range check with smoother falloff
        float rangeCheck = smoothstep(0.0, 1.0, radius / abs(position.z - sampleDepth));
        
        // Calculate angle between normal and sample vector
        vec3 sampleDir = normalize(samplePos - position);
        float normalCheck = max(dot(normal, sampleDir), 0.0);
        
        // Accumulate weighted occlusion
        float weight = rangeCheck * normalCheck;
        occlusion += (sampleDepth >= samplePos.z + bias ? 1.0 : 0.0) * weight;
        totalWeight += weight;
    }
    
    // Normalize and enhance the occlusion effect
    occlusion = occlusion / max(totalWeight, 0.00001);
    occlusion = pow(1.0 - occlusion, 2.0);  // Increased contrast
    
    // Output enhanced occlusion factor
    fragColor = occlusion;
} 