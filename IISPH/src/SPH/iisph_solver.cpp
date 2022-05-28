#include "iisph_solver.h"

void IISPHsolver::init(const int gridX, const int gridY, const int fluidWidth, const int fluidHeight) {
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
    //addSolidBox(30, 5, 40, 15);
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
    _pressure = std::vector<Real>(_position.size(), 0);
    _density = std::vector<Real>(_position.size(), 0);
    _neighborsGrid = std::vector<std::vector<Index>>(resX() * (size_t)resY(), std::vector<Index>());

    Dii = std::vector<Vec2f>(_position.size(), Vec2f(0, 0));
    Aii = std::vector<Real>(_position.size(), 0);
    sumDijPj = std::vector<Vec2f>(_position.size(), Vec2f(0, 0));

    Vadv = std::vector<Vec2f>(_position.size(), Vec2f(0, 0));
    Dadv = std::vector<Real>(_position.size(), 0);
    Pl = std::vector<Real>(_position.size(), 0);
    Dcorr = std::vector<Real>(_position.size(), 0);
    Fadv = std::vector<Vec2f>(_position.size(), Vec2f(0, 0));
    Fp = std::vector<Vec2f>(_position.size(), Vec2f(0, 0));
}

void IISPHsolver::addSolidBox(int bottomX, int bottomY, int topX, int topY) {
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

void IISPHsolver::update() {
    buildNeighbor();
    predictAdvection();
    pressureSolve();
    integration();
    resolveCollision();
    updateColor();
}

void IISPHsolver::buildNeighbor()
{
    for (auto& indices : _neighborsGrid)
        indices.clear();

    int x = 0;
    int y = 0;

    for (int i = 0; i < _fluidCount; i++)
    {
        x = std::floor(_position[i].x);
        y = std::floor(_position[i].y);

        _neighborsGrid[idx1d(x, y)].push_back(i);
    }
}

void IISPHsolver::predictAdvection() {
#pragma omp parallel for
    for (int i = 0; i < _fluidCount; i++)
        computeDensity(i);

#pragma omp parallel for
    for (int i = 0; i < _fluidCount; i++) {
        computeAdvectionForces(i);
        predictVelocity(i);
        storeDii(i);
    }

#pragma omp parallel for
    for (int i = 0; i < _fluidCount; i++) {
        predictDensity(i);
        initPressure(i);
        storeAii(i);
    }
}

void IISPHsolver::pressureSolve() {
    int l = 0;
    avgDensity = 0.0f;

    while ( ((avgDensity - _rho0) > _eta) || (l < 2) )
    {
#pragma omp parallel for
        for (int i = 0; i < _fluidCount; i++)
            storeSumDijPj(i);

#pragma omp parallel for
        for (int i = 0; i < _fluidCount; i++)
            computePressure(i);

        computeError();
        l++;
    }
}

void IISPHsolver::integration() {
#pragma omp parallel for
    for (int i = 0; i < _fluidCount; i++)
        computePressureForces(i);

#pragma omp parallel for
    for (int i = 0; i < _fluidCount; i++) {
        updateVelocity(i);
        updatePosition(i);
    }
}

void IISPHsolver::resolveCollision()
{
    std::vector<Index> need_res;
    for (Index i = 0; i < _fluidCount; i++) {
        if (_position[i].x<=_l || _position[i].y<=_b || _position[i].x>=_r || _position[i].y>=_t)
            need_res.push_back(i);
    }

    for (auto it = need_res.begin(); it < need_res.end(); ++it) {
        const Vec2f p0 = _position[*it];
        _position[*it].x = clamp(_position[*it].x, _l, _r);
        _position[*it].y = clamp(_position[*it].y, _b, _t);
        _velocity[*it] = (_position[*it] - p0) / _dt;
    }
}

void IISPHsolver::updateColor()
{
    for (Index i = 0; i < _fluidCount; i++) {
        _color[i].x = lightColor.x + (_density[i] / _rho0) * (denseColor.x - lightColor.x);
        _color[i].y = lightColor.y + (_density[i] / _rho0) * (denseColor.y - lightColor.y);
        _color[i].z = lightColor.z + (_density[i] / _rho0) * (denseColor.z - lightColor.z);
    }
}


///////////////////////////////////////////////////////

std::vector<Cell> IISPHsolver::getNeighbourCells(const int i, const int j)
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

Index IISPHsolver::idx1d(const int i, const int j) {
    static int maxInd = _resX * _resY - 1;
    return clamp(i + j * resX(), 0, maxInd);
}

std::vector<Index> IISPHsolver::getNeighbors(int i) {
    int x = std::floor(_position[i].x);
    int y = std::floor(_position[i].y);
    std::vector<Index> neighbors;

    for (Cell cell : getNeighbourCells(x, y)) {
        std::vector<Index> neighborsFromCell = _neighborsGrid[idx1d(cell.x, cell.y)];
        neighbors.insert(neighbors.end(), neighborsFromCell.begin(), neighborsFromCell.end());
    }
    return neighbors;
}

//////////////////////////////////////////////////////

void IISPHsolver::computeDensity(int i) {
    _density[i] = 0.0f;
    Vec2f pos_ij;

    std::vector<Index> neighbors = getNeighbors(i);
    for (Index& j : neighbors) {
        pos_ij = _position[i] - _position[j];
        _density[i] += _m0 * _kernel.W(pos_ij);
    }
}

void IISPHsolver::computeAdvectionForces(int i) {
    Fadv[i].x = 0.0f;
    Fadv[i].y = 0.0f;
    addBodyForce(i);
    addViscousForce(i);
}

void IISPHsolver::addBodyForce(int i) {
    Fadv[i] += _m0 * _g;
}

void IISPHsolver::addViscousForce(int i) {
    Vec2f pos_ij;
    Vec2f vel_ij;

    std::vector<Index> neighbors = getNeighbors(i);
    for (Index& j : neighbors)
        if (j != i) {
            pos_ij = _position[i] - _position[j];
            vel_ij = _velocity[i] - _velocity[j];
            Fadv[i] += 2 * _nu * (square(_m0) / _density[j]) * vel_ij * pos_ij * _kernel.gradW(pos_ij) / (pos_ij * pos_ij + 0.01 * _h);
        }
}

void IISPHsolver::predictVelocity(int i) {
    Vadv[i] = _velocity[i] + _dt * Fadv[i] / _m0;
}

void IISPHsolver::storeDii(int i) {
    Dii[i].x = 0.0f;
    Dii[i].y = 0.0f;
    Vec2f pos_ij;

    std::vector<Index> neighbors = getNeighbors(i);
    for (Index& j : neighbors)
        if (j != i) {
            pos_ij = _position[i] - _position[j];
            Dii[i] += square(_dt) * (-_m0 / square(_density[i])) * _kernel.gradW(pos_ij);
        }
}

///////////////////////////////////////////////////////

void IISPHsolver::predictDensity(int i) {
    Dadv[i] = _density[i];
    Vec2f pos_ij;
    Vec2f vel_adv_ij;

    std::vector<Index> neighbors = getNeighbors(i);
    for (Index& j : neighbors)
        if (j != i) {
            pos_ij = _position[i] - _position[j];
            vel_adv_ij = Vadv[i] - Vadv[j];
            Dadv[i] += _dt * _m0 * vel_adv_ij.dotProduct(_kernel.gradW(pos_ij));
        }
}

void IISPHsolver::initPressure(int i) {
    Pl[i] = 0.5f * _pressure[i];
}

void IISPHsolver::storeAii(int i) {
    Aii[i] = 0.0f;
    Vec2f pos_ij;
    Vec2f d_ji;

    std::vector<Index> neighbors = getNeighbors(i);
    for (Index& j : neighbors)
        if (j != i) {
            pos_ij = _position[i] - _position[j];
            d_ji = -(square(_dt) * _m0) / square(_density[i]) * (-_kernel.gradW(pos_ij));
            Aii[i] += _m0 * (Dii[i] - d_ji).dotProduct(_kernel.gradW(pos_ij));       
        }
}

////////////////////////////////////////////////////////

void IISPHsolver::storeSumDijPj(int i) {
    sumDijPj[i].x = 0.0f;
    sumDijPj[i].y = 0.0f;
    Vec2f pos_ij;

    std::vector<Index> neighbors = getNeighbors(i);
    for (Index& j : neighbors)
        if (j != i) {
            pos_ij = _position[i] - _position[j];
            sumDijPj[i] += square(_dt) * (-_m0 / square(_density[j])) * _pressure[j] * _kernel.gradW(pos_ij);
        }
}

void IISPHsolver::computePressure(int i) {
    Real  sum = 0.0f;
    Vec2f pos_ij;
    Vec2f d_ji;
    Vec2f aux;

    std::vector<Index> neighbors = getNeighbors(i);
    for (Index& j : neighbors)
        if (j != i) {
            pos_ij = _position[i] - _position[j];
            d_ji = -(square(_dt) * _m0) / square(_density[i]) * (-_kernel.gradW(pos_ij));
            aux = sumDijPj[i] - Dii[j] * Pl[j] - (sumDijPj[j] - d_ji * Pl[i]);
            sum += _m0 * aux.dotProduct(_kernel.gradW(pos_ij));
        }

    Real previousPl = Pl[i];
    Dcorr[i] = Dadv[i] + sum;

    if (std::abs(Aii[i]) > std::numeric_limits<Real>::epsilon())
        Pl[i] = (1 - _omega) * previousPl + (_omega / Aii[i]) * (_rho0 - Dcorr[i]);
    else
        Pl[i] = 0.0;
    
    _pressure[i] = std::max(Pl[i], 0.0f);
    Pl[i] = _pressure[i];
    Dcorr[i] += Aii[i] * previousPl;
}

void IISPHsolver::computeError()
{
    avgDensity = 0.0;
    for (int i = 0; i < _position.size(); i++)
        avgDensity += Dcorr[i];

    avgDensity /= _position.size();
}

////////////////////////////////////////////////////////

void IISPHsolver::computePressureForces(int i) {
    Fp[i].x = 0.0f;
    Fp[i].y = 0.0f;
    Vec2f pos_ij;

    std::vector<Index> neighbors = getNeighbors(i);
    for (Index& j : neighbors)
        if (j != i) {
            pos_ij = _position[i] - _position[j];
            Fp[i] += - square(_m0) * (_pressure[i] / square(_density[i]) + _pressure[j] / square(_density[j])) * _kernel.gradW(pos_ij);
        }
}

void IISPHsolver::updateVelocity(int i) {
    _velocity[i] = Vadv[i] + _dt * Fp[i] / _m0;
}

void IISPHsolver::updatePosition(int i) {
    _position[i] += _dt * _velocity[i];
}