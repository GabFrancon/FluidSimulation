#include "sph_solver3D.h"

/*--------------------------------------------Main functions--------------------------------------------------*/

void IISPHsolver3D::init(Vec3i gridSize, Vec3i fluidSize) {
    // - create a grid of size gridSize
    // - init a fluid mass of size fluidSize
    // - each cell containing the fluid is sampled with 2x2 particles
    _gridHelper = GridHelper(1.0f, gridSize);
    _kernel = CubicSpline(_h, 3);

    // sample fluid mass
    _fPosition.clear();
    sampleFluidCube(Vec3i(1), fluidSize + 1);
    _fluidCount = _fPosition.size();
    _fColor = std::vector<Vec3f>(_fluidCount, _denseColor);

    // sample boundaries
    _bPosition.clear();
    sampleBoundaryCube(Vec3i(0), gridSize);
    _boundaryCount = _bPosition.size();
    _bColor = std::vector<Vec3f>(_boundaryCount, _wallColor);

    // init other particle quantities
    _fDensity  = std::vector<Real> (_fluidCount, 0.0f);
    _fVelocity = std::vector<Vec3f>(_fluidCount, Vec3f(0.0f));
    _fPressure = std::vector<Real> (_fluidCount, 0.0f);

    _Psi      = std::vector<Real> (_boundaryCount, 0.0f);
    _Dii      = std::vector<Vec3f>(_fluidCount, Vec3f(0.0f));
    _Aii      = std::vector<Real> (_fluidCount, 0.0f);
    _sumDijPj = std::vector<Vec3f>(_fluidCount, Vec3f(0.0f));
    _Vadv     = std::vector<Vec3f>(_fluidCount, Vec3f(0.0f));
    _Dadv     = std::vector<Real> (_fluidCount, 0.0f);
    _Pl       = std::vector<Real> (_fluidCount, 0.0f);
    _Dcorr    = std::vector<Real> (_fluidCount, 0.0f);
    _Fadv     = std::vector<Vec3f>(_fluidCount, Vec3f(0.0f));
    _Fp       = std::vector<Vec3f>(_fluidCount, Vec3f(0.0f));

    // init neighboring system
    _fGrid = std::vector<std::vector<Index>>((size_t)_gridHelper.cellCount(), std::vector<Index>());
    _bGrid = std::vector<std::vector<Index>>((size_t)_gridHelper.cellCount(), std::vector<Index>());
    buildNeighborGrid();

    _fNeighbors = std::vector<std::vector<Index>>(_fluidCount, std::vector<Index>());
    _bNeighbors = std::vector<std::vector<Index>>(_fluidCount, std::vector<Index>());
    searchNeighbors();

    // compute fluid and boundary density for vizualisation
#pragma omp parallel for
    for (int i = 0; i < _boundaryCount; i++)
        computePsi(i);

#pragma omp parallel for
    for (int i = 0; i < _fluidCount; i++)
        computeDensity(i);

    visualizeFluidDensity();
}

void IISPHsolver3D::sampleFluidCube(Vec3i bottomLeft, Vec3i topRight) {
    for (int k = bottomLeft.z; k < topRight.z; k++)
        for (int j = bottomLeft.y; j < topRight.y; j++)
            for (int i = bottomLeft.x; i < topRight.x; i++) {
                _fPosition.push_back(Vec3f(i + 0.25, j + 0.25, k + 0.25));
                _fPosition.push_back(Vec3f(i + 0.75, j + 0.25, k + 0.25));
                _fPosition.push_back(Vec3f(i + 0.25, j + 0.75, k + 0.25));
                _fPosition.push_back(Vec3f(i + 0.75, j + 0.75, k + 0.25));
                _fPosition.push_back(Vec3f(i + 0.25, j + 0.25, k + 0.75));
                _fPosition.push_back(Vec3f(i + 0.75, j + 0.25, k + 0.75));
                _fPosition.push_back(Vec3f(i + 0.25, j + 0.75, k + 0.75));
                _fPosition.push_back(Vec3f(i + 0.75, j + 0.75, k + 0.75));
            }
}

void IISPHsolver3D::sampleBoundaryCube(Vec3i bottomLeft, Vec3i topRight) {
    for (int i = bottomLeft.x; i < topRight.x; i++) 
        for (int j = bottomLeft.y; j < topRight.y; j++) {
            // back plane
            _bPosition.push_back(Vec3f(i + 0.25, j + 0.25, bottomLeft.z + 0.25));
            _bPosition.push_back(Vec3f(i + 0.75, j + 0.25, bottomLeft.z + 0.25));
            _bPosition.push_back(Vec3f(i + 0.25, j + 0.75, bottomLeft.z + 0.25));
            _bPosition.push_back(Vec3f(i + 0.75, j + 0.75, bottomLeft.z + 0.25));
            _bPosition.push_back(Vec3f(i + 0.25, j + 0.25, bottomLeft.z + 0.75));
            _bPosition.push_back(Vec3f(i + 0.75, j + 0.25, bottomLeft.z + 0.75));
            _bPosition.push_back(Vec3f(i + 0.25, j + 0.75, bottomLeft.z + 0.75));
            _bPosition.push_back(Vec3f(i + 0.75, j + 0.75, bottomLeft.z + 0.75));
            
            // front plane
            _bPosition.push_back(Vec3f(i + 0.25, j + 0.25, topRight.z - 0.25));
            _bPosition.push_back(Vec3f(i + 0.75, j + 0.25, topRight.z - 0.25));
            _bPosition.push_back(Vec3f(i + 0.25, j + 0.75, topRight.z - 0.25));
            _bPosition.push_back(Vec3f(i + 0.75, j + 0.75, topRight.z - 0.25));
            _bPosition.push_back(Vec3f(i + 0.25, j + 0.25, topRight.z - 0.75));
            _bPosition.push_back(Vec3f(i + 0.75, j + 0.25, topRight.z - 0.75));
            _bPosition.push_back(Vec3f(i + 0.25, j + 0.75, topRight.z - 0.75));
            _bPosition.push_back(Vec3f(i + 0.75, j + 0.75, topRight.z - 0.75));
        }

    for (int i = bottomLeft.x; i < topRight.x; i++)
        for (int k = bottomLeft.z; k < topRight.z; k++) {
            // bottom plane
            _bPosition.push_back(Vec3f(i + 0.25, bottomLeft.y + 0.25, k + 0.25));
            _bPosition.push_back(Vec3f(i + 0.75, bottomLeft.y + 0.25, k + 0.25));
            _bPosition.push_back(Vec3f(i + 0.25, bottomLeft.y + 0.25, k + 0.75));
            _bPosition.push_back(Vec3f(i + 0.75, bottomLeft.y + 0.25, k + 0.75));
            _bPosition.push_back(Vec3f(i + 0.25, bottomLeft.y + 0.75, k + 0.25));
            _bPosition.push_back(Vec3f(i + 0.75, bottomLeft.y + 0.75, k + 0.25));
            _bPosition.push_back(Vec3f(i + 0.25, bottomLeft.y + 0.75, k + 0.75));
            _bPosition.push_back(Vec3f(i + 0.75, bottomLeft.y + 0.75, k + 0.75));

            // top plane
            _bPosition.push_back(Vec3f(i + 0.25, topRight.y - 0.25, k + 0.25));
            _bPosition.push_back(Vec3f(i + 0.75, topRight.y - 0.25, k + 0.25));
            _bPosition.push_back(Vec3f(i + 0.25, topRight.y - 0.25, k + 0.75));
            _bPosition.push_back(Vec3f(i + 0.75, topRight.y - 0.25, k + 0.75));
            _bPosition.push_back(Vec3f(i + 0.25, topRight.y - 0.75, k + 0.25));
            _bPosition.push_back(Vec3f(i + 0.75, topRight.y - 0.75, k + 0.25));
            _bPosition.push_back(Vec3f(i + 0.25, topRight.y - 0.75, k + 0.75));
            _bPosition.push_back(Vec3f(i + 0.75, topRight.y - 0.75, k + 0.75));
        }

    for (int j = bottomLeft.y; j < topRight.y; j++)
        for (int k = bottomLeft.z; k < topRight.z; k++) {
            // left plane
            _bPosition.push_back(Vec3f(bottomLeft.x + 0.25, j + 0.25, k + 0.25));
            _bPosition.push_back(Vec3f(bottomLeft.x + 0.25, j + 0.75, k + 0.25));
            _bPosition.push_back(Vec3f(bottomLeft.x + 0.25, j + 0.25, k + 0.75));
            _bPosition.push_back(Vec3f(bottomLeft.x + 0.25, j + 0.75, k + 0.75));
            _bPosition.push_back(Vec3f(bottomLeft.x + 0.75, j + 0.25, k + 0.25));
            _bPosition.push_back(Vec3f(bottomLeft.x + 0.75, j + 0.75, k + 0.25));
            _bPosition.push_back(Vec3f(bottomLeft.x + 0.75, j + 0.25, k + 0.75));
            _bPosition.push_back(Vec3f(bottomLeft.x + 0.75, j + 0.75, k + 0.75));

            // right plane
            _bPosition.push_back(Vec3f(topRight.x - 0.25, j + 0.25, k + 0.25));
            _bPosition.push_back(Vec3f(topRight.x - 0.25, j + 0.75, k + 0.25));
            _bPosition.push_back(Vec3f(topRight.x - 0.25, j + 0.25, k + 0.75));
            _bPosition.push_back(Vec3f(topRight.x - 0.25, j + 0.75, k + 0.75));
            _bPosition.push_back(Vec3f(topRight.x - 0.75, j + 0.25, k + 0.25));
            _bPosition.push_back(Vec3f(topRight.x - 0.75, j + 0.75, k + 0.25));
            _bPosition.push_back(Vec3f(topRight.x - 0.75, j + 0.25, k + 0.75));
            _bPosition.push_back(Vec3f(topRight.x - 0.75, j + 0.75, k + 0.75));
        }

}

void IISPHsolver3D::update() {
    buildNeighborGrid();
    searchNeighbors();

    predictAdvection();
    pressureSolve();
    integration();

    visualizeFluidDensity();
}

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



/*-------------------------------------------Neighbor search------------------------------------------------*/

void IISPHsolver3D::fillFluidGrid(int i) {
    int id = _gridHelper.cellID(_fPosition[i]);

    if (_gridHelper.isInsideGrid(id))
        _fGrid[id].push_back(i);
}

void IISPHsolver3D::fillBoundaryGrid(int i) {
    int id = _gridHelper.cellID(_bPosition[i]);

    if (_gridHelper.isInsideGrid(id))
        _bGrid[id].push_back(i);
}

void  IISPHsolver3D::findFluidNeighbors(std::vector< Index >& neighbors, Vec3f position, const float radius) {
    std::vector<Index> neighborCells;
    Real  squaredRadius = square(radius);
    Real  distance = 0.0f;
    Index neighborID = 0;

    _gridHelper.getNeighborCells(neighborCells, position, radius);

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

    _gridHelper.getNeighborCells(neighborCells, position, radius);

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



/*------------------------------------------Fluid simulation-------------------------------------------------*/

void IISPHsolver3D::computePsi(int i) {
    _Psi[i] = 0.0f;
    Vec3f pos_ij;

    std::vector<Index> boundaryNeighbors;
    findBoundaryNeighbors(boundaryNeighbors, _bPosition[i], _h);

    for (Index& j : boundaryNeighbors) {
        pos_ij = _bPosition[i] - _bPosition[j];
        _Psi[i] += _kernel.W(pos_ij);
    }

    _Psi[i] = _rho0 / _Psi[i];
}

void IISPHsolver3D::computeDensity(int i) {
    _fDensity[i] = 0.0f;
    Vec3f pos_ij;

    std::vector<Index> fluidNeighbors = _fNeighbors[i];
    for (Index& j : fluidNeighbors) {
        pos_ij = _fPosition[i] - _fPosition[j];
        _fDensity[i] += _m0 * _kernel.W(pos_ij);
    }

    std::vector<Index> boundaryNeighbors = _bNeighbors[i];
    for (Index& j : boundaryNeighbors) {
        pos_ij = _fPosition[i] - _bPosition[j];
        _fDensity[i] += _Psi[j] * _kernel.W(pos_ij);
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

    std::vector<Index> fluidNeighbors = _fNeighbors[i];
    for (Index& j : fluidNeighbors)
        if (_fPosition[j] != _fPosition[i]) {
            pos_ij = _fPosition[i] - _fPosition[j];
            vel_ij = _fVelocity[i] - _fVelocity[j];
            _Fadv[i] += 2 * _nu * (square(_m0) / _fDensity[j]) * vel_ij.dotProduct(pos_ij) * _kernel.gradW(pos_ij) / (pos_ij.lengthSquare() + 0.01 * _h);
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

    std::vector<Index> fluidNeighbors = _fNeighbors[i];
    for (Index& j : fluidNeighbors)
        if (_fPosition[j] != _fPosition[i]) {
            pos_ij = _fPosition[i] - _fPosition[j];
            _Dii[i] += (-_m0 / square(_fDensity[i])) * _kernel.gradW(pos_ij);
        }

    std::vector<Index> boundaryNeighbors = _bNeighbors[i];
    for (Index& j : boundaryNeighbors)
        if (_bPosition[j] != _fPosition[i]) {
            pos_ij = _fPosition[i] - _bPosition[j];
            _Dii[i] += (-_Psi[j] / square(_fDensity[i])) * _kernel.gradW(pos_ij);
        }

    _Dii[i] *= square(_dt);
}

void IISPHsolver3D::predictDensity(int i) {
    _Dadv[i] = 0.0f;
    Vec3f pos_ij;
    Vec3f vel_adv_ij;

    std::vector<Index> fluidNeighbors = _fNeighbors[i];
    for (Index& j : fluidNeighbors)
        if (_fPosition[j] != _fPosition[i]) {
            pos_ij = _fPosition[i] - _fPosition[j];
            vel_adv_ij = _Vadv[i] - _Vadv[j];
            _Dadv[i] += _m0 * vel_adv_ij.dotProduct(_kernel.gradW(pos_ij));
        }

    std::vector<Index> boundaryNeighbors = _bNeighbors[i];
    for (Index& j : boundaryNeighbors)
        if (_bPosition[j] != _fPosition[i]) {
            pos_ij = _fPosition[i] - _bPosition[j];
            vel_adv_ij = _Vadv[i];
            _Dadv[i] += _Psi[j] * vel_adv_ij.dotProduct(_kernel.gradW(pos_ij));
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

    std::vector<Index> fluidNeighbors = _fNeighbors[i];
    for (Index& j : fluidNeighbors)
        if (_fPosition[j] != _fPosition[i]) {
            pos_ij = _fPosition[i] - _fPosition[j];
            d_ji = -(square(_dt) * _m0 / square(_fDensity[i])) * (-_kernel.gradW(pos_ij));
            _Aii[i] += _m0 * (_Dii[i] - d_ji).dotProduct(_kernel.gradW(pos_ij));
        }

    std::vector<Index> boundaryNeighbors = _bNeighbors[i];
    for (Index& j : boundaryNeighbors)
        if (_bPosition[j] != _fPosition[i]) {
            pos_ij = _fPosition[i] - _bPosition[j];
            _Aii[i] += _Psi[j] * _Dii[i].dotProduct(_kernel.gradW(pos_ij));
        }
}

void IISPHsolver3D::storeSumDijPj(int i) {
    _sumDijPj[i].x = 0.0f;
    _sumDijPj[i].y = 0.0f;
    _sumDijPj[i].z = 0.0f;
    Vec3f pos_ij;

    std::vector<Index> fluidNeighbors = _fNeighbors[i];
    for (Index& j : fluidNeighbors)
        if (_fPosition[j] != _fPosition[i]) {
            pos_ij = _fPosition[i] - _fPosition[j];
            _sumDijPj[i] += -(_m0 * _fPressure[j] / square(_fDensity[j])) * _kernel.gradW(pos_ij);
        }

    _sumDijPj[i] *= square(_dt);
}

void IISPHsolver3D::computePressure(int i) {
    _Dcorr[i] = 0.0f;
    Vec3f pos_ij;
    Vec3f d_ji;
    Vec3f aux;

    std::vector<Index> fluidNeighbors = _fNeighbors[i];
    for (Index& j : fluidNeighbors)
        if (_fPosition[j] != _fPosition[i]) {
            pos_ij = _fPosition[i] - _fPosition[j];
            d_ji = -(square(_dt) * _m0 / square(_fDensity[i])) * (-_kernel.gradW(pos_ij));
            aux = _sumDijPj[i] - _Dii[j] * _Pl[j] - (_sumDijPj[j] - d_ji * _Pl[i]);
            _Dcorr[i] += _m0 * aux.dotProduct(_kernel.gradW(pos_ij));
        }

    std::vector<Index> boundaryNeighbors = _bNeighbors[i];
    for (Index& j : boundaryNeighbors)
        if (_bPosition[j] != _fPosition[i]) {
            pos_ij = _fPosition[i] - _bPosition[j];
            _Dcorr[i] += _Psi[j] * _sumDijPj[i].dotProduct(_kernel.gradW(pos_ij));
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

    std::vector<Index> fluidNeighbors = _fNeighbors[i];
    for (Index& j : fluidNeighbors)
        if (_fPosition[j] != _fPosition[i]) {
            pos_ij = _fPosition[i] - _fPosition[j];
            _Fp[i] += -square(_m0) * (_fPressure[i] / square(_fDensity[i]) + _fPressure[j] / square(_fDensity[j])) * _kernel.gradW(pos_ij);
        }

    std::vector<Index> boundaryNeighbors = _bNeighbors[i];
    for (Index& j : boundaryNeighbors)
        if (_bPosition[j] != _fPosition[i]) {
            pos_ij = _fPosition[i] - _bPosition[j];
            _Fp[i] += -_m0 * _Psi[j] * (_fPressure[i] / square(_fDensity[i])) * _kernel.gradW(pos_ij);
        }
}

void IISPHsolver3D::updateVelocity(int i) {
    _fVelocity[i] = _Vadv[i] + _dt * _Fp[i] / _m0;
}

void IISPHsolver3D::updatePosition(int i) {

    if (_gridHelper.isInsideGrid(_fPosition[i] + _dt * _fVelocity[i]))
        _fPosition[i] += _dt * _fVelocity[i];
    else
        debugCrash(i);
}



/*----------------------------------------Debug / visualization-----------------------------------------------*/

void IISPHsolver3D::visualizeFluidDensity() {
    for (Index i = 0; i < _fluidCount; i++) {
        _fColor[i].x = _lightColor.x + (_fDensity[i] / _rho0) * (_denseColor.x - _lightColor.x);
        _fColor[i].y = _lightColor.y + (_fDensity[i] / _rho0) * (_denseColor.y - _lightColor.y);
        _fColor[i].z = _lightColor.z + (_fDensity[i] / _rho0) * (_denseColor.z - _lightColor.z);
    }
    for (Index i = 0; i < _boundaryCount; i++)
        _bColor[i] = _wallColor;
}

void IISPHsolver3D::visualizeBoundaryDensity() {
    for (Index i = 0; i < _boundaryCount; i++) {
        _bColor[i].x = _lightColor.x + (_Psi[i] / _rho0) * (_wallColor.x - _lightColor.x);
        _bColor[i].y = _lightColor.y + (_Psi[i] / _rho0) * (_wallColor.y - _lightColor.y);
        _bColor[i].z = _lightColor.z + (_Psi[i] / _rho0) * (_wallColor.z - _lightColor.z);
    }
}

void IISPHsolver3D::visualizeFluidNeighbors(int i) {

    for (int j = 0; j < _fNeighbors[i].size(); j++)
        _fColor[_fNeighbors[i][j]] = _greenColor;

    for (int j = 0; j < _bNeighbors[i].size(); j++)
        _bColor[_bNeighbors[i][j]] = _pinkColor;

    _fColor[i] = _redColor;
}

void IISPHsolver3D::debugCrash(int i) {
    std::cout
        << "position     : " << _fPosition[i] << "\n"
        << "velocity     : " << _fVelocity[i] << "\n"
        << "pressure     : " << _fPressure[i] << "\n"
        << "density      : " << _fDensity[i] << "\n"
        << "F_p          : " << _Fp[i] << "\n"
        << "rho_corr     : " << _Dcorr[i] << "\n"
        << "sum d_ij p_j : " << _sumDijPj[i] << "\n"
        << "a_ii         : " << _Aii[i] << "\n"
        << "rho_adv      : " << _Dadv[i] << "\n"
        << "d_ii         : " << _Dii[i] << "\n"
        << "v_adv        : " << _Vadv[i] << "\n"
        << "F_adv        : " << _Fadv[i] << "\n"
        << std::endl;

    std::cout << "neighbors : \n";

    std::vector<Index> neighbors = _fNeighbors[i];
    for (Index j : neighbors) {
        if (_fPosition[j] != _fPosition[i]) {
            Vec3f pos_ij = _fPosition[i] - _fPosition[j];

            std::cout
                << "    position     : " << _fPosition[j] << "\n"
                << "    velocity     : " << _fVelocity[j] << "\n"
                << "    pressure     : " << _fPressure[j] << "\n"
                << "    density      : " << _fDensity[j] << "\n"
                << "    F_p          : " << _Fp[j] << "\n"
                << "    rho_corr     : " << _Dcorr[i] << "\n"
                << "    sum d_ij p_j : " << _sumDijPj[j] << "\n"
                << "    a_ii         : " << _Aii[j] << "\n"
                << "    rho_adv      : " << _Dadv[j] << "\n"
                << "    d_ii         : " << _Dii[j] << "\n"
                << "    v_adv        : " << _Vadv[j] << "\n"
                << "    F_adv        : " << _Fadv[j] << "\n"
                << "    gradient     : " << _kernel.gradW(pos_ij) << "\n"
                << std::endl;
        }
    }
    std::cout << "---------------------------------------------\n\n" << std::endl;
}

