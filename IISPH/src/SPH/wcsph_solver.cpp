#include "wcsph_solver.h"


void WCSPHsolver::init(const int gridX, const int gridY, const int fluidWidth, const int fluidHeight) {
    // - init a fluid mass of size (fluidWidth, fluitHeight)
    // - create a grid of gridX * gridY cells starting from bottom left corner of the fluid
    // - each cell of the grid is sampled with 2x2 particles
    _resX = gridX;
    _resY = gridY;

    // sample fluid mass
    _position.clear();
    for (int j = 0; j < fluidHeight; j++) {
        for (int i = 0; i < fluidWidth; i++) {
            _position.push_back(Vec2f(i + 0.25, j + 0.25));
            _position.push_back(Vec2f(i + 0.75, j + 0.25));
            _position.push_back(Vec2f(i + 0.25, j + 0.75));
            _position.push_back(Vec2f(i + 0.75, j + 0.75));
        }
    }
    _fluidCount = _position.size();

    // sample walls
    //addSolidBox(0, 0, _resX, _resY);
    _l = 0.5 * _h;
    _r = static_cast<Real>(_resX) - 0.5 * _h;
    _b = 0.5 * _h;
    _t = static_cast<Real>(_resY) - 0.5 * _h;

    // color particles
    _color.clear();
    for (Index i = 0; i < _fluidCount; i++)
        _color.push_back(denseColor);
    //for (Index i = _fluidCount; i < _position.size(); i++)
        //_color.push_back(wallColor);

    // init other particle quantities
    _velocity = std::vector<Vec2f>(_position.size(), Vec2f(0, 0));
    _acceleration = std::vector<Vec2f>(_position.size(), Vec2f(0, 0));
    _pressure = std::vector<Real>(_position.size(), 0);
    _density = std::vector<Real>(_position.size(), 0);
    _neighborsGrid = std::vector<std::vector<Index>>(_resX * (size_t)_resY, std::vector<Index>());
}

void WCSPHsolver::addSolidBox(int bottomX, int bottomY, int topX, int topY) {
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

void WCSPHsolver::update()
{
    buildNeighbor();
    computeDensity();
    computePressure();

    applyBodyForce();
    applyPressureForce();
    applyViscousForce();

    updateVelocity();
    updatePosition();
    resolveCollision();
    updateColor();
}

std::vector<Cell> WCSPHsolver::getNeighbourCells(const int i, const int j)
{
    std::vector<Cell> neighbors;
    neighbors.push_back(Cell(i, j));

    if (i > 0)
        neighbors.push_back(Cell(i - 1, j));
    if (i < _resX - 1)
        neighbors.push_back(Cell(i + 1, j));
    if (j > 0)
        neighbors.push_back(Cell(i, j - 1));
    if (j < _resY - 1)
        neighbors.push_back(Cell(i, j + 1));
    if (i > 0 && j > 0)
        neighbors.push_back(Cell(i - 1, j - 1));
    if (i < _resX - 1 && j > 0)
        neighbors.push_back(Cell(i + 1, j - 1));
    if (i > 0 && j < _resY - 1)
        neighbors.push_back(Cell(i - 1, j + 1));
    if (i < _resX - 1 && j < _resY - 1)
        neighbors.push_back(Cell(i + 1, j + 1));

    return neighbors;
}

Index WCSPHsolver::idx1d(const int i, const int j) {
    static int maxInd = _resX * _resY - 1;
    return clamp(i + j * _resX, 0, maxInd);
}

std::vector<Index> WCSPHsolver::getNeighbors(int i) {
    int x = std::floor(_position[i].x);
    int y = std::floor(_position[i].y);
    std::vector<Index> neighbors;

    for (Cell cell : getNeighbourCells(x, y)) {
        std::vector<Index> neighborsFromCell = _neighborsGrid[idx1d(cell.x, cell.y)];
        neighbors.insert(std::end(neighbors), std::begin(neighborsFromCell), std::end(neighborsFromCell));
    }
    return neighbors;
}

void WCSPHsolver::buildNeighbor()
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

void WCSPHsolver::computeDensity()
{
#pragma omp parallel for
    for (int i = 0; i < _position.size(); i++)
    {
        _density[i] = 0.0f;

        std::vector<Index> neighbors = getNeighbors(i);
        for (Index& j : neighbors) {
            Vec2f pos_ij = _position[i] - _position[j];
            _density[i] += _m0 * _kernel.W(pos_ij);
        }
    }
}

void WCSPHsolver::computePressure()
{
#pragma omp parallel for
    for (int i = 0; i < _position.size(); i++)
        _pressure[i] = std::max(0.0f, _k * (std::pow(_density[i] / _rho0, _gamma) - 1));
}

void WCSPHsolver::applyBodyForce()
{
#pragma omp parallel for
    for (int i = 0; i < _fluidCount; i++)
        _acceleration[i] = _g;
}

void WCSPHsolver::applyPressureForce()
{
#pragma omp parallel for
    for (int i = 0; i < _fluidCount; i++)
    {
        std::vector<Index> neighbors = getNeighbors(i);
        for (Index& j : neighbors)
        {
            if (j != i) {
                Vec2f pos_ij = _position[i] - _position[j];
                _acceleration[i] -= _m0 * (_pressure[i] / square(_density[i]) + _pressure[j] / square(_density[j])) * _kernel.gradW(pos_ij);
            }
        }
    }
}

void WCSPHsolver::applyViscousForce()
{
#pragma omp parallel for
    for (int i = 0; i < _fluidCount; i++)
    {
        std::vector<Index> neighbors = getNeighbors(i);
        for (Index& j : neighbors)
        {
            if (j != i) {
                Vec2f pos_ij = _position[i] - _position[j];
                Vec2f vel_ij = _velocity[i] - _velocity[j];
                _acceleration[i] += 2 * _nu * (_m0 / _density[j]) * vel_ij * pos_ij * _kernel.gradW(pos_ij) / (pos_ij * pos_ij + 0.01 * _h);
            }
        }
    }
}

void WCSPHsolver::updateVelocity()
{
#pragma omp parallel for
    for (int i = 0; i < _fluidCount; i++)
        _velocity[i] += _acceleration[i] * _dt;
}

void WCSPHsolver::updatePosition()
{
#pragma omp parallel for
    for (int i = 0; i < _fluidCount; i++)
        _position[i] += _velocity[i] * _dt;
}

void WCSPHsolver::resolveCollision()
{
    std::vector<Index> need_res;
    for (Index i = 0; i < _fluidCount; i++) {
        if (_position[i].x <= _l || _position[i].y <= _b || _position[i].x >= _r || _position[i].y >= _t)
            need_res.push_back(i);
    }

    for (auto it = need_res.begin(); it < need_res.end(); ++it) {
        const Vec2f p0 = _position[*it];
        _position[*it].x = clamp(_position[*it].x, _l, _r);
        _position[*it].y = clamp(_position[*it].y, _b, _t);
        _velocity[*it] = (_position[*it] - p0) / _dt;
    }
}

void WCSPHsolver::updateColor()
{
    for (Index i = 0; i < _fluidCount; i++) {
        _color[i].x = lightColor.x + (_density[i] / _rho0) * (denseColor.x - lightColor.x);
        _color[i].y = lightColor.y + (_density[i] / _rho0) * (denseColor.y - lightColor.y);
        _color[i].z = lightColor.z + (_density[i] / _rho0) * (denseColor.z - lightColor.z);
    }
}
