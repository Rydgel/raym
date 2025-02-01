import numpy as np
from PIL import Image

def generate_perlin_noise(width, height, scale=10.0, octaves=6, persistence=0.5, lacunarity=2.0):
    def generate_noise(width, height):
        noise = np.zeros((height, width))
        for y in range(height):
            for x in range(width):
                noise[y][x] = np.random.random() * 2.0 - 1.0
        return noise

    def interpolate(a0, a1, w):
        return (a1 - a0) * ((w * (w * 6.0 - 15.0) + 10.0) * w * w * w) + a0

    def generate_smooth_noise(base_noise, octave):
        smooth_noise = np.zeros_like(base_noise)
        period = 1 << octave
        frequency = 1.0 / period
        
        for y in range(height):
            y0 = (y // period) * period
            y1 = (y0 + period) % height
            vertical_blend = (y - y0) * frequency
            
            for x in range(width):
                x0 = (x // period) * period
                x1 = (x0 + period) % width
                horizontal_blend = (x - x0) * frequency
                
                top = interpolate(base_noise[y0][x0], base_noise[y0][x1], horizontal_blend)
                bottom = interpolate(base_noise[y1][x0], base_noise[y1][x1], horizontal_blend)
                smooth_noise[y][x] = interpolate(top, bottom, vertical_blend)
                
        return smooth_noise

    base_noise = generate_noise(width, height)
    perlin = np.zeros((height, width))
    amplitude = 1.0
    total_amplitude = 0.0

    for octave in range(octaves):
        amplitude *= persistence
        total_amplitude += amplitude
        
        period = 1 << octave
        noise = generate_smooth_noise(base_noise, octave)
        perlin += noise * amplitude
    
    perlin /= total_amplitude
    return perlin

def generate_normal_map(height_map, strength=1.0):
    height, width = height_map.shape
    normal_map = np.zeros((height, width, 3))
    
    for y in range(height):
        for x in range(width):
            left = height_map[y, (x - 1) % width]
            right = height_map[y, (x + 1) % width]
            up = height_map[(y - 1) % height, x]
            down = height_map[(y + 1) % height, x]
            
            dx = (right - left) * strength
            dy = (down - up) * strength
            dz = 1.0 / strength
            
            normal = np.array([-dx, -dy, dz])
            normal = normal / np.sqrt(np.sum(normal * normal))
            normal = (normal + 1.0) * 0.5
            
            normal_map[y, x] = normal
    
    return normal_map

def generate_dudv_map(width, height):
    perlin1 = generate_perlin_noise(width, height, scale=5.0, octaves=4)
    perlin2 = generate_perlin_noise(width, height, scale=5.0, octaves=4)
    
    dudv = np.zeros((height, width, 3))
    dudv[:, :, 0] = perlin1
    dudv[:, :, 1] = perlin2
    dudv[:, :, 2] = 0
    
    return (dudv + 1.0) * 0.5

# Generate textures
SIZE = 512

# Generate and save normal map
height_map = generate_perlin_noise(SIZE, SIZE, scale=4.0, octaves=6)
normal_map = generate_normal_map(height_map, strength=2.0)
normal_image = Image.fromarray((normal_map * 255).astype(np.uint8))
normal_image.save("resources/textures/water_normal.png")

# Generate and save dudv map
dudv_map = generate_dudv_map(SIZE, SIZE)
dudv_image = Image.fromarray((dudv_map * 255).astype(np.uint8))
dudv_image.save("resources/textures/water_dudv.png")

print("Water textures generated successfully!") 