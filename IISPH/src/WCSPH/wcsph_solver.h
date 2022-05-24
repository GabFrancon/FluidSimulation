#pragma once

#include <glm/vec3.hpp>
#include <glm/exponential.hpp>

#define _USE_MATH_DEFINES

// local
#include "vector.h"

// std
#include <cmath>

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


    void init(const int gridX, const int gridY, const int fluidWidth, const int fluidHeight) {
        // - init a fluid mass of size (fluidWidth, fluitHeight)
        // - create a grid of gridX * gridY cells starting from bottom left corner of the fluid
        // - each cell of the grid is sampled with 2x2 particles
        _resX = gridX;
        _resY = gridY;

        // sample fluid mass
        _position.clear();
        for (int j = 1 ; j < fluidHeight + 1 ; j++) {
            for (int i = 1 ; i < fluidWidth + 1 ; i++) {
                _position.push_back(Vec2f(i + 0.25, j + 0.25));
                _position.push_back(Vec2f(i + 0.75, j + 0.25));
                _position.push_back(Vec2f(i + 0.25, j + 0.75));
                _position.push_back(Vec2f(i + 0.75, j + 0.75));
            }
        }
        _fluidCount = _position.size();

        // sample walls
        addSolidBox(0, 0, _resX, _resY);
        addSolidBox(35, 4, 45, 12);

        // color particles
        _color.clear();
        for (Index i = 0; i < _fluidCount; i++)
            _color.push_back(denseColor);
        for (Index i = _fluidCount; i < _position.size(); i++)
            _color.push_back(wallColor);

        // init other particle quantities
        _velocity = std::vector<Vec2f>(_position.size(), Vec2f(0, 0));
        _acceleration = std::vector<Vec2f>(_position.size(), Vec2f(0, 0));
        _pressure = std::vector<Real>(_position.size(), 0);
        _density = std::vector<Real>(_position.size(), 0);
        _neighborsGrid = std::vector<std::vector<Index>>(resX() * (size_t)resY(), std::vector<Index>());
    }

    void addSolidBox(int bottomX, int bottomY, int topX, int topY) {
        for (int i = bottomX; i < topX; i++) {
            _position.push_back(Vec2f(i + 0.25, bottomY + 0.25));
            _position.push_back(Vec2f(i + 0.75, bottomY + 0.25));
            _position.push_back(Vec2f(i + 0.25, bottomY + 0.75));
            _position.push_back(Vec2f(i + 0.75, bottomY + 0.75));
        }
        for (int i = bottomX; i < topX; i++) {
            _position.push_back(Vec2f(i + 0.25, topY - 0.25));
            _position.push_back(Vec2f(i + 0.75, topY - 0.25));
            _position.push_back(Vec2f(i + 0.25, topY - 0.75));
            _position.push_back(Vec2f(i + 0.75, topY - 0.75));
        }
        for (int j = bottomY + 1; j < topY - 1; j++) {
            _position.push_back(Vec2f(bottomX + 0.25, j + 0.25));
            _position.push_back(Vec2f(bottomX + 0.75, j + 0.25));
            _position.push_back(Vec2f(bottomX + 0.25, j + 0.75));
            _position.push_back(Vec2f(bottomX + 0.75, j + 0.75));
        }
        for (int j = bottomY + 1; j < topY - 1; j++) {
            _position.push_back(Vec2f(topX - 0.25, j + 0.25));
            _position.push_back(Vec2f(topX - 0.75, j + 0.25));
            _position.push_back(Vec2f(topX - 0.25, j + 0.75));
            _position.push_back(Vec2f(topX - 0.75, j + 0.75));
        }
    }

    void update()
    {
        buildNeighbor();
        computeDensity();
        computePressure();

        _acceleration = std::vector<Vec2f>(_position.size(), Vec2f(0, 0));
        applyBodyForce();
        applyPressureForce();
        applyViscousForce();

        updateVelocity();
        updatePosition();
        updateColor();
    }

    const Index fluidCount() const { return _fluidCount; }
    const Index particleCount() const { return _position.size(); }
    const Vec2f& position(const Index i) const { return _position[i]; }
    const glm::vec3& color(const Index i) const { return _color[i]; }
    const int resX() const { return _resX; }
    const int resY() const { return _resY; }

