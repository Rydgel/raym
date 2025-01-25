#ifndef PERLIN_H
#define PERLIN_H

float GetPerlinValue(float x, float y, float z);
float GetSimplexValue(float x, float y, float z);
float GetSimplexFractalValue(float x, float y, float z, int octaves, float persistence);

#endif // PERLIN_H