#pragma once

#include "Util.hpp"
#include <d3d9.h>

struct Vertex {
    float x, y, z;
    float u, v; // Texture coordinates
};

bool create_vertex_buffer(IDirect3DDevice9* dev, Vertex* v, size_t vertexCount, IDirect3DVertexBuffer9** buf);
