#pragma once

#define _USE_MATH_DEFINES


// local
#include "vector.h"

// std
#include <cmath>

#ifndef M_PI
#define M_PI 3.141592
#endif


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
    Real derivative_f(const Real l) const
    {
        const Real q = l / _h;
        if (q <= 1e0) return _gc[_dim - 1] * (-3e0 * q + 2.25 * square(q));
        else if (q < 2e0) return -_gc[_dim - 1] * 0.75 * square(2e0 - q);
        return 0;
    }

    Real w(const Vec2f& rij) const { return f(rij.length()); }
    Vec2f grad_w(const Vec2f& rij) const { return grad_w(rij, rij.length()); }
    Vec2f grad_w(const Vec2f& rij, const Real len) const
    {
        return derivative_f(len) * rij / len;
    }

private:
    unsigned int _dim;
    Real _h, _sr, _c[3], _gc[3];
};


class WCSPHSolver
{
public:
    explicit WCSPHSolver(
        const Real nu = 0.08, const Real h = 0.5, const Real density = 1e3,
        const Vec2f g = Vec2f(0, -9.8), const Real eta = 0.01, const Real gamma = 7.0) :
        _kernel(h), _nu(nu), _h(h), _d0(density),
        _g(g), _eta(eta), _gamma(gamma)
    {
        _dt = 0.0005;
        _m0 = _d0 * _h * _h;
        _c = std::fabs(_g.y) / _eta;
        _k = _d0 * _c * _c / _gamma;
    }

    // assume an arbitrary grid with the size of res_x*res_y; a fluid mass fill up
    // the size of f_width, f_height; each cell is sampled with 2x2 particles.

    void initScene(const int res_x, const int res_y, const int f_width, const int f_height)
    {
        _pos.clear();

        _resX = res_x;
        _resY = res_y;

        // sample fluid mass
        for (int j = 1 ; j < f_height +1 ; ++j) {
            for (int i = 1 ; i < f_width + 1 ; ++i) {
                _pos.push_back(Vec2f(i + 0.25, j + 0.25));
                _pos.push_back(Vec2f(i + 0.75, j + 0.25));
                _pos.push_back(Vec2f(i + 0.25, j + 0.75));
                _pos.push_back(Vec2f(i + 0.75, j + 0.75));
            }
        }
        _particleCount = _pos.size();

        // sample wallS
        addSolidBox(0, 0, _resY, _resX);
        addSolidBox(4, 35, 13, 45);

        // make sure for the other particle quantities
        _vel = std::vector<Vec2f>(_pos.size(), Vec2f(0, 0));
        _acc = std::vector<Vec2f>(_pos.size(), Vec2f(0, 0));
        _p = std::vector<Real>(_pos.size(), 0);
        _d = std::vector<Real>(_pos.size(), 0);

        // vizualisation
        _col = std::vector<float>(_pos.size() * 4, 1.0);
        _vln = std::vector<float>(_pos.size() * 4, 0.0);
        for (tIndex i = 0; i < _pos.size(); ++i) {
            _col[i * 4 + 0] = _d[i] / _d0;
            _col[i * 4 + 1] = 0.0;
            _col[i * 4 + 2] = 1 - _d[i] / _d0;
        }
        updateColor();

        // neighbors grid
        _pidxInGrid = std::vector<std::vector<tIndex>>((float)resX() * resY(), std::vector<tIndex>());
    }