private:
    std::vector<Cell> getNeighbourCells(const int i, const int j)
    {
        std::vector<Cell> neighbors;
        neighbors.push_back(Cell(i, j));

        if (i > 0)
            neighbors.push_back(Cell(i - 1, j));
        if (i < resX() - 1)
            neighbors.push_back(Cell(i + 1, j));
        if (j > 0)
            neighbors.push_back(Cell(i, j - 1));
        if (j < resY() - 1)
            neighbors.push_back(Cell(i, j + 1));
        if (i > 0 && j > 0)
            neighbors.push_back(Cell(i - 1, j - 1));
        if (i < resX() - 1 && j > 0)
            neighbors.push_back(Cell(i + 1, j - 1));
        if (i > 0 && j < resY() - 1)
            neighbors.push_back(Cell(i - 1, j + 1));
        if (i < resX() - 1 && j < resY() - 1)
            neighbors.push_back(Cell(i + 1, j + 1));

        return neighbors;
    }
    
    Index idx1d(const int i, const int j) { 
        return i + j * resX(); 
    }

    void buildNeighbor()
    {
        for (auto& indices : _neighborsGrid)
            indices.clear();

        for (int i = 0; i < _position.size(); i++)
        {
            Vec2f particle = _position[i];
            int x = std::floor(particle.x);
            int y = std::floor(particle.y);
            _neighborsGrid[idx1d(x, y)].push_back(i);
        }
    }

    void computeDensity()
    {
        #pragma omp parallel for
        for (int i = 0; i < _position.size(); i++)
        {
            Vec2f particle = _position[i];
            int x = std::floor(particle.x);
            int y = std::floor(particle.y);

            _density[i] = 0.0f;

            for (Cell cell : getNeighbourCells(x, y))
            {
                std::vector<Index> neighbors = _neighborsGrid[idx1d(cell.x, cell.y)];
                for (Index &n : neighbors)
                    _density[i] += _m0 * _kernel.W(_position[i] - _position[n]);
            }
        }
    }

    void computePressure()
    {
        #pragma omp parallel for
        for (int i = 0; i < _position.size(); i++)
            _pressure[i] = std::max(0.0f, _k * (std::pow(_density[i] / _rho0, _gamma) - 1));
    }

    void applyBodyForce()
    {
        #pragma omp parallel for
        for (int i = 0; i < _fluidCount; i++)
            _acceleration[i] = _g;
    }

    void applyPressureForce()
    {
        #pragma omp parallel for
        for (int i = 0; i < _fluidCount; i++)
        {
            Vec2f particle = _position[i];
            int x = std::floor(particle.x);
            int y = std::floor(particle.y);

            for (Cell cell : getNeighbourCells(x, y))
            {
                std::vector<Index> neighbors = _neighborsGrid[idx1d(cell.x, cell.y)];
                for (Index &n : neighbors)
                {
                    Vec2f deltaPos = _position[i] - _position[n];
                    if (deltaPos.length() > 0.1)
                        _acceleration[i] -= _m0 * (_pressure[i] / (_density[i] * _density[i]) + _pressure[n] / (_density[n] * _density[n])) * _kernel.gradW(deltaPos);
                }
            }
        }
    }

    void applyViscousForce()
    {
        #pragma omp parallel for
        for (int i = 0; i < _fluidCount; i++)
        {
            Vec2f particle = _position[i];
            int x = std::floor(particle.x);
            int y = std::floor(particle.y);

            for (Cell cell : getNeighbourCells(x, y))
            {
                std::vector<Index> neighbors = _neighborsGrid[idx1d(cell.x, cell.y)];
                for (Index &n : neighbors)
                {
                    Vec2f deltaPos = _position[i] - _position[n];
                    Vec2f deltaVel = _velocity[i] - _velocity[n];
                    if (deltaPos.length() > 0.1)
                        _acceleration[i] += 2 * _nu * (_m0 / _density[n]) * deltaVel * deltaPos * _kernel.gradW(deltaPos) / (deltaPos * deltaPos + 0.01 * _h);
                }
            }
        }
    }

    void updateVelocity()
    {
        #pragma omp parallel for
        for (int i = 0; i < _fluidCount; i++)
            _velocity[i] += _acceleration[i] * _dt;
    }

    void updatePosition()
    {
        #pragma omp parallel for
        for (int i = 0; i < _fluidCount; i++)
            _position[i] += _velocity[i] * _dt;
    }

    void updateColor()
    {
        for (Index i = 0; i < _fluidCount; i++) {
            _color[i].x = lightColor.x + (_density[i] / _rho0) * (denseColor.x - lightColor.x);
            _color[i].y = lightColor.y + (_density[i] / _rho0) * (denseColor.y - lightColor.y);
            _color[i].z = lightColor.z + (_density[i] / _rho0) * (denseColor.z - lightColor.z);
        }
    }


    /*---------------------------------------CLASS MEMBERS-----------------------------------------------*/

    // smooth kernel
    CubicSpline _kernel;

    // particle data
    std::vector<Vec2f> _position;
    std::vector<Vec2f> _velocity;
    std::vector<Vec2f> _acceleration;
    std::vector<Real>  _pressure;
    std::vector<Real>  _density;
    std::vector<glm::vec3> _color;
    std::vector< std::vector<Index> > _neighborsGrid;

    // visualization
    glm::vec3 wallColor  = {  92 / 255.0f,  54 / 255.0f,  29 / 255.0f };
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