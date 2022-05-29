#include "iisph_solver.h"


/*--------------------------------------------Main functions--------------------------------------------------*/

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
    _l = 0.5 * _h;
    _r = static_cast<Real>(_resX) - 0.5 * _h;
    _b = 0.5 * _h;
    _t = static_cast<Real>(_resY) - 0.5 * _h;

    // color particles
    _color.clear();
    for (Index i = 0; i < _fluidCount; i++)
        _color.push_back(denseColor);

    // init other particle quantities
    _velocity = std::vector<Vec2f>(_fluidCount, Vec2f(0, 0));
    _pressure = std::vector<Real> (_fluidCount, 0);
    _density  = std::vector<Real> (_fluidCount, 0);

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

void IISPHsolver::buildNeighborGrid()
{
    for (auto& indices : _neighborsGrid)
        indices.clear();

    for (int i = 0; i < _fluidCount; i++) {
        int id = cellID(_position[i]);

        if(isInsideGrid(id))
            _neighborsGrid[id].push_back(i);
    }

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

void IISPHsolver::updateColor() {
    for (Index i = 0; i < _fluidCount; i++) {
        _color[i].x = lightColor.x + (_density[i] / _rho0) * (denseColor.x - lightColor.x);
        _color[i].y = lightColor.y + (_density[i] / _rho0) * (denseColor.y - lightColor.y);
        _color[i].z = lightColor.z + (_density[i] / _rho0) * (denseColor.z - lightColor.z);
    }
}



/*-------------------------------------------Neighbor search------------------------------------------------*/

void IISPHsolver::get9NeighborCells(std::vector<Index>& neighbors, Vec2f particle, const int radius) {
    Vec2i cell = cellPos(particle);
    int   id   = cellID(particle);

    if (!isInsideGrid(id)) {
        neighbors.clear();
        return;
    }

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

Vec2i IISPHsolver::cellPos(Vec2f particle) {
    Vec2i cell;
    cell.x = std::floor(particle.x);
    cell.y = std::floor(particle.y);
    return cell;
}

Index IISPHsolver::cellID(Vec2f particle) {
    Vec2i cell = cellPos(particle);
    return cellID(cell.x, cell.y);
}

Index IISPHsolver::cellID(int i, int j) {
    return i + j * _resX;
}

bool IISPHsolver::isInsideGrid(int id) {
    return id >= 0 && id < _resX * _resY;
}

void IISPHsolver::findNeighbors(int particleID, const int radius) {
    size_t lastFluidSize = _fluidNeighbors[particleID].size();
    _fluidNeighbors[particleID].clear();
    _fluidNeighbors.reserve(lastFluidSize);

    std::vector<Index> neighborCells;
    Real squaredRadius = square(radius);
    Real distance      = 0.0f;
    Index neighborID   = 0;

    get9NeighborCells(neighborCells, _position[particleID], radius);

    for (size_t i = 0; i < neighborCells.size(); i++)
    {
        std::vector<Index> particlesInCell = _neighborsGrid[neighborCells[i]];

        for (size_t j = 0; j < particlesInCell.size(); j++)
        {
            neighborID = particlesInCell[j];
            distance = (_position[neighborID] - _position[particleID]).lengthSquare();

            if (distance < squaredRadius) {
                _fluidNeighbors[particleID].push_back(neighborID);
            }
        }
    }
}



/*------------------------------------------Fluid simulation-------------------------------------------------*/

void IISPHsolver::computeDensity(int i) {
    _density[i] = 0.0f;
    Vec2f pos_ij;

    std::vector<Index> neighbors = _fluidNeighbors[i];
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

    std::vector<Index> neighbors = _fluidNeighbors[i];
    for (Index& j : neighbors)
        if (_position[j] != _position[i]) {
            pos_ij = _position[i] - _position[j];
            vel_ij = _velocity[i] - _velocity[j];
            Fadv[i] += 2 * _nu * (square(_m0) / _density[j]) * vel_ij.dotProduct(pos_ij) * _kernel.gradW(pos_ij) / (pos_ij.lengthSquare() + 0.01 * _h);
        }
}

void IISPHsolver::predictVelocity(int i) {
    Vadv[i] = _velocity[i] + _dt * Fadv[i] / _m0;
}

void IISPHsolver::storeDii(int i) {
    Dii[i].x = 0.0f;
    Dii[i].y = 0.0f;
    Vec2f pos_ij;

    std::vector<Index> neighbors = _fluidNeighbors[i];
    for (Index& j : neighbors)
        if (_position[j] != _position[i]) {
            pos_ij = _position[i] - _position[j];
            Dii[i] += square(_dt) * (-_m0 / square(_density[i])) * _kernel.gradW(pos_ij);
        }
}

void IISPHsolver::predictDensity(int i) {
    Dadv[i] = _density[i];
    Vec2f pos_ij;
    Vec2f vel_adv_ij;

    std::vector<Index> neighbors = _fluidNeighbors[i];
    for (Index& j : neighbors)
        if (_position[j] != _position[i]) {
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

    std::vector<Index> neighbors = _fluidNeighbors[i];
    for (Index& j : neighbors)
        if (_position[j] != _position[i]) {
            pos_ij = _position[i] - _position[j];
            d_ji = -(square(_dt) * _m0) / square(_density[i]) * (-_kernel.gradW(pos_ij));
            Aii[i] += _m0 * (Dii[i] - d_ji).dotProduct(_kernel.gradW(pos_ij));       
        }
}

void IISPHsolver::storeSumDijPj(int i) {
    sumDijPj[i].x = 0.0f;
    sumDijPj[i].y = 0.0f;
    Vec2f pos_ij;

    std::vector<Index> neighbors = _fluidNeighbors[i];
    for (Index& j : neighbors)
        if (_position[j] != _position[i]) {
            pos_ij = _position[i] - _position[j];
            sumDijPj[i] += square(_dt) * (-_m0 / square(_density[j])) * _pressure[j] * _kernel.gradW(pos_ij);
        }
}

void IISPHsolver::computePressure(int i) {
    Real  sum = 0.0f;
    Vec2f pos_ij;
    Vec2f d_ji;
    Vec2f aux;

    std::vector<Index> neighbors = _fluidNeighbors[i];
    for (Index& j : neighbors)
        if (_position[j] != _position[i]) {
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
    
    _pressure[i] = std::fmax(Pl[i], 0.0f);
    Pl[i] = _pressure[i];
    Dcorr[i] += Aii[i] * previousPl;
}

void IISPHsolver::computeError()
{
    _avgDensity = 0.0;
    for (int i = 0; i < _fluidCount; i++)
        _avgDensity += Dcorr[i];

    _avgDensity /= _position.size();
}

void IISPHsolver::computePressureForces(int i) {
    Fp[i].x = 0.0f;
    Fp[i].y = 0.0f;
    Vec2f pos_ij;

    std::vector<Index> neighbors = _fluidNeighbors[i];
    for (Index& j : neighbors)
        if (_position[j] != _position[i]) {
            pos_ij = _position[i] - _position[j];
            Fp[i] += - square(_m0) * (_pressure[i] / square(_density[i]) + _pressure[j] / square(_density[j])) * _kernel.gradW(pos_ij);
        }
}

void IISPHsolver::updateVelocity(int i) {
    _velocity[i] = Vadv[i] + _dt * Fp[i] / _m0;
}

void IISPHsolver::updatePosition(int i) {
    Vec2f previousPos = _position[i];
    _position[i] += _dt * _velocity[i];

    if (!isInsideGrid(cellID(_position[i]))) {
        _position[i] = previousPos;
        debugCrash(i);
    }
}

void IISPHsolver::debugCrash(int i) {
    std::cout
        << "position     : " << _position[i] << "\n"
        << "velocity     : " << _velocity[i] << "\n"
        << "F_p          : " << Fp[i] << "\n"
        << "pressure     : " << _pressure[i] << "\n"
        << "sum d_ij p_j : " << sumDijPj[i] << "\n"
        << "a_ii         : " << Aii[i] << "\n"
        << "rho_adv      : " << Dadv[i] << "\n"
        << "d_ii         : " << Dii[i] << "\n"
        << "v_adv        : " << Vadv[i] << "\n"
        << "F_adv        : " << Fadv[i] << "\n"
        << "density      : " << _density[i] << "\n"
        << std::endl;

    std::cout << "neighbors : \n";

    std::vector<Index> neighbors = _fluidNeighbors[i];
    for (Index j : neighbors) {
        if (_position[j] != _position[i]) {
            Vec2f pos_ij = _position[i] - _position[j];

            std::cout
                << "    position     : " << _position[j] << "\n"
                << "    velocity     : " << _velocity[j] << "\n"
                << "    F_p          : " << Fp[j] << "\n"
                << "    pressure     : " << _pressure[j] << "\n"
                << "    sum d_ij p_j : " << sumDijPj[j] << "\n"
                << "    a_ii         : " << Aii[j] << "\n"
                << "    rho_adv      : " << Dadv[j] << "\n"
                << "    d_ii         : " << Dii[j] << "\n"
                << "    v_adv        : " << Vadv[j] << "\n"
                << "    F_adv        : " << Fadv[j] << "\n"
                << "    gradient     : " << _kernel.gradW(pos_ij) << "\n"
                << "    density      : " << _density[j] << "\n"
                << std::endl;
        }
    }
    std::cout << "---------------------------------------------\n\n" << std::endl;
}
