#include "Vertex.hpp"

bool CreateVertexBuffer(IDirect3DDevice9 *dev, Vertex* v, size_t vertexCount, IDirect3DVertexBuffer9 **buf)

{
    const auto vsz = vertexCount * sizeof(Vertex);
    if (auto ret = dev->CreateVertexBuffer(vsz, 0, D3DFVF_XYZ | D3DFVF_TEX1, D3DPOOL_MANAGED, buf, nullptr); FAILED(ret)) {
        Dbg("D3D initialization failed: CreateVertexBuffer");
        return false;
    }

    void* bufmem;
    (*buf)->Lock(0, vsz, &bufmem, 0);
    memcpy(bufmem, v, vsz);
    (*buf)->Unlock();

    return true;
}
