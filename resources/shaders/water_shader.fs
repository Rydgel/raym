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

const float waveStrength = 0.08;        // Increased distortion
const float shineDamper = 32.0;         // Increased shininess
const float reflectivity = 0.9;         // Increased reflectivity
const vec3 waterColor = vec3(0.0, 0.3, 0.7);  // Deeper blue
const vec3 shadowColor = vec3(0.0, 0.0, 0.1); // Dark blue shadows

void main()
{
    // Convert clip space to NDC coordinates for reflection/refraction
    vec2 ndc = (clipSpace.xy / clipSpace.w) / 2.0 + 0.5;
    
    // Calculate distortion
    vec2 distortedTexCoords = texture(dudvMap, vec2(fragTexCoord.x + moveFactor, 
                                                   fragTexCoord.y)).rg * 0.15;
    distortedTexCoords = fragTexCoord + vec2(distortedTexCoords.x, distortedTexCoords.y + moveFactor);
    vec2 totalDistortion = (texture(dudvMap, distortedTexCoords).rg * 2.0 - 1.0) * waveStrength;
    
    // Apply distortion to reflection/refraction coordinates
    vec2 reflectTexCoords = vec2(ndc.x, -ndc.y) + totalDistortion;
    vec2 refractTexCoords = ndc + totalDistortion;
    
    // Sample reflection and refraction textures
    vec4 reflectColor = texture(reflectionTexture, reflectTexCoords);
    vec4 refractColor = texture(refractionTexture, refractTexCoords);
    
    // Calculate normal from normal map with increased effect
    vec4 normalMapColor = texture(normalMap, distortedTexCoords);
    vec3 normal = normalize(vec3(normalMapColor.r * 2.5 - 1.0, 
                               normalMapColor.b * 0.5, 
                               normalMapColor.g * 2.5 - 1.0));
    
    // Calculate Fresnel effect with sharper transition
    vec3 viewVector = normalize(viewPos - fragPosition);
    float refractiveFactor = dot(viewVector, normal);
    refractiveFactor = pow(refractiveFactor, 0.3); // Sharper fresnel
    refractiveFactor = clamp(refractiveFactor, 0.0, 1.0);
    
    // Calculate specular lighting with higher contrast
    vec3 lightDir = normalize(lightPos - fragPosition);
    vec3 reflectedLight = reflect(-lightDir, normal);
    float specular = max(dot(reflectedLight, viewVector), 0.0);
    specular = pow(specular, shineDamper);
    vec3 specularHighlights = lightColor * specular * reflectivity * 2.0; // Doubled highlights
    
    // Mix reflection and refraction with more contrast
    vec3 waterColorMix = mix(reflectColor.rgb, refractColor.rgb, refractiveFactor);
    waterColorMix = mix(shadowColor, waterColor, refractiveFactor * 0.7); // More contrast
    waterColorMix += specularHighlights;
    
    // Add dark edges based on view angle
    float edge = 1.0 - pow(refractiveFactor, 2.0);
    waterColorMix = mix(waterColorMix, shadowColor, edge * 0.5);
    
    // Add some transparency with less opacity in waves
    float alpha = mix(0.95, 0.85, edge);
    finalColor = vec4(waterColorMix, alpha);
} 