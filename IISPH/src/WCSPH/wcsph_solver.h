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
    void initScene(
        const int res_x, const int res_y, const int f_width, const int f_height)
    {
        _pos.clear();

        _resX = res_x;
        _resY = res_y;

        // set wall for boundary
        _l = 0.5 * _h;
        _r = static_cast<Real>(res_x) - 0.5 * _h;
        _b = 0.5 * _h;
        _t = static_cast<Real>(res_y) - 0.5 * _h;

        // sample a fluid mass
        for (int j = 0; j < f_height; ++j) {
            for (int i = 0; i < f_width; ++i) {
                _pos.push_back(Vec2f(i + 0.25, j + 0.25));
                _pos.push_back(Vec2f(i + 0.75, j + 0.25));
                _pos.push_back(Vec2f(i + 0.25, j + 0.75));
                _pos.push_back(Vec2f(i + 0.75, j + 0.75));
            }
        }

        // make sure for the other particle quantities
        _vel = std::vector<Vec2f>(_pos.size(), Vec2f(0, 0));
        _acc = std::vector<Vec2f>(_pos.size(), Vec2f(0, 0));
        _p = std::vector<Real>(_pos.size(), 0);
        _d = std::vector<Real>(_pos.size(), 0);

        _col = std::vector<float>(_pos.size() * 4, 1.0); // RGBA
        _vln = std::vector<float>(_pos.size() * 4, 0.0); // GL_LINES

        _pidxInGrid = std::vector<std::vector<tIndex>>(resX() * resY(), std::vector<tIndex>());

        updateColor();
    }

    void update()
    {
        std::cout << '.' << std::flush;

        buildNeighbor();
        computeDensity();
        computePressure();

        _acc = std::vector<Vec2f>(_pos.size(), Vec2f(0, 0));
        applyBodyForce();
        applyPressureForce();
        applyViscousForce();

        updateVelocity();
        updatePosition();

        resolveCollision();

        updateColor();
        if (gShowVel) updateVelLine();
    }

    tIndex particleCount() const { return _pos.size(); }
    const Vec2f& position(const tIndex i) const { return _pos[i]; }
    const float& color(const tIndex i) const { return _col[i]; }
    const float& vline(const tIndex i) const { return _vln[i]; }

    int resX() const { return _resX; }
    int resY() const { return _resY; }

    Real equationOfState(
        const Real d, const Real d0, const Real k,
        const Real gamma = 7.0)
    {
        // TODO: pressure calculation
    }

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
        // TODO:
        for (auto& pid : _pidxInGrid)
            pid.clear();

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
        // TODO:
        for (int i = 0; i < _pos.size(); i++)
        {
            Vec2f particle = _pos[i];
            int x = std::floor(particle.x);
            int y = std::floor(particle.y);

            Real density = 0;
            std::vector<Couple> neighborCells = getNeighbours(x, y);

            for (Couple cell : neighborCells)
            {
                std::vector<tIndex> neighbors = _pidxInGrid[idx1d(cell.x, cell.y)];
                for (tIndex n : neighbors)
                    density += _m0 * _kernel.w(_pos[i] - _pos[n]);
            }
            _d[i] = density;
        }
    }

    void computePressure()
    {
        // TODO:
        for (int i = 0; i < _p.size(); i++)
            _p[i] = std::max(0.0, _k * (std::pow(_d[i] / _d0, 7) - 1));
    }

    void applyBodyForce()
    {
        // TODO:
        for (int i = 0; i < _acc.size(); i++)
            _acc[i] = _g;
    }

    void applyPressureForce()
    {
        // TODO:
        for (int i = 0; i < _pos.size(); i++)
        {
            Vec2f particle = _pos[i];
            int x = std::floor(particle.x);
            int y = std::floor(particle.y);
            std::vector<Couple> neighborCells = getNeighbours(x, y);
            for (Couple cell : neighborCells)
            {
                std::vector<tIndex> neighbors = _pidxInGrid[idx1d(cell.x, cell.y)];
                for (tIndex n : neighbors)
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
        // TODO:
        for (int i = 0; i < _pos.size(); i++)
        {
            Vec2f particle = _pos[i];
            int x = std::floor(particle.x);
            int y = std::floor(particle.y);
            std::vector<Couple> neighborCells = getNeighbours(x, y);
            for (Couple cell : neighborCells)
            {
                std::vector<tIndex> neighbors = _pidxInGrid[idx1d(cell.x, cell.y)];
                for (tIndex n : neighbors)
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
        // TODO:
        for (int i = 0; i < _vel.size(); i++)
            _vel[i] += _acc[i] * _dt;
    }

    void updatePosition()
    {
        // TODO:
        for (int i = 0; i < _pos.size(); i++)
            _pos[i] += _vel[i] * _dt;
    }

    // simple collision detection/resolution for each particle
    void resolveCollision()
    {
        std::vector<tIndex> need_res;
        for (tIndex i = 0; i < particleCount(); ++i) {
            if (_pos[i].x<_l || _pos[i].y<_b || _pos[i].x>_r || _pos[i].y>_t)
                need_res.push_back(i);
        }

        for (
            std::vector<tIndex>::const_iterator it = need_res.begin();
            it < need_res.end();
            ++it) {
            const Vec2f p0 = _pos[*it];
            _pos[*it].x = clamp(_pos[*it].x, _l, _r);
            _pos[*it].y = clamp(_pos[*it].y, _b, _t);
            _vel[*it] = (_pos[*it] - p0) / _dt;
        }
    }

    void updateColor()
    {
        for (tIndex i = 0; i < particleCount(); ++i) {
            _col[i * 4 + 0] = _d[i] / _d0;
            _col[i * 4 + 1] = 0.2;
            _col[i * 4 + 2] = 1 - _d[i] / _d0;
        }
    }

    void updateVelLine()
    {
        for (tIndex i = 0; i < particleCount(); ++i) {
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

    std::vector< std::vector<tIndex> > _pidxInGrid; // will help you find neighbor particles

    std::vector<float> _col;    // particle color; just for visualization
    std::vector<float> _vln;    // particle velocity lines; just for visualization

    // simulation
    Real _dt;                     // time step

    int _resX, _resY;             // background grid resolution

    // wall
    Real _l, _r, _b, _t;          // wall (boundary)

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

    // options
    bool gShowVel = false;
};