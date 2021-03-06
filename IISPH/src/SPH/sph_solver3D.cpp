#include "sph_solver3D.h"

/*--------------------------------------------Main functions--------------------------------------------------*/

void IISPHsolver3D::prepareSolver(std::vector<Vec3f> fluidPos, std::vector<Vec3f> boundaryPos) {
    // sample input fluid
    _fPosition = fluidPos;
    _fluidCount = _fPosition.size();

    // sample input boundaries
    _bPosition = boundaryPos;
    _inBoundaryCount = _bPosition.size();

    // sample global boundaries
    Sampler::cubeSurface(_bPosition, _pGridHelper.cellSize(), Vec3f(0.0f), _pGridHelper.size(), 1);
    _boundaryCount = _bPosition.size();

    // sample distance field
    Sampler::gridNodes(_sPosition, _sGridHelper.cellSize(), Vec3f(0.0f), _sGridHelper.size());
    _surfaceCount = _sPosition.size();

    std::cout << "\n"
        << "number of fluid particles    : " << _fluidCount    << "\n"
        << "number of boundary particles : " << _boundaryCount << "\n"
        << "number of surface nodes      : " << _surfaceCount  << "\n"
        << std::endl;

    // init smooth kernels
    _pKernel = CubicSpline(_h, 3);
    _sKernel = SimpleKernel(_h);

    // init other quantities
    _fDensity      = std::vector<Real> (_fluidCount, 0.0f);
    _fVelocity     = std::vector<Vec3f>(_fluidCount, Vec3f(0.0f));
    _fPressure     = std::vector<Real> (_fluidCount, 0.0f);
    _fColor        = std::vector<Vec3f>(_fluidCount, _denseColor);
    _bColor        = std::vector<Vec3f>(_boundaryCount, _wallColor);
    _Psi           = std::vector<Real> (_boundaryCount, 0.0f);
    _Dii           = std::vector<Vec3f>(_fluidCount, Vec3f(0.0f));
    _Aii           = std::vector<Real> (_fluidCount, 0.0f);
    _sumDijPj      = std::vector<Vec3f>(_fluidCount, Vec3f(0.0f));
    _Vadv          = std::vector<Vec3f>(_fluidCount, Vec3f(0.0f));
    _Dadv          = std::vector<Real> (_fluidCount, 0.0f);
    _Pl            = std::vector<Real> (_fluidCount, 0.0f);
    _Dcorr         = std::vector<Real> (_fluidCount, 0.0f);
    _Fadv          = std::vector<Vec3f>(_fluidCount, Vec3f(0.0f));
    _Fp            = std::vector<Vec3f>(_fluidCount, Vec3f(0.0f));
    _distanceField = std::vector<Real> (_surfaceCount, 0.0f);

    // init neighboring system
    _fGrid = std::vector<std::vector<Index>>((size_t)_pGridHelper.cellCount(), std::vector<Index>());
    _bGrid = std::vector<std::vector<Index>>((size_t)_pGridHelper.cellCount(), std::vector<Index>());
    buildNeighborGrid();

    _fNeighbors = std::vector<std::vector<Index>>(_fluidCount, std::vector<Index>());
    _bNeighbors = std::vector<std::vector<Index>>(_fluidCount, std::vector<Index>());
    searchNeighbors();

    // compute density number ones and for all
    #pragma omp parallel for
    for (int i = 0; i < _boundaryCount; i++)
        computePsi(i);

    // visualize initial fluid density
    #pragma omp parallel for
    for (int i = 0; i < _fluidCount; i++)
        computeDensity(i);

    visualizeFluidDensity();
}

void IISPHsolver3D::solveSimulation() {
    static Real count = 0.5f;

    auto start = Clock::now();
    buildNeighborGrid();
    searchNeighbors();
    std::chrono::milliseconds elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - start);
    searchNeighborsTime = (elapsed.count() + (count - 1) * searchNeighborsTime) / count;

    start = Clock::now();
    predictAdvection();
    elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - start);
    predictAdvectionTime = (elapsed.count() + (count - 1) * predictAdvectionTime) / count;

    start = Clock::now();
    pressureSolve();
    elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - start);
    solvePressureTime = (elapsed.count() + (count - 1) * solvePressureTime) / count;


    start = Clock::now();
    integration();
    elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - start);
    correctPositionTime = (elapsed.count() + (count - 1) * correctPositionTime) / count;

    visualizeFluidDensity();
    count += 0.5f;
}

void IISPHsolver3D::reconstructSurface() {
    static int count = 1;

    auto start = Clock::now();

    #pragma omp parallel for
    for (int i = 0; i < _surfaceCount; i++)
        computeDistanceField(i, 2.0f * _h);

    std::chrono::milliseconds elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - start);
    distanceFieldTime = (elapsed.count() + (count - 1) * distanceFieldTime) / count;

    start = Clock::now();

    generateIsoSurface();
    elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - start);
    marchingCubesTime = (elapsed.count() + (count - 1) * marchingCubesTime) / count;

    count++;
}

void IISPHsolver3D::showGeneralStatistics() {
    double sphComputation     = searchNeighborsTime + predictAdvectionTime + solvePressureTime + correctPositionTime;
    double surfaceComputation = distanceFieldTime + marchingCubesTime;

    std::cout
        << "|    SPH simulation         : " << std::setw(6) << sphComputation     << " ms\n"
        << "|    surface reconstruction : " << std::setw(6) << surfaceComputation << " ms\n"
        << std::endl;
}

void IISPHsolver3D::showDetailedStatistics() {
    std::cout
        << "|    search neighbors  : " << std::setw(6) << searchNeighborsTime  << " ms\n"
        << "|    predict advection : " << std::setw(6) << predictAdvectionTime << " ms\n"
        << "|    solve pressure    : " << std::setw(6) << solvePressureTime    << " ms\n"
        << "|    correct position  : " << std::setw(6) << correctPositionTime  << " ms\n"
        << "|    distance field    : " << std::setw(6) << distanceFieldTime    << " ms\n"
        << "|    marching cubes    : " << std::setw(6) << marchingCubesTime    << " ms\n"
        << std::endl;
}


/*-------------------------------------------Neighbor search------------------------------------------------*/

