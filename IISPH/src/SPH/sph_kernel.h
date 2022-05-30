#pragma once

#include "vector2D.h"
#include "vector3D.h"

#ifndef M_PI
#define M_PI 3.141592
#endif

// SPH Kernel function : cubic spline
class CubicSpline
{
public:
    explicit CubicSpline(const Real h = 1, unsigned int dim = 2) : _dim(dim) {
        setSmoothingLen(h);
    }
    void setSmoothingLen(const Real h) {
        const Real h2 = square(h), h3 = h2 * h;
        _h = h;
        _sr = 2e0 * h;
        _c[0] = 2e0 / (3e0 * h);
        _c[1] = 10e0 / (7e0 * M_PI * h2);
        _c[2] = 1e0 / (M_PI * h3);
        _gc[0] = _c[0] / h;
        _gc[1] = _c[1] / h;
        _gc[2] = _c[2] / h;
    }

    Real smoothingLen() const { return _h; }

    Real supportRadius() const { return _sr; }

    Real f(const Real l) const {
        const Real q = l / _h;
        if (q < 1e0) return _c[_dim - 1] * (1e0 - 1.5 * square(q) + 0.75 * cube(q));
        else if (q < 2e0) return _c[_dim - 1] * (0.25 * cube(2e0 - q));
        return 0;
    }

    Real derivativeF(const Real l) const {
        const Real q = l / _h;
        if (q <= 1e0) return _gc[_dim - 1] * (-3e0 * q + 2.25 * square(q));
        else if (q < 2e0) return -_gc[_dim - 1] * 0.75 * square(2e0 - q);
        return 0;
    }

    Real W(const Vec2f& rij) const { return f(rij.length()); }
    Vec2f gradW(const Vec2f& rij) const { return gradW(rij, rij.length()); }
    Vec2f gradW(const Vec2f& rij, const Real len) const { return derivativeF(len) * rij / len; }

    Real W(const Vec3f& rij) const { return f(rij.length()); }
    Vec3f gradW(const Vec3f& rij) const { return gradW(rij, rij.length()); }
    Vec3f gradW(const Vec3f& rij, const Real len) const { return derivativeF(len) * rij / len; }

private:
    unsigned int _dim;
    Real _h, _sr, _c[3], _gc[3];
};