    void addSolidBox(int bj, int bi, int tj, int ti) {
        // sample wall masses
        for (int i = bi; i < ti; ++i) {
            _pos.push_back(Vec2f(i + 0.25, bj + 0.25));
            _pos.push_back(Vec2f(i + 0.75, bj + 0.25));
            _pos.push_back(Vec2f(i + 0.25, bj + 0.75));
            _pos.push_back(Vec2f(i + 0.75, bj + 0.75));
        }
        for (int i = bi; i < ti; ++i) {
            _pos.push_back(Vec2f(i + 0.25, tj - 0.25));
            _pos.push_back(Vec2f(i + 0.75, tj - 0.25));
            _pos.push_back(Vec2f(i + 0.25, tj - 0.75));
            _pos.push_back(Vec2f(i + 0.75, tj - 0.75));
        }
        for (int j = bj + 1; j < tj - 1; ++j) {
            _pos.push_back(Vec2f(bi + 0.25, j + 0.25));
            _pos.push_back(Vec2f(bi + 0.75, j + 0.25));
            _pos.push_back(Vec2f(bi + 0.25, j + 0.75));
            _pos.push_back(Vec2f(bi + 0.75, j + 0.75));
        }
        for (int j = bj + 1; j < tj - 1; ++j) {
            _pos.push_back(Vec2f(ti - 0.25, j + 0.25));
            _pos.push_back(Vec2f(ti - 0.75, j + 0.25));
            _pos.push_back(Vec2f(ti - 0.25, j + 0.75));
            _pos.push_back(Vec2f(ti - 0.75, j + 0.75));
        }
    }

    void update()
    {
        buildNeighbor();
        computeDensity();
        computePressure();

        _acc = std::vector<Vec2f>(_pos.size(), Vec2f(0, 0));
        applyBodyForce();
        applyPressureForce();
        applyViscousForce();

        updateVelocity();
        updatePosition();

        //updateColor();
        //updateVelLine();
    }

    tIndex particleCount() const { return _particleCount; }
    tIndex allParticleCount() const { return _pos.size(); }

    const Vec2f& position(const tIndex i) const { return _pos[i]; }
    const float& color(const tIndex i) const { return _col[i]; }
    const float& vline(const tIndex i) const { return _vln[i]; }

    int resX() const { return _resX; }
    int resY() const { return _resY; }

    Real equationOfState(const Real d, const Real d0, const Real k, const Real gamma = 7.0) {}

    struct Couple {
        Couple(int _i, int _j) { x = _i; y = _j; }
        int x = 0;
        int y = 0;
    };

    std::vector<Couple> getNeighbours(int i, int j)
    {
        std::vector<Couple> neighbors;
        neighbors.push_back(Couple(i, j));

        if (i > 0)
            neighbors.push_back(Couple(i - 1, j));
        if (i < resX() - 1)
            neighbors.push_back(Couple(i + 1, j));
        if (j > 0)
            neighbors.push_back(Couple(i, j - 1));
        if (j < resY() - 1)
            neighbors.push_back(Couple(i, j + 1));
        if (i > 0 && j > 0)
            neighbors.push_back(Couple(i - 1, j - 1));
        if (i < resX() - 1 && j > 0)
            neighbors.push_back(Couple(i + 1, j - 1));
        if (i > 0 && j < resY() - 1)
            neighbors.push_back(Couple(i - 1, j + 1));
        if (i < resX() - 1 && j < resY() - 1)
            neighbors.push_back(Couple(i + 1, j + 1));

        return neighbors;
    }

private:
    void buildNeighbor()
    {
        // clean all arrays
        for (auto& pid : _pidxInGrid)
            pid.clear();

        // fill up the arrays pos
        for (int i = 0; i < _pos.size(); i++)
        {
            Vec2f particle = _pos[i];
            int x = std::floor(particle.x);
            int y = std::floor(particle.y);
            _pidxInGrid[idx1d(x, y)].push_back(i);
        }
    }

    void computeDensity()
    {
        #pragma omp parallel for
        for (int i = 0; i < _pos.size(); i++)
        {
            Vec2f particle = _pos[i];
            int x = std::floor(particle.x);
            int y = std::floor(particle.y);

            _d[i] = 0.0f;

            for (Couple cell : getNeighbours(x, y))
            {
                std::vector<tIndex> neighbors = _pidxInGrid[idx1d(cell.x, cell.y)];
                for (tIndex &n : neighbors)
                    _d[i] += _m0 * _kernel.w(_pos[i] - _pos[n]);
            }
        }
    }

    void computePressure()
    {
        #pragma omp parallel for
        for (int i = 0; i < _pos.size(); i++)
            _p[i] = std::max(0.0, _k * (std::pow(_d[i] / _d0, 7) - 1));
    }