void IISPHsolver3D::buildNeighborGrid() {
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

void IISPHsolver3D::searchNeighbors() {
#pragma omp parallel for
    for (int i = 0; i < _fluidCount; i++) {
        // search for fluid neighbor particles
        size_t lastFluidSize = _fNeighbors[i].size();
        _fNeighbors[i].clear();
        _fNeighbors[i].reserve(lastFluidSize);
        findFluidNeighbors(_fNeighbors[i], _fPosition[i], 2 * _h);

        // search for boundary neighbor particles
        size_t lastBoundarySize = _bNeighbors[i].size();
        _bNeighbors[i].clear();
        _bNeighbors[i].reserve(lastBoundarySize);
        findBoundaryNeighbors(_bNeighbors[i], _fPosition[i], 2 * _h);
    }
}

void IISPHsolver3D::fillFluidGrid(int i) {
    int id = _pGridHelper.cellID(_fPosition[i]);

    if (_pGridHelper.isInsideGrid(id))
        _fGrid[id].push_back(i);
}

void IISPHsolver3D::fillBoundaryGrid(int i) {
    int id = _pGridHelper.cellID(_bPosition[i]);

    if (_pGridHelper.isInsideGrid(id))
        _bGrid[id].push_back(i);
}

void IISPHsolver3D::findFluidNeighbors(std::vector< Index >& neighbors, Vec3f position, const float radius) {
    std::vector<Index> neighborCells;
    Real  squaredRadius = square(radius);
    Real  distance = 0.0f;
    Index neighborID = 0;

    _pGridHelper.getNeighborCells(neighborCells, position, radius);

    for (size_t j = 0; j < neighborCells.size(); j++) {
        std::vector<Index> fluidInCell = _fGrid[neighborCells[j]];

        for (size_t k = 0; k < fluidInCell.size(); k++) {
            neighborID = fluidInCell[k];
            distance = (_fPosition[neighborID] - position).lengthSquare();

            if (distance < squaredRadius) {
                neighbors.push_back(neighborID);
            }
        }
    }
}

void IISPHsolver3D::findBoundaryNeighbors(std::vector< Index >& neighbors, Vec3f position, const float radius) {
    std::vector<Index> neighborCells;
    Real  squaredRadius = square(radius);
    Real  distance = 0.0f;
    Index neighborID = 0;

    _pGridHelper.getNeighborCells(neighborCells, position, radius);

    for (size_t j = 0; j < neighborCells.size(); j++) {
        std::vector<Index> boundaryInCell = _bGrid[neighborCells[j]];

        for (size_t k = 0; k < boundaryInCell.size(); k++) {
            neighborID = boundaryInCell[k];
            distance = (_bPosition[neighborID] - position).lengthSquare();

            if (distance < squaredRadius) {
                neighbors.push_back(neighborID);
            }
        }
    }
}



/*-----------------------------------------Particle simulation------------------------------------------------*/

void IISPHsolver3D::predictAdvection() {
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

void IISPHsolver3D::pressureSolve() {
    int l = 0;
    _avgDensity = 0.0f;

    while (((_avgDensity - _rho0) > _eta) || (l < 2))
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

void IISPHsolver3D::integration() {
#pragma omp parallel for
    for (int i = 0; i < _fluidCount; i++)
        computePressureForces(i);

#pragma omp parallel for
    for (int i = 0; i < _fluidCount; i++) {
        updateVelocity(i);
        updatePosition(i);
    }
}

void IISPHsolver3D::computePsi(int i) {
    Real sumK = 0.0f;
    Vec3f pos_ij;

    std::vector<Index> boundaryNeighbors;
    findBoundaryNeighbors(boundaryNeighbors, _bPosition[i], _h);

    for (Index& j : boundaryNeighbors) {
        pos_ij = _bPosition[i] - _bPosition[j];
        sumK += _pKernel.W(pos_ij);
    }

    _Psi[i] = _rho0 / sumK;
}

void IISPHsolver3D::computeDensity(int i) {
    _fDensity[i] = 0.0f;
    Vec3f pos_ij;

    for (Index& j : _fNeighbors[i]) {
        pos_ij = _fPosition[i] - _fPosition[j];
        _fDensity[i] += _m0 * _pKernel.W(pos_ij);
    }

    for (Index& j : _bNeighbors[i]) {
        pos_ij = _fPosition[i] - _bPosition[j];
        _fDensity[i] += _Psi[j] * _pKernel.W(pos_ij);
    }
}

void IISPHsolver3D::computeAdvectionForces(int i) {
    _Fadv[i].x = 0.0f;
    _Fadv[i].y = 0.0f;
    _Fadv[i].z = 0.0f;
    addBodyForce(i);
    addViscousForce(i);
}

void IISPHsolver3D::addBodyForce(int i) {
    _Fadv[i] += _m0 * _g;
}

void IISPHsolver3D::addViscousForce(int i) {
    Vec3f pos_ij;
    Vec3f vel_ij;

    for (Index& j : _fNeighbors[i])
        if (_fPosition[j] != _fPosition[i]) {
            pos_ij = _fPosition[i] - _fPosition[j];
            vel_ij = _fVelocity[i] - _fVelocity[j];
            _Fadv[i] += 2 * _nu * (square(_m0) / _fDensity[j]) * vel_ij.dotProduct(pos_ij) * _pKernel.gradW(pos_ij) / (pos_ij.lengthSquare() + 0.01 * square(_h));
        }
}

void IISPHsolver3D::predictVelocity(int i) {
    _Vadv[i] = _fVelocity[i] + _dt * _Fadv[i] / _m0;
}

void IISPHsolver3D::storeDii(int i) {
    _Dii[i].x = 0.0f;
    _Dii[i].y = 0.0f;
    _Dii[i].z = 0.0f;
    Vec3f pos_ij;

    for (Index& j : _fNeighbors[i])
        if (_fPosition[j] != _fPosition[i]) {
            pos_ij = _fPosition[i] - _fPosition[j];
            _Dii[i] += (-_m0 / square(_fDensity[i])) * _pKernel.gradW(pos_ij);
        }

    for (Index& j : _bNeighbors[i])
        if (_bPosition[j] != _fPosition[i]) {
            pos_ij = _fPosition[i] - _bPosition[j];
            _Dii[i] += (-_Psi[j] / square(_fDensity[i])) * _pKernel.gradW(pos_ij);
        }

    _Dii[i] *= square(_dt);
}

void IISPHsolver3D::predictDensity(int i) {
    _Dadv[i] = 0.0f;
    Vec3f pos_ij;
    Vec3f vel_adv_ij;

    for (Index& j : _fNeighbors[i])
        if (_fPosition[j] != _fPosition[i]) {
            pos_ij = _fPosition[i] - _fPosition[j];
            vel_adv_ij = _Vadv[i] - _Vadv[j];
            _Dadv[i] += _m0 * vel_adv_ij.dotProduct(_pKernel.gradW(pos_ij));
        }

    for (Index& j : _bNeighbors[i])
        if (_bPosition[j] != _fPosition[i]) {
            pos_ij = _fPosition[i] - _bPosition[j];
            vel_adv_ij = _Vadv[i];
            _Dadv[i] += _Psi[j] * vel_adv_ij.dotProduct(_pKernel.gradW(pos_ij));
        }

    _Dadv[i] *= _dt;
    _Dadv[i] += _fDensity[i];
}

void IISPHsolver3D::initPressure(int i) {
    _Pl[i] = 0.5f * _fPressure[i];
}

void IISPHsolver3D::storeAii(int i) {
    _Aii[i] = 0.0f;
    Vec3f pos_ij;
    Vec3f d_ji;

    for (Index& j : _fNeighbors[i])
        if (_fPosition[j] != _fPosition[i]) {
            pos_ij = _fPosition[i] - _fPosition[j];
            d_ji = -(square(_dt) * _m0 / square(_fDensity[i])) * (-_pKernel.gradW(pos_ij));
            _Aii[i] += _m0 * (_Dii[i] - d_ji).dotProduct(_pKernel.gradW(pos_ij));
        }

    for (Index& j : _bNeighbors[i])
        if (_bPosition[j] != _fPosition[i]) {
            pos_ij = _fPosition[i] - _bPosition[j];
            _Aii[i] += _Psi[j] * _Dii[i].dotProduct(_pKernel.gradW(pos_ij));
        }
}

void IISPHsolver3D::storeSumDijPj(int i) {
    _sumDijPj[i].x = 0.0f;
    _sumDijPj[i].y = 0.0f;
    _sumDijPj[i].z = 0.0f;
    Vec3f pos_ij;

    for (Index& j : _fNeighbors[i])
        if (_fPosition[j] != _fPosition[i]) {
            pos_ij = _fPosition[i] - _fPosition[j];
            _sumDijPj[i] += -(_m0 * _fPressure[j] / square(_fDensity[j])) * _pKernel.gradW(pos_ij);
        }

    _sumDijPj[i] *= square(_dt);
}

void IISPHsolver3D::computePressure(int i) {
    _Dcorr[i] = 0.0f;
    Vec3f pos_ij;
    Vec3f d_ji;
    Vec3f temp;

    for (Index& j : _fNeighbors[i])
        if (_fPosition[j] != _fPosition[i]) {
            pos_ij = _fPosition[i] - _fPosition[j];
            d_ji   = -(square(_dt) * _m0 / square(_fDensity[i])) * (-_pKernel.gradW(pos_ij));
            temp   = _sumDijPj[i] - _Dii[j] * _Pl[j] - (_sumDijPj[j] - d_ji * _Pl[i]);
            _Dcorr[i] += _m0 * temp.dotProduct(_pKernel.gradW(pos_ij));
        }

    for (Index& j : _bNeighbors[i])
        if (_bPosition[j] != _fPosition[i]) {
            pos_ij = _fPosition[i] - _bPosition[j];
            _Dcorr[i] += _Psi[j] * _sumDijPj[i].dotProduct(_pKernel.gradW(pos_ij));
        }

    _Dcorr[i] += _Dadv[i];

    Real previousPl = _Pl[i];
    if (std::abs(_Aii[i]) > std::numeric_limits<Real>::epsilon())
        _Pl[i] = (1 - _omega) * previousPl + (_omega / _Aii[i]) * (_rho0 - _Dcorr[i]);
    else
        _Pl[i] = 0.0;

    _fPressure[i] = std::fmax(_Pl[i], 0.0f);
    _Pl[i] = _fPressure[i];
    _Dcorr[i] += _Aii[i] * previousPl;
}

void IISPHsolver3D::computeError()
{
    _avgDensity = 0.0;
    for (int i = 0; i < _fluidCount; i++)
        _avgDensity += _Dcorr[i];

    _avgDensity /= _fPosition.size();
}

void IISPHsolver3D::computePressureForces(int i) {
    _Fp[i].x = 0.0f;
    _Fp[i].y = 0.0f;
    _Fp[i].z = 0.0f;
    Vec3f pos_ij;

    for (Index& j : _fNeighbors[i])
        if (_fPosition[j] != _fPosition[i]) {
            pos_ij = _fPosition[i] - _fPosition[j];
            _Fp[i] += -square(_m0) * (_fPressure[i] / square(_fDensity[i]) + _fPressure[j] / square(_fDensity[j])) * _pKernel.gradW(pos_ij);
        }

    for (Index& j : _bNeighbors[i])
        if (_bPosition[j] != _fPosition[i]) {
            pos_ij = _fPosition[i] - _bPosition[j];
            _Fp[i] += -_m0 * _Psi[j] * (_fPressure[i] / square(_fDensity[i])) * _pKernel.gradW(pos_ij);
        }
}

void IISPHsolver3D::updateVelocity(int i) {
    _fVelocity[i] = _Vadv[i] + _dt * _Fp[i] / _m0;
}

void IISPHsolver3D::updatePosition(int i) {

    if (_pGridHelper.isInsideGrid(_fPosition[i] + _dt * _fVelocity[i]))
        _fPosition[i] += _dt * _fVelocity[i];
    else {
        debugCrash(i);
    }
}



/*---------------------------------------Surface reconstruction----------------------------------------------*/

void IISPHsolver3D::computeDistanceField(int i, const float radius) {
    Vec3f sumX = Vec3f(0.0f);
    Real  sumK = 0.0f;
    Real  temp = 0.0f;
    Vec3f pos_ij;

    std::vector<Index> neighbors;
    findFluidNeighbors(neighbors, _sPosition[i], radius);

    for (Index& j : neighbors) {
        pos_ij = _sPosition[i] - _fPosition[j];
        temp   = _sKernel.W(pos_ij);
        sumX  += _fPosition[j] * temp;
        sumK  += temp;
    }

    if (std::abs(sumK) < std::numeric_limits<Real>::epsilon()) {
        if (std::abs(sumX.length()) < std::numeric_limits<Real>::epsilon())
            _distanceField[i] = (_sPosition[i]).length() - _h / 2;
        else
            _distanceField[i] = 0.0f;
    }

    else {
        _distanceField[i] = (_sPosition[i] - sumX / sumK).length() - _h / 2;
    }
}

void IISPHsolver3D::generateIsoSurface() {
    _isoSurface.GenerateSurface(
        _distanceField.data(), 0.0f,
        _sGridHelper.resX(), _sGridHelper.resY(), _sGridHelper.resZ(),
        _sGridHelper.cellSize(), _sGridHelper.cellSize(), _sGridHelper.cellSize()
    );
}



/*----------------------------------------Debug / visualization-----------------------------------------------*/

void IISPHsolver3D::visualizeFluidDensity() {
    for (Index i = 0; i < _fluidCount; i++) {
        _fColor[i].x = _lightColor.x + (_fDensity[i] / _rho0) * (_denseColor.x - _lightColor.x);
        _fColor[i].y = _lightColor.y + (_fDensity[i] / _rho0) * (_denseColor.y - _lightColor.y);
        _fColor[i].z = _lightColor.z + (_fDensity[i] / _rho0) * (_denseColor.z - _lightColor.z);
    }
}

void IISPHsolver3D::visualizeBoundaryDensity() {
    for (Index i = 0; i < _boundaryCount; i++) {
        _bColor[i].x = _lightColor.x + (_Psi[i] / _rho0) * (_wallColor.x - _lightColor.x);
        _bColor[i].y = _lightColor.y + (_Psi[i] / _rho0) * (_wallColor.y - _lightColor.y);
        _bColor[i].z = _lightColor.z + (_Psi[i] / _rho0) * (_wallColor.z - _lightColor.z);
    }
}

void IISPHsolver3D::visualizeFluidNeighbors(int i) {

    for (Index& j : _fNeighbors[i])
        _fColor[j] = _greenColor;

    for (Index& j : _bNeighbors[i])
        _bColor[j] = _pinkColor;

    _fColor[i] = _redColor;
}

void IISPHsolver3D::debugCrash(int i) {
    std::cout
        << "position     : " << _fPosition[i] << "\n"
        << "velocity     : " << _fVelocity[i] << "\n"
        << "pressure     : " << _fPressure[i] << "\n"
        << "density      : " << _fDensity[i]  << "\n"
        << "F_p          : " << _Fp[i]        << "\n"
        << "rho_corr     : " << _Dcorr[i]     << "\n"
        << "sum d_ij p_j : " << _sumDijPj[i]  << "\n"
        << "a_ii         : " << _Aii[i]       << "\n"
        << "rho_adv      : " << _Dadv[i]      << "\n"
        << "d_ii         : " << _Dii[i]       << "\n"
        << "v_adv        : " << _Vadv[i]      << "\n"
        << "F_adv        : " << _Fadv[i]      << "\n"
        << std::endl;

    std::cout << "neighbors : \n";

    Vec3f pos_ij;

    for (Index j : _fNeighbors[i]) {
        if (_fPosition[j] != _fPosition[i]) {
            pos_ij = _fPosition[i] - _fPosition[j];

            std::cout
                << "    position     : " << _fPosition[j] << "\n"
                << "    velocity     : " << _fVelocity[j] << "\n"
                << "    pressure     : " << _fPressure[j] << "\n"
                << "    density      : " << _fDensity[j]  << "\n"
                << "    F_p          : " << _Fp[j]        << "\n"
                << "    rho_corr     : " << _Dcorr[i]     << "\n"
                << "    sum d_ij p_j : " << _sumDijPj[j]  << "\n"
                << "    a_ii         : " << _Aii[j]       << "\n"
                << "    rho_adv      : " << _Dadv[j]      << "\n"
                << "    d_ii         : " << _Dii[j]       << "\n"
                << "    v_adv        : " << _Vadv[j]      << "\n"
                << "    F_adv        : " << _Fadv[j]      << "\n"
                << "    gradient     : " << _pKernel.gradW(pos_ij) << "\n"
                << std::endl;
        }
    }
    std::cout << "---------------------------------------------\n\n" << std::endl;
}



/*--------------------------------------------In development---------------------------------------------------*/

void concat(std::vector<Vec3f>& vector1, std::vector<Vec3f>& vector2) {
    vector1.insert(vector1.end(), vector2.begin(), vector2.end());
}

std::vector<Vec3f> triangle_to_set_of_points(Vec3f p1, Vec3f p2, Vec3f p3, Real particle_radius) {
    Vec3f bary = (p1 + p2 + p3) / 3; // barycenter
    Real a = p2.distanceTo(p1);
    Real b = p3.distanceTo(p2);
    Real c = p1.distanceTo(p3);

    Real p = a + b + c;                             // triangle perimeter
    Real s = p / 2;                                 // semi-perimeter
    Real A = sqrt(s * (s - a) * (s - b) * (s - c)); // triangle area
    Real r = 2 * A / p;                             // inner circle radius

    std::vector<Vec3f> points = std::vector<Vec3f>();

    if (particle_radius < r ) { // triangle is big enough
        std::vector<Vec3f> points1 = triangle_to_set_of_points(p1, p2, bary, particle_radius);
        std::vector<Vec3f> points2 = triangle_to_set_of_points(p1, p3, bary, particle_radius);
        std::vector<Vec3f> points3 = triangle_to_set_of_points(p2, p3, bary, particle_radius);

        concat(points, points1);
        concat(points, points2);
        concat(points, points3);

        points.push_back(bary);

        return points;
    }

    return {};
}

std::vector<Vec3f> mesh_to_set_of_points(std::vector<Vec3f> points, std::vector<Index> triangles, Real particle_radius) {
    std::vector<Vec3f> r_points = std::vector<Vec3f>();

    for (Index t = 0; t < triangles.size(); t += 3) {
        Vec3f p1 = points[triangles[t]];
        Vec3f p2 = points[triangles[t + 1]];
        Vec3f p3 = points[triangles[t + 2]];

        std::vector<Vec3f> new_points = triangle_to_set_of_points(p1, p2, p3, particle_radius);
        concat(r_points, new_points);
    }

    return r_points;
}
