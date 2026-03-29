#pragma once

#include <stdint.h>
#include <string>;

struct Texture {
    int width;
    int height;
    int channels;
    uint32_t* pixels;
    std::string path;
};

struct Material {
    uint32_t ambient;
    uint32_t diffuse;
    uint32_t specular;
    uint32_t emissive;

    float Ns;
    float Ni;
    float d;

    Texture* diffuseMap = nullptr;
    Texture* specularMap = nullptr;
    Texture* normalMap = nullptr;
    Texture* emissiveMap = nullptr;

    std::string name;
};

struct MTLLibrary {
    Material* materials;
    int count;
};