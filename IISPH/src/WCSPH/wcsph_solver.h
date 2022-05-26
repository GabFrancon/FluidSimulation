#pragma once

#include "vector.h"

#include <glm/vec3.hpp>
#include <glm/exponential.hpp>

#ifndef M_PI
#define M_PI 3.141592
#endif

struct Cell {
    Cell(int _i, int _j) { x = _i; y = _j; }
    int x = 0;
    int y = 0;
};

// SPH Kernel function: cubic spline
class CubicSpline
{
public:
    explicit CubicSpline(const Real h = 1) : _dim(2)
    {
        setSmoothingLen(h);
    }
    void setSmoothingLen(const Real h)
    {
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

    Real f(const Real l) const
    {
        const Real q = l / _h;
        if (q < 1e0) return _c[_dim - 1] * (1e0 - 1.5 * square(q) + 0.75 * cube(q));
        else if (q < 2e0) return _c[_dim - 1] * (0.25 * cube(2e0 - q));
        return 0;
    }
    Real derivativeF(const Real l) const
    {
        const Real q = l / _h;
        if (q <= 1e0) return _gc[_dim - 1] * (-3e0 * q + 2.25 * square(q));
        else if (q < 2e0) return -_gc[_dim - 1] * 0.75 * square(2e0 - q);
        return 0;
    }

    Real W(const Vec2f& rij) const { return f(rij.length()); }
    Vec2f gradW(const Vec2f& rij) const { return gradW(rij, rij.length()); }
    Vec2f gradW(const Vec2f& rij, const Real len) const
    {
        return derivativeF(len) * rij / len;
    }

private:
    unsigned int _dim;
    Real _h, _sr, _c[3], _gc[3];
};


class WCSPHSolver
{
public:
    explicit WCSPHSolver(
        const Real h     = 0.5f ,   // particle spacing
        const Real rho0  = 1e3f ,   // rest density
        const Real nu    = 0.08f,   // kinematic viscosity
        const Real eta   = 0.01f,   // compressibility
        const Real gamma = 7.0f )   // EOS pow factor
    {
        // fluid properties
        _h     = h;
        _rho0  = rho0;
        _nu    = nu;
        _eta   = eta;
        _gamma = gamma;

        // fixed constants
        _dt = 0.0005f;
        _g  = Vec2f(-0.0f, -9.8f);

        // derived properties
        _m0     = _rho0 * _h * _h;
        _c      = std::fabs(_g.y) / _eta;
        _k      = _rho0 * _c * _c / _gamma;
        _kernel = CubicSpline(_h);
    }


    void init(const int gridX, const int gridY, const int fluidWidth, const int fluidHeight);
    void addSolidBox(int bottomX, int bottomY, int topX, int topY);
    void update();

    const inline Index fluidCount() const { return _fluidCount; }
    const inline Index particleCount() const { return _position.size(); }
    const inline Vec2f& position(const Index i) const { return _position[i]; }
    const inline glm::vec3& color(const Index i) const { return _color[i]; }
    const inline int resX() const { return _resX; }
    const inline int resY() const { return _resY; }

private:
    std::vector<Cell> getNeighbourCells(const int i, const int j);  
    Index idx1d(const int i, const int j);

    void buildNeighbor();
    void computeDensity();
    void computePressure();
    void applyBodyForce();
    void applyPressureForce();
    void applyViscousForce();
    void updateVelocity();
    void updatePosition();
    void updateColor();


    /*---------------------------------------CLASS MEMBERS-----------------------------------------------*/

    // smooth kernel
    CubicSpline _kernel;

    // particle data
    std::vector<Vec2f>     _position;
    std::vector<Vec2f>     _velocity;
    std::vector<Vec2f>     _acceleration;
    std::vector<Real>      _pressure;
    std::vector<Real>      _density;
    std::vector<glm::vec3> _color;

    // neigboring structure
    std::vector< std::vector<Index> > _neighborsGrid;

    // visualization
    glm::vec3 wallColor  = { 195 / 255.0f,  50 / 255.0f,  30 / 255.0f };
    glm::vec3 lightColor = { 213 / 255.0f, 240 / 255.0f, 255 / 255.0f };
    glm::vec3 denseColor = {   2 / 255.0f,  73 / 255.0f, 113 / 255.0f };

    // simulation
    int _resX = 0;                // grid resolution on x-axis
    int _resY = 0;                // grid resolution on y-axis
    int _fluidCount = 0;          // number of fluid particles

    // SPH coefficients
    Real _dt;                     // time step
    Real _nu;                     // kinematic viscosity
    Real _eta;                    // compressibility
    Real _rho0;                   // rest density
    Real _h;                      // particle spacing
    Vec2f _g;                     // gravity
    Real _m0;                     // rest mass
    Real _c;                      // speed of sound
    Real _k;                      // EOS coefficient
    Real _gamma;                  // EOS power factor
};