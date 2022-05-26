#include "wcsph_solver.h"


void WCSPHSolver::init(const int gridX, const int gridY, const int fluidWidth, const int fluidHeight) {
    // - init a fluid mass of size (fluidWidth, fluitHeight)
    // - create a grid of gridX * gridY cells starting from bottom left corner of the fluid
    // - each cell of the grid is sampled with 2x2 particles
    _resX = gridX;
    _resY = gridY;

    // sample fluid mass
    _position.clear();
    for (int j = 1; j < fluidHeight + 1; j++) {
        for (int i = 1; i < fluidWidth + 1; i++) {
            _position.push_back(Vec2f(i + 0.25, j + 0.25));
            _position.push_back(Vec2f(i + 0.75, j + 0.25));
            _position.push_back(Vec2f(i + 0.25, j + 0.75));
            _position.push_back(Vec2f(i + 0.75, j + 0.75));
        }
    }
    _fluidCount = _position.size();

    // sample walls
    addSolidBox(0, 0, _resX, _resY);
    addSolidBox(30, 5, 40, 15);

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

void WCSPHSolver::addSolidBox(int bottomX, int bottomY, int topX, int topY) {
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

void WCSPHSolver::update()
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

std::vector<Cell> WCSPHSolver::getNeighbourCells(const int i, const int j)
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

Index WCSPHSolver::idx1d(const int i, const int j) {
    return i + j * resX();
}

void WCSPHSolver::buildNeighbor()
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

void WCSPHSolver::computeDensity()
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
            for (Index& n : neighbors)
                _density[i] += _m0 * _kernel.W(_position[i] - _position[n]);
        }
    }
}

void WCSPHSolver::computePressure()
{
#pragma omp parallel for
    for (int i = 0; i < _position.size(); i++)
        _pressure[i] = std::max(0.0f, _k * (std::pow(_density[i] / _rho0, _gamma) - 1));
}

void WCSPHSolver::applyBodyForce()
{
#pragma omp parallel for
    for (int i = 0; i < _fluidCount; i++)
        _acceleration[i] = _g;
}

void WCSPHSolver::applyPressureForce()
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
            for (Index& n : neighbors)
            {
                Vec2f deltaPos = _position[i] - _position[n];
                if (deltaPos.length() > 0.1)
                    _acceleration[i] -= _m0 * (_pressure[i] / (_density[i] * _density[i]) + _pressure[n] / (_density[n] * _density[n])) * _kernel.gradW(deltaPos);
            }
        }
    }
}

void WCSPHSolver::applyViscousForce()
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
            for (Index& n : neighbors)
            {
                Vec2f deltaPos = _position[i] - _position[n];
                Vec2f deltaVel = _velocity[i] - _velocity[n];
                if (deltaPos.length() > 0.1)
                    _acceleration[i] += 2 * _nu * (_m0 / _density[n]) * deltaVel * deltaPos * _kernel.gradW(deltaPos) / (deltaPos * deltaPos + 0.01 * _h);
            }
        }
    }
}

void WCSPHSolver::updateVelocity()
{
#pragma omp parallel for
    for (int i = 0; i < _fluidCount; i++)
        _velocity[i] += _acceleration[i] * _dt;
}

void WCSPHSolver::updatePosition()
{
#pragma omp parallel for
    for (int i = 0; i < _fluidCount; i++)
        _position[i] += _velocity[i] * _dt;
}

void WCSPHSolver::updateColor()
{
    for (Index i = 0; i < _fluidCount; i++) {
        _color[i].x = lightColor.x + (_density[i] / _rho0) * (denseColor.x - lightColor.x);
        _color[i].y = lightColor.y + (_density[i] / _rho0) * (denseColor.y - lightColor.y);
        _color[i].z = lightColor.z + (_density[i] / _rho0) * (denseColor.z - lightColor.z);
    }
}