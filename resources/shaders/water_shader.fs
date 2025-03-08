#version 330

// Input vertex attributes (from vertex shader)
in vec3 fragPosition;
in vec3 fragNormal;
in vec2 fragTexCoord;
in vec4 clipSpace;

// Input uniform values
uniform vec3 viewPos;
uniform vec3 lightPos;
uniform vec3 lightColor;
uniform float time;
uniform sampler2D reflectionTexture;
uniform sampler2D refractionTexture;
uniform sampler2D dudvMap;
uniform sampler2D normalMap;
uniform float moveFactor;

// Output fragment color
out vec4 finalColor;

const float waveStrength = 0.1;         // Increased distortion
const float shineDamper = 40.0;         // Higher shininess for sharper reflections
const float reflectivity = 1.0;         // Maximum reflectivity
const vec3 waterDeepColor = vec3(0.0, 0.2, 0.5);   // Deeper dark blue
const vec3 waterShallowColor = vec3(0.0, 0.5, 0.8); // Brighter blue for shallow areas
const vec3 foamColor = vec3(0.9, 0.95, 1.0);       // White foam color

// Function to create procedural waves using Gerstner waves algorithm
vec3 gerstnerWave(vec2 position, float steepness, float wavelength, float time, vec2 direction) {
    direction = normalize(direction);
    float k = 2.0 * 3.14159 / wavelength;
    float f = k * dot(direction, position) - time;
    float a = steepness / k;
    
    return vec3(
        direction.x * a * cos(f),
        a * sin(f),
        direction.y * a * cos(f)
    );
}

void main()
{
    // Convert clip space to NDC coordinates for reflection/refraction
    vec2 ndc = (clipSpace.xy / clipSpace.w) / 2.0 + 0.5;
    
    // Calculate dynamic wave distortion based on position and time
    vec2 position = fragPosition.xz * 0.05;
    float adjustedTime = time * 0.3;
    
    // Combine multiple waves for more natural look
    vec3 wave1 = gerstnerWave(position, 0.05, 8.0, adjustedTime, vec2(1.0, 0.0));
    vec3 wave2 = gerstnerWave(position, 0.04, 6.0, adjustedTime, vec2(0.0, 1.0));
    vec3 wave3 = gerstnerWave(position, 0.03, 4.0, adjustedTime * 1.5, vec2(0.7, 0.7));
    
    // Combine waves for final displacement
    vec3 waveDisplacement = wave1 + wave2 + wave3;
    
    // Use wave height information to influence distortion
    float waveHeight = waveDisplacement.y;
    
    // Calculate distortion using DuDv map and wave information
    vec2 distortedTexCoords = texture(dudvMap, vec2(fragTexCoord.x + moveFactor, 
                                                   fragTexCoord.y)).rg * 0.15;
    distortedTexCoords = fragTexCoord + vec2(distortedTexCoords.x, distortedTexCoords.y + moveFactor);
    vec2 totalDistortion = (texture(dudvMap, distortedTexCoords).rg * 2.0 - 1.0) * waveStrength;
    
    // Add wave influence to distortion
    totalDistortion += waveDisplacement.xz * 0.01;
    
    // Apply distortion to reflection/refraction coordinates
    vec2 reflectTexCoords = vec2(ndc.x, -ndc.y) + totalDistortion;
    vec2 refractTexCoords = ndc + totalDistortion;
    
    // Clamp coordinates to prevent sampling outside texture
    reflectTexCoords = clamp(reflectTexCoords, 0.001, 0.999);
    refractTexCoords = clamp(refractTexCoords, 0.001, 0.999);
    
    // Sample reflection and refraction textures
    vec4 reflectColor = texture(reflectionTexture, reflectTexCoords);
    vec4 refractColor = texture(refractionTexture, refractTexCoords);
    
    // Calculate normal from normal map with animation
    vec2 normalMapCoords = distortedTexCoords;
    normalMapCoords += waveDisplacement.xz * 0.05; // Add wave influence to normal mapping
    
    // Combine two normal map samples with different scales for more detail
    vec3 normalMap1 = texture(normalMap, normalMapCoords).rgb;
    vec3 normalMap2 = texture(normalMap, normalMapCoords * 2.0).rgb;
    vec3 normalMapMix = normalize(mix(normalMap1, normalMap2, 0.5));
    
    vec3 normal = normalize(vec3(normalMapMix.r * 2.0 - 1.0, 
                               normalMapMix.b * 0.8, 
                               normalMapMix.g * 2.0 - 1.0));
    
    // Add wave influence to normal
    normal = normalize(normal + vec3(waveDisplacement.x, 0.0, waveDisplacement.z) * 0.2);
    
    // Calculate Fresnel effect for dynamic water transparency
    vec3 viewVector = normalize(viewPos - fragPosition);
    float refractiveFactor = dot(viewVector, normal);
    refractiveFactor = pow(refractiveFactor, 0.35); // Adjusted power for better effect
    refractiveFactor = clamp(refractiveFactor, 0.0, 1.0);
    
    // Calculate specular lighting with multiple highlight components
    vec3 lightDir = normalize(lightPos - fragPosition);
    vec3 halfwayDir = normalize(lightDir + viewVector);
    
    // Traditional specular using reflection
    vec3 reflectedLight = reflect(-lightDir, normal);
    float mainSpecular = pow(max(dot(reflectedLight, viewVector), 0.0), shineDamper);
    
    // Blinn-Phong specular for wider, softer highlights
    float blinnSpecular = pow(max(dot(normal, halfwayDir), 0.0), shineDamper * 0.5);
    
    // Combine specular highlights with different colors for more natural look
    vec3 mainHighlight = lightColor * mainSpecular * reflectivity;
    vec3 secondaryHighlight = lightColor * blinnSpecular * reflectivity * 0.3;
    vec3 specularHighlights = mainHighlight + secondaryHighlight;
    
    // Add foam effect at wave peaks
    float foamFactor = max(0.0, waveHeight * 5.0);
    foamFactor = clamp(pow(foamFactor, 3.0), 0.0, 0.3);
    
    // Calculate water depth effect (deeper = darker color)
    float depthFactor = clamp(1.0 - refractiveFactor * 0.5, 0.0, 1.0);
    vec3 waterBaseColor = mix(waterShallowColor, waterDeepColor, depthFactor);
    
    // Mix reflection and refraction with color and foam
    vec3 waterColorMix = mix(refractColor.rgb * waterBaseColor, reflectColor.rgb, refractiveFactor * 0.6);
    
    // Add foam to wave peaks
    waterColorMix = mix(waterColorMix, foamColor, foamFactor);
    
    // Add specular highlights
    waterColorMix += specularHighlights;
    
    // Add subtle blue tint for deep areas and sides
    float edgeFactor = 1.0 - pow(refractiveFactor, 3.0);
    waterColorMix = mix(waterColorMix, waterDeepColor, edgeFactor * 0.3);
    
    // Calculate dynamic transparency based on view angle and wave height
    float alpha = mix(0.9, 0.7, pow(refractiveFactor, 2.0));
    alpha = mix(alpha, 0.98, foamFactor); // Foam is more opaque
    
    // Final color with transparency
    finalColor = vec4(waterColorMix, alpha);
} 