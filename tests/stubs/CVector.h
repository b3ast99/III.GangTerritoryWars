#pragma once
// Stub: minimal CVector for unit tests. Only x/y/z needed.
struct CVector {
    float x, y, z;
    CVector() : x(0.f), y(0.f), z(0.f) {}
    CVector(float x, float y, float z) : x(x), y(y), z(z) {}
};
