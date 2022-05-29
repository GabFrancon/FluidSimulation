#pragma once

#include "sph_kernel.h"

#include <glm/vec3.hpp>
#include <glm/exponential.hpp>


class WCSPHsolver
{
public:
    explicit WCSPHsolver(
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
        _g  = Vec2f(0.0f, -9.8f);

        // derived properties
        _m0     = _rho0 * _h * _h;
        _c      = std::fabs(_g.y) / _eta;
        _k      = _rho0 * _c * _c / _gamma;
        _kernel = CubicSpline(_h);
    }


    void init(const int gridX, const int gridY, const int fluidWidth, const int fluidHeight);
    void addSolidBox(int bottomX, int bottomY, int topX, int topY);
    void update();

    const inline Index particleCount() const { return _fluidCount; }
    const inline Vec2f& position(const Index i) const { return _position[i]; }
    const inline glm::vec3& color(const Index i) const { return _color[i]; }
    const inline int resX() const { return _resX; }
    const inline int resY() const { return _resY; }

private:
    std::vector<Cell> getNeighbourCells(const int i, const int j);  
    Index idx1d(const int i, const int j);
    std::vector<Index> getNeighbors(int i);

    void buildNeighbor();
    void computeDensity();
    void computePressure();
    void applyBodyForce();
    void applyPressureForce();
    void applyViscousForce();
    void updateVelocity();
    void updatePosition();
    void resolveCollision();
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

    // walls
    Real _l, _r, _b, _t;
};