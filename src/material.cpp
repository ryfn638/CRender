#include "material.h"
#include <math.h>
#include <iostream>
#include "engine.h"

#include <fstream>    // std::ifstream
#include <sstream>    // std::istringstream
#include <cstdio> 
using namespace std;

static uint32_t packARGB(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255)
{
    return (a << 24) | (r << 16) | (g << 8) | b;
}

void Engine::loadMTL(const std::string& path)
{
    if (this->lib == nullptr) {
        this->lib = new MTLLibrary();
    }
    // first pass — count materials
    std::ifstream file(path);
    if (!file.is_open()) {
        printf("Failed to open MTL file: %s\n", path.c_str());
    }

    int count = 0;
    std::string line;
    while (std::getline(file, line))
        if (line.substr(0, 6) == "newmtl") count++;

    this->lib->materials = new Material[count];
    this->lib->count = count;

    // second pass — fill materials
    file.clear();
    file.seekg(0);

    int index = -1;
    while (std::getline(file, line))
    {
        if (line.empty() || line[0] == '#') continue;

        std::istringstream ss(line);
        std::string token;
        ss >> token;

        if (token == "newmtl")
        {
            index++;
            ss >> this->lib->materials[index].name;
        }
        else if (index < 0) continue;
        else if (token == "Ka") {
            float r, g, b;
            ss >> r >> g >> b;
            this->lib->materials[index].ambient = packARGB(r * 255, g * 255, b * 255);
        }
        else if (token == "Kd") {
            float r, g, b;
            ss >> r >> g >> b;
            this->lib->materials[index].diffuse = packARGB(r * 255, g * 255, b * 255);
        }
        else if (token == "Ks") {
            float r, g, b;
            ss >> r >> g >> b;
            this->lib->materials[index].specular = packARGB(r * 255, g * 255, b * 255);
        }
        else if (token == "Ke") {
            float r, g, b;
            ss >> r >> g >> b;
            this->lib->materials[index].emissive = packARGB(r * 255, g * 255, b * 255);
        }
        else if (token == "Ns") { ss >> this->lib->materials[index].Ns; }
        else if (token == "Ni") { ss >> this->lib->materials[index].Ni; }
        else if (token == "d") { ss >> this->lib->materials[index].d; }
    }

}

void freeMTL(MTLLibrary& lib)
{
    delete[] lib.materials;
    lib.materials = nullptr;
    lib.count = 0;
}