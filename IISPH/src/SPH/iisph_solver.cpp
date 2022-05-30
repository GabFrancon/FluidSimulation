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
    sampleFluidCube(1, 1, fluidWidth + 1, fluidHeight + 1);
    _fluidCount = _fPosition.size();
    _fColor = std::vector<glm::vec3>(_fluidCount, denseColor);
 
    // sample boundaries
    _bPosition.clear();
    sampleBoundaryCube(0, 0, _resX, _resY);
    _boundaryCount = _bPosition.size();
    _bColor = std::vector<glm::vec3>(_boundaryCount, wallColor);

    // walls
    _l = 2.0f * _h;
    _r = static_cast<Real>(_resX) - 2.0f * _h;
    _b = 2.0f * _h;
    _t = static_cast<Real>(_resY) - 2.0f * _h;

    // init other particle quantities
    _fVelocity  = std::vector<Vec2f>(_fluidCount, Vec2f(0, 0));
    _bVelocity = std::vector<Vec2f>(_boundaryCount, Vec2f(0, 0));

    _fDensity = std::vector<Real>(_fluidCount, 0);
    _bDensity = std::vector<Real>(_boundaryCount, 0);

    _fNeighbors = std::vector<std::vector<Index>>(_fluidCount, std::vector<Index>());
    _bNeighbors = std::vector<std::vector<Index>>(_fluidCount, std::vector<Index>());

    _fPressure = std::vector<Real>(_fluidCount, 0);

    Dii      = std::vector<Vec2f>(_fluidCount, Vec2f(0, 0));
    Aii      = std::vector<Real> (_fluidCount, 0);
    sumDijPj = std::vector<Vec2f>(_fluidCount, Vec2f(0, 0));
    Vadv     = std::vector<Vec2f>(_fluidCount, Vec2f(0, 0));
    Dadv     = std::vector<Real> (_fluidCount, 0);
    Pl       = std::vector<Real> (_fluidCount, 0);
    Dcorr    = std::vector<Real> (_fluidCount, 0);
    Fadv     = std::vector<Vec2f>(_fluidCount, Vec2f(0, 0));
    Fp       = std::vector<Vec2f>(_fluidCount, Vec2f(0, 0));

    initNeighbors();
}

void IISPHsolver::sampleFluidCube(int bottomX, int bottomY, int topX, int topY) {
    for (int j = bottomY; j < topY; j++) {
        for (int i = bottomX; i < topX; i++) {
            _fPosition.push_back(Vec2f(i + 0.25, j + 0.25));
            _fPosition.push_back(Vec2f(i + 0.75, j + 0.25));
            _fPosition.push_back(Vec2f(i + 0.25, j + 0.75));
            _fPosition.push_back(Vec2f(i + 0.75, j + 0.75));
        }
    }
}

void IISPHsolver::sampleBoundaryCube(int bottomX, int bottomY, int topX, int topY) {
    for (int i = bottomX; i < topX; i++) {
        _bPosition.push_back(Vec2f(i + 0.25, bottomY + 0.25));
        _bPosition.push_back(Vec2f(i + 0.75, bottomY + 0.25));
        _bPosition.push_back(Vec2f(i + 0.25, bottomY + 0.75));
        _bPosition.push_back(Vec2f(i + 0.75, bottomY + 0.75));
    }
    for (int i = bottomX; i < topX; i++) {
        _bPosition.push_back(Vec2f(i + 0.25, topY - 0.25));
        _bPosition.push_back(Vec2f(i + 0.75, topY - 0.25));
        _bPosition.push_back(Vec2f(i + 0.25, topY - 0.75));
        _bPosition.push_back(Vec2f(i + 0.75, topY - 0.75));
    }
    for (int j = bottomY + 1; j < topY - 1; j++) {
        _bPosition.push_back(Vec2f(bottomX + 0.25, j + 0.25));
        _bPosition.push_back(Vec2f(bottomX + 0.75, j + 0.25));
        _bPosition.push_back(Vec2f(bottomX + 0.25, j + 0.75));
        _bPosition.push_back(Vec2f(bottomX + 0.75, j + 0.75));
    }
    for (int j = bottomY + 1; j < topY - 1; j++) {
        _bPosition.push_back(Vec2f(topX - 0.25, j + 0.25));
        _bPosition.push_back(Vec2f(topX - 0.75, j + 0.25));
        _bPosition.push_back(Vec2f(topX - 0.25, j + 0.75));
        _bPosition.push_back(Vec2f(topX - 0.75, j + 0.75));
    }
}

void IISPHsolver::initNeighbors() {
    _fGrid = std::vector<std::vector<Index>>(_resX * (size_t)_resY, std::vector<Index>());
    _bGrid = std::vector<std::vector<Index>>(_resX * (size_t)_resY, std::vector<Index>());

    buildNeighborGrid();

#pragma omp parallel for
    for (int i = 0; i < _boundaryCount; i++)
        computeBoundaryDensity(i);


}

void IISPHsolver::update() {
    updateNeighbors();

    predictAdvection();
    pressureSolve();
    integration();
    resolveCollision();

    visualizeFluidDensity();
    viualizeFluidNeighbors(500);
}

void IISPHsolver::updateNeighbors() {
    buildNeighborGrid();

#pragma omp parallel for
    for (int i = 0; i < _fluidCount; i++)
        findFluidNeighbors(i, 2 * _h);
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



/*-------------------------------------------Neighbor search------------------------------------------------*/

void IISPHsolver::buildNeighborGrid() {
    for (auto& fIndices : _fGrid) {
        size_t lastFluidGridSize = fIndices.size();
        fIndices.clear();
        fIndices.reserve(lastFluidGridSize);
    }

    for (auto& bIndices : _bGrid) {
        size_t lastBoundaryGridSize = bIndices.size();
        bIndices.clear();
        bIndices.reserve(lastBoundaryGridSize);
    }

    for (int i = 0; i < _fluidCount; i++)
        fillFluidGrid(i);

    for (int i = 0; i < _boundaryCount; i++)
        fillBoundaryGrid(i);
}

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

void IISPHsolver::fillFluidGrid(int i) {
    int id = cellID(_fPosition[i]);

    if (isInsideGrid(id))
        _fGrid[id].push_back(i);
}

void IISPHsolver::fillBoundaryGrid(int i) {
    int id = cellID(_bPosition[i]);

    if (isInsideGrid(id))
        _bGrid[id].push_back(i);
}

void IISPHsolver::findFluidNeighbors(int i, const int radius) {
    size_t lastFluidSize = _fNeighbors[i].size();
    _fNeighbors[i].clear();
    _fNeighbors.reserve(lastFluidSize);

    size_t lastBoundarySize = _bNeighbors[i].size();
    _bNeighbors[i].clear();
    _bNeighbors.reserve(lastBoundarySize);

    std::vector<Index> neighborCells;
    Real  squaredRadius = square(radius);
    Real  distance      = 0.0f;
    Index neighborID    = 0;

    getNeighborCells(neighborCells, _fPosition[i], radius);

    for (size_t j = 0; j < neighborCells.size(); j++) {

        // find fluid neighbors
        std::vector<Index> fluidInCell = _fGrid[neighborCells[j]];

        for (size_t k = 0; k < fluidInCell.size(); k++) {
            neighborID = fluidInCell[k];
            distance = (_fPosition[neighborID] - _fPosition[i]).lengthSquare();

            if (distance < squaredRadius) {
                _fNeighbors[i].push_back(neighborID);
            }
        }
        // find boundary neighbors
        std::vector<Index> boundaryInCell = _bGrid[neighborCells[j]];

        for (size_t k = 0; k < boundaryInCell.size(); k++) {
            neighborID = boundaryInCell[k];
            distance = (_bPosition[neighborID] - _fPosition[i]).lengthSquare();

            if (distance < squaredRadius) {
                _bNeighbors[i].push_back(neighborID);
            }
        }
    }
}

void IISPHsolver::findBoundaryNeighbors(std::vector< Index >& neighbors, int i, const int radius) {
    size_t lastSize = neighbors.size();
    neighbors.clear();
    neighbors.reserve(lastSize);

    std::vector<Index> neighborsCells;
    getNeighborCells(neighborsCells, _bPosition[i], radius);

    for (size_t j = 0; j < neighborsCells.size(); ++j) {
        std::vector<Index> boundaryInCell = _bGrid[neighborsCells[j]];
        neighbors.insert(neighbors.end(), boundaryInCell.begin(), boundaryInCell.end());
    }
}



/*------------------------------------------Fluid simulation-------------------------------------------------*/

void IISPHsolver::computeBoundaryDensity(int i) {
    _bDensity[i] = 0.0f;
    Vec2f pos_ij;

    std::vector<Index> neighbors;
    findBoundaryNeighbors(neighbors, i, _h);

    for (Index& j : neighbors) {
        pos_ij = _bPosition[i] - _bPosition[j];
        _bDensity[i] += _kernel.W(pos_ij);
    }
    _bDensity[i] = _rho0 / _bDensity[i];
}

void IISPHsolver::computeDensity(int i) {
    _fDensity[i] = 0.0f;
    Vec2f pos_ij;

    std::vector<Index> neighbors = _fNeighbors[i];
    for (Index& j : neighbors) {
        pos_ij = _fPosition[i] - _fPosition[j];
        _fDensity[i] += _kernel.W(pos_ij);
    }
    _fDensity[i] = _m0 * _fDensity[i];
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

    std::vector<Index> neighbors = _fNeighbors[i];
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

    std::vector<Index> neighbors = _fNeighbors[i];
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

    std::vector<Index> neighbors = _fNeighbors[i];
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

    std::vector<Index> neighbors = _fNeighbors[i];
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

    std::vector<Index> neighbors = _fNeighbors[i];
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

    std::vector<Index> neighbors = _fNeighbors[i];
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

    std::vector<Index> neighbors = _fNeighbors[i];
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



/*----------------------------------------Debug / visualization-----------------------------------------------*/

void IISPHsolver::visualizeFluidDensity() {
    for (Index i = 0; i < _fluidCount; i++) {
        _fColor[i].x = lightColor.x + (_fDensity[i] / _rho0) * (denseColor.x - lightColor.x);
        _fColor[i].y = lightColor.y + (_fDensity[i] / _rho0) * (denseColor.y - lightColor.y);
        _fColor[i].z = lightColor.z + (_fDensity[i] / _rho0) * (denseColor.z - lightColor.z);
    }
    for (Index i = 0; i < _boundaryCount; i++)
        _bColor[i] = wallColor;
}

void IISPHsolver::visualizeBoundaryDensity() {
    for (Index i = 0; i < _boundaryCount; i++) {
        _bColor[i].x = lightColor.x + (_bDensity[i] / _rho0) * (wallColor.x - lightColor.x);
        _bColor[i].y = lightColor.y + (_bDensity[i] / _rho0) * (wallColor.y - lightColor.y);
        _bColor[i].z = lightColor.z + (_bDensity[i] / _rho0) * (wallColor.z - lightColor.z);
    }
}

void IISPHsolver::viualizeFluidNeighbors(int i) {

    for (int j = 0; j < _fNeighbors[i].size(); j++)
        _fColor[_fNeighbors[i][j]] = greenColor;

    for (int j = 0; j < _bNeighbors[i].size(); j++)
        _bColor[_bNeighbors[i][j]] = pinkColor;

    _fColor[i] = redColor;
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

    std::vector<Index> neighbors = _fNeighbors[i];
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