    void applyBodyForce()
    {
        #pragma omp parallel for
        for (int i = 0; i < _particleCount; i++)
            _acc[i] = _g;
    }

    void applyPressureForce()
    {
        #pragma omp parallel for
        for (int i = 0; i < _particleCount; i++)
        {
            Vec2f particle = _pos[i];
            int x = std::floor(particle.x);
            int y = std::floor(particle.y);

            for (Couple cell : getNeighbours(x, y))
            {
                std::vector<tIndex> neighbors = _pidxInGrid[idx1d(cell.x, cell.y)];
                for (tIndex &n : neighbors)
                {
                    Vec2f deltaPos = _pos[i] - _pos[n];
                    if (deltaPos.length() > 0.1)
                        _acc[i] -= _m0 * (_p[i] / (_d[i] * _d[i]) + _p[n] / (_d[n] * _d[n])) * _kernel.grad_w(deltaPos);
                }
            }
        }
    }

    void applyViscousForce()
    {
        #pragma omp parallel for
        for (int i = 0; i < _particleCount; i++)
        {
            Vec2f particle = _pos[i];
            int x = std::floor(particle.x);
            int y = std::floor(particle.y);

            for (Couple cell : getNeighbours(x, y))
            {
                std::vector<tIndex> neighbors = _pidxInGrid[idx1d(cell.x, cell.y)];
                for (tIndex &n : neighbors)
                {
                    Vec2f deltaPos = _pos[i] - _pos[n];
                    Vec2f deltaVel = _vel[i] - _vel[n];
                    if (deltaPos.length() > 0.1)
                        _acc[i] += 2 * _nu * (_m0 / _d[n]) * deltaVel * deltaPos * _kernel.grad_w(deltaPos) / (deltaPos * deltaPos + 0.01 * _h);
                }
            }
        }
    }

    void updateVelocity()
    {
        #pragma omp parallel for
        for (int i = 0; i < _particleCount; i++)
            _vel[i] += _acc[i] * _dt;
    }

    void updatePosition()
    {
        #pragma omp parallel for
        for (int i = 0; i < _particleCount; i++)
            _pos[i] += _vel[i] * _dt;
    }

    void updateColor()
    {
        for (tIndex i = 0; i < _pos.size(); ++i) {
            _col[i * 4 + 0] = _d[i] / _d0;
            _col[i * 4 + 1] = 0.2;
            _col[i * 4 + 2] = 1 - _d[i] / _d0;
        }
    }

    void updateVelLine()
    {
        for (tIndex i = 0; i < _pos.size(); ++i) {
            _vln[i * 4 + 0] = _pos[i].x;
            _vln[i * 4 + 1] = _pos[i].y;
            _vln[i * 4 + 2] = _pos[i].x + _vel[i].x;
            _vln[i * 4 + 3] = _pos[i].y + _vel[i].y;
        }
    }

    inline tIndex idx1d(const int i, const int j) { return i + j * resX(); }

    CubicSpline _kernel;

    // particle data
    std::vector<Vec2f> _pos;      // position
    std::vector<Vec2f> _vel;      // velocity
    std::vector<Vec2f> _acc;      // acceleration
    std::vector<Real>  _p;        // pressure
    std::vector<Real>  _d;        // density
    int _particleCount;           // number of moving particles

    std::vector< std::vector<tIndex> > _pidxInGrid; // will help you find neighbor particles

    std::vector<float> _col;    // particle color; just for visualization
    std::vector<float> _vln;    // particle velocity lines; just for visualization

    // simulation
    Real _dt;                     // time step
    int _resX, _resY;             // background grid resolution

    // SPH coefficients
    Real _nu;                     // viscosity coefficient
    Real _d0;                     // rest density
    Real _h;                      // particle spacing (i.e., diameter)
    Vec2f _g;                     // gravity

    Real _m0;                     // rest mass
    Real _k;                      // EOS coefficient

    Real _eta;
    Real _c;                      // speed of sound
    Real _gamma;                  // EOS power factor
};