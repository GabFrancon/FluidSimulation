#include "iisph_solver.h"


/*--------------------------------------------Main functions--------------------------------------------------*/

void IISPHsolver::init(const int gridX, const int gridY, const int fluidWidth, const int fluidHeight) {
    // - init a fluid mass of size (fluidWidth, fluitHeight)
    // - create a grid of gridX * gridY cells starting from bottom left corner of the fluid
    // - each cell of the grid is sampled with 2x2 particles
    _resX = gridX;
    _resY = gridY;

    // sample fluid mass
    _fPosition.clear();
    for (int j = 0; j < fluidHeight; j++) {
        for (int i = 0; i < fluidWidth; i++) {
            _fPosition.push_back(Vec2f(i + 0.25, j + 0.25));
            _fPosition.push_back(Vec2f(i + 0.75, j + 0.25));
            _fPosition.push_back(Vec2f(i + 0.25, j + 0.75));
            _fPosition.push_back(Vec2f(i + 0.75, j + 0.75));
        }
    }
    _fluidCount = _fPosition.size();
 
    // sample walls
    _l = 0.5 * _h;
    _r = static_cast<Real>(_resX) - 0.5 * _h;
    _b = 0.5 * _h;
    _t = static_cast<Real>(_resY) - 0.5 * _h;

    // color particles
    _fColor.clear();
    for (Index i = 0; i < _fluidCount; i++)
        _fColor.push_back(denseColor);

    // init other particle quantities
    _fVelocity = std::vector<Vec2f>(_fluidCount, Vec2f(0, 0));
    _fPressure = std::vector<Real> (_fluidCount, 0);
    _fDensity  = std::vector<Real> (_fluidCount, 0);

    _neighborsGrid  = std::vector<std::vector<Index>>(_resX * (size_t)_resY, std::vector<Index>());
    _fluidNeighbors = std::vector<std::vector<Index>>(_fluidCount, std::vector<Index>());

    Dii      = std::vector<Vec2f>(_fluidCount, Vec2f(0, 0));
    Aii      = std::vector<Real> (_fluidCount, 0);
    sumDijPj = std::vector<Vec2f>(_fluidCount, Vec2f(0, 0));
    Vadv     = std::vector<Vec2f>(_fluidCount, Vec2f(0, 0));
    Dadv     = std::vector<Real> (_fluidCount, 0);
    Pl       = std::vector<Real> (_fluidCount, 0);
    Dcorr    = std::vector<Real> (_fluidCount, 0);
    Fadv     = std::vector<Vec2f>(_fluidCount, Vec2f(0, 0));
    Fp       = std::vector<Vec2f>(_fluidCount, Vec2f(0, 0));
}

void IISPHsolver::update() {
    buildNeighborGrid();

    predictAdvection();
    pressureSolve();
    integration();

    resolveCollision();
    updateColor();
}

void IISPHsolver::buildNeighborGrid() {
    for (auto& indices : _neighborsGrid)
        indices.clear();

    for (int i = 0; i < _fluidCount; i++)
        fillNeighborGrid(i);

#pragma omp parallel for
    for (int i = 0; i < _fluidCount; i++)
        findNeighbors(i, 2 * _h);
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
    _avgDensity = 0.0f;

    while ( ((_avgDensity - _rho0) > _eta) || (l < 2) )
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

void IISPHsolver::resolveCollision() {
    std::vector<Index> need_res;
    for (Index i = 0; i < _fluidCount; i++) {
        if (_fPosition[i].x<=_l || _fPosition[i].y<=_b || _fPosition[i].x>=_r || _fPosition[i].y>=_t)
            need_res.push_back(i);
    }

    for (auto it = need_res.begin(); it < need_res.end(); ++it) {
        const Vec2f p0 = _fPosition[*it];
        _fPosition[*it].x = clamp(_fPosition[*it].x, _l, _r);
        _fPosition[*it].y = clamp(_fPosition[*it].y, _b, _t);
        _fVelocity[*it] = (_fPosition[*it] - p0) / _dt;
    }
}

void IISPHsolver::updateColor() {
    for (Index i = 0; i < _fluidCount; i++) {
        _fColor[i].x = lightColor.x + (_fDensity[i] / _rho0) * (denseColor.x - lightColor.x);
        _fColor[i].y = lightColor.y + (_fDensity[i] / _rho0) * (denseColor.y - lightColor.y);
        _fColor[i].z = lightColor.z + (_fDensity[i] / _rho0) * (denseColor.z - lightColor.z);
    }
}



/*-------------------------------------------Neighbor search------------------------------------------------*/

void IISPHsolver::getNeighborCells(std::vector<Index>& neighbors, Vec2f particle, const int radius) {
    if (!isInsideGrid(particle)) {
        neighbors.clear();
        return;
    }

    Vec2i cell = cellPos(particle);

    int imin = std::max(cell.x - radius, 0);
    int imax = std::min(cell.x + radius, _resX - 1);
    int jmin = std::max(cell.y - radius, 0);
    int jmax = std::min(cell.y + radius, _resY - 1);

    int count = 0;
    int size = (jmax - jmin + 1) * (imax - imin + 1);
    neighbors.resize(size);

    for (int j = jmin; j <= jmax; ++j)
        for (int i = imin; i <= imax; ++i) {
            neighbors[count] = cellID(i, j);
            count++;
        }
}

Index IISPHsolver::cellID(Vec2f particle) {
    Vec2i cell = cellPos(particle);
    return cellID(cell.x, cell.y);
}

Index IISPHsolver::cellID(int i, int j) {
    return i + j * _resX;
}

bool IISPHsolver::isInsideGrid(Vec2f particle) {
    Index id = cellID(particle);
    return isInsideGrid(id);
}

bool IISPHsolver::isInsideGrid(int id) {
    return id >= 0 && id < _resX * _resY;
}

Vec2i IISPHsolver::cellPos(Vec2f particle) {
    Vec2i cell;
    cell.x = std::floor(particle.x);
    cell.y = std::floor(particle.y);
    return cell;
}

void  IISPHsolver::fillNeighborGrid(int i) {
    int id = cellID(_fPosition[i]);

    if (isInsideGrid(id))
        _neighborsGrid[id].push_back(i);
}

void IISPHsolver::findNeighbors(int i, const int radius) {
    size_t lastFluidSize = _fluidNeighbors[i].size();
    _fluidNeighbors[i].clear();
    _fluidNeighbors.reserve(lastFluidSize);

    std::vector<Index> neighborCells;
    Real squaredRadius = square(radius);
    Real distance      = 0.0f;
    Index neighborID   = 0;

    getNeighborCells(neighborCells, _fPosition[i], radius);

    for (size_t j = 0; j < neighborCells.size(); j++) {
        std::vector<Index> particlesInCell = _neighborsGrid[neighborCells[j]];

        for (size_t k = 0; k < particlesInCell.size(); k++) {
            neighborID = particlesInCell[k];
            distance = (_fPosition[neighborID] - _fPosition[i]).lengthSquare();

            if (distance < squaredRadius) {
                _fluidNeighbors[i].push_back(neighborID);
            }
        }
    }
}



/*------------------------------------------Fluid simulation-------------------------------------------------*/

void IISPHsolver::computeDensity(int i) {
    _fDensity[i] = 0.0f;
    Vec2f pos_ij;

    std::vector<Index> neighbors = _fluidNeighbors[i];
    for (Index& j : neighbors) {
        pos_ij = _fPosition[i] - _fPosition[j];
        _fDensity[i] += _m0 * _kernel.W(pos_ij);
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

    std::vector<Index> neighbors = _fluidNeighbors[i];
    for (Index& j : neighbors)
        if (_fPosition[j] != _fPosition[i]) {
            pos_ij = _fPosition[i] - _fPosition[j];
            vel_ij = _fVelocity[i] - _fVelocity[j];
            Fadv[i] += 2 * _nu * (square(_m0) / _fDensity[j]) * vel_ij.dotProduct(pos_ij) * _kernel.gradW(pos_ij) / (pos_ij.lengthSquare() + 0.01 * _h);
        }
}

void IISPHsolver::predictVelocity(int i) {
    Vadv[i] = _fVelocity[i] + _dt * Fadv[i] / _m0;
}

void IISPHsolver::storeDii(int i) {
    Dii[i].x = 0.0f;
    Dii[i].y = 0.0f;
    Vec2f pos_ij;

    std::vector<Index> neighbors = _fluidNeighbors[i];
    for (Index& j : neighbors)
        if (_fPosition[j] != _fPosition[i]) {
            pos_ij = _fPosition[i] - _fPosition[j];
            Dii[i] += square(_dt) * (-_m0 / square(_fDensity[i])) * _kernel.gradW(pos_ij);
        }
}

void IISPHsolver::predictDensity(int i) {
    Dadv[i] = _fDensity[i];
    Vec2f pos_ij;
    Vec2f vel_adv_ij;

    std::vector<Index> neighbors = _fluidNeighbors[i];
    for (Index& j : neighbors)
        if (_fPosition[j] != _fPosition[i]) {
            pos_ij = _fPosition[i] - _fPosition[j];
            vel_adv_ij = Vadv[i] - Vadv[j];
            Dadv[i] += _dt * _m0 * vel_adv_ij.dotProduct(_kernel.gradW(pos_ij));
        }
}

void IISPHsolver::initPressure(int i) {
    Pl[i] = 0.5f * _fPressure[i];
}

void IISPHsolver::storeAii(int i) {
    Aii[i] = 0.0f;
    Vec2f pos_ij;
    Vec2f d_ji;

    std::vector<Index> neighbors = _fluidNeighbors[i];
    for (Index& j : neighbors)
        if (_fPosition[j] != _fPosition[i]) {
            pos_ij = _fPosition[i] - _fPosition[j];
            d_ji = -(square(_dt) * _m0) / square(_fDensity[i]) * (-_kernel.gradW(pos_ij));
            Aii[i] += _m0 * (Dii[i] - d_ji).dotProduct(_kernel.gradW(pos_ij));       
        }
}

void IISPHsolver::storeSumDijPj(int i) {
    sumDijPj[i].x = 0.0f;
    sumDijPj[i].y = 0.0f;
    Vec2f pos_ij;

    std::vector<Index> neighbors = _fluidNeighbors[i];
    for (Index& j : neighbors)
        if (_fPosition[j] != _fPosition[i]) {
            pos_ij = _fPosition[i] - _fPosition[j];
            sumDijPj[i] += square(_dt) * (-_m0 / square(_fDensity[j])) * _fPressure[j] * _kernel.gradW(pos_ij);
        }
}

void IISPHsolver::computePressure(int i) {
    Real  sum = 0.0f;
    Vec2f pos_ij;
    Vec2f d_ji;
    Vec2f aux;

    std::vector<Index> neighbors = _fluidNeighbors[i];
    for (Index& j : neighbors)
        if (_fPosition[j] != _fPosition[i]) {
            pos_ij = _fPosition[i] - _fPosition[j];
            d_ji = -(square(_dt) * _m0) / square(_fDensity[i]) * (-_kernel.gradW(pos_ij));
            aux = sumDijPj[i] - Dii[j] * Pl[j] - (sumDijPj[j] - d_ji * Pl[i]);
            sum += _m0 * aux.dotProduct(_kernel.gradW(pos_ij));
        }

    Real previousPl = Pl[i];
    Dcorr[i] = Dadv[i] + sum;

    if (std::abs(Aii[i]) > std::numeric_limits<Real>::epsilon())
        Pl[i] = (1 - _omega) * previousPl + (_omega / Aii[i]) * (_rho0 - Dcorr[i]);
    else
        Pl[i] = 0.0;
    
    _fPressure[i] = std::fmax(Pl[i], 0.0f);
    Pl[i] = _fPressure[i];
    Dcorr[i] += Aii[i] * previousPl;
}

void IISPHsolver::computeError()
{
    _avgDensity = 0.0;
    for (int i = 0; i < _fluidCount; i++)
        _avgDensity += Dcorr[i];

    _avgDensity /= _fPosition.size();
}

void IISPHsolver::computePressureForces(int i) {
    Fp[i].x = 0.0f;
    Fp[i].y = 0.0f;
    Vec2f pos_ij;

    std::vector<Index> neighbors = _fluidNeighbors[i];
    for (Index& j : neighbors)
        if (_fPosition[j] != _fPosition[i]) {
            pos_ij = _fPosition[i] - _fPosition[j];
            Fp[i] += - square(_m0) * (_fPressure[i] / square(_fDensity[i]) + _fPressure[j] / square(_fDensity[j])) * _kernel.gradW(pos_ij);
        }
}

void IISPHsolver::updateVelocity(int i) {
    _fVelocity[i] = Vadv[i] + _dt * Fp[i] / _m0;
}

void IISPHsolver::updatePosition(int i) {

    if (isInsideGrid(_fPosition[i] + _dt * _fVelocity[i]) )
        _fPosition[i] += _dt * _fVelocity[i];
    else
        debugCrash(i);
}

void IISPHsolver::debugCrash(int i) {
    std::cout
        << "position     : " << _fPosition[i] << "\n"
        << "velocity     : " << _fVelocity[i] << "\n"
        << "pressure     : " << _fPressure[i] << "\n"
        << "density      : " << _fDensity[i] << "\n"
        << "F_p          : " << Fp[i] << "\n"
        << "sum d_ij p_j : " << sumDijPj[i] << "\n"
        << "a_ii         : " << Aii[i] << "\n"
        << "rho_adv      : " << Dadv[i] << "\n"
        << "d_ii         : " << Dii[i] << "\n"
        << "v_adv        : " << Vadv[i] << "\n"
        << "F_adv        : " << Fadv[i] << "\n"
        << std::endl;

    std::cout << "neighbors : \n";

    std::vector<Index> neighbors = _fluidNeighbors[i];
    for (Index j : neighbors) {
        if (_fPosition[j] != _fPosition[i]) {
            Vec2f pos_ij = _fPosition[i] - _fPosition[j];

            std::cout
                << "    position     : " << _fPosition[j] << "\n"
                << "    velocity     : " << _fVelocity[j] << "\n"
                << "    pressure     : " << _fPressure[j] << "\n"
                << "    density      : " << _fDensity[j] << "\n"
                << "    F_p          : " << Fp[j] << "\n"
                << "    sum d_ij p_j : " << sumDijPj[j] << "\n"
                << "    a_ii         : " << Aii[j] << "\n"
                << "    rho_adv      : " << Dadv[j] << "\n"
                << "    d_ii         : " << Dii[j] << "\n"
                << "    v_adv        : " << Vadv[j] << "\n"
                << "    F_adv        : " << Fadv[j] << "\n"
                << "    gradient     : " << _kernel.gradW(pos_ij) << "\n"
                << std::endl;
        }
    }
    std::cout << "---------------------------------------------\n\n" << std::endl;
}
