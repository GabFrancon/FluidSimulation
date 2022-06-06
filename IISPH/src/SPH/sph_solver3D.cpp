#include "sph_solver3D.h"

/*--------------------------------------------Main functions--------------------------------------------------*/

void IISPHsolver3D::init(Vec3i gridSize, Vec3i fluidSize) {
    // - create a grid of size gridSize
    // - init a fluid mass of size fluidSize
    // - each cell containing the fluid is sampled with 2x2 particles
    _pGridHelper = GridHelper(2 * _h, gridSize);
    _pKernel = CubicSpline(_h, 3);

    _sGridHelper = GridHelper(0.5f * _h, gridSize - 2);
    _sKernel = SimpleKernel(_h);

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

    // sample nodes surface
    _sNodes.clear();
    sampleSurfaceNodes(Vec3i(1), gridSize);
    _surfaceCount = _sNodes.size();

    // init other quantities
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
    _Phi      = std::vector<Real>(_surfaceCount, 0.0f);

    // init neighboring system
    _fGrid = std::vector<std::vector<Index>>((size_t)_pGridHelper.cellCount(), std::vector<Index>());
    _bGrid = std::vector<std::vector<Index>>((size_t)_pGridHelper.cellCount(), std::vector<Index>());
    buildNeighborGrid();

    _fNeighbors = std::vector<std::vector<Index>>(_fluidCount, std::vector<Index>());
    _sNeighbors = std::vector<std::vector<Index>>(_surfaceCount, std::vector<Index>());
    _bNeighbors = std::vector<std::vector<Index>>(_fluidCount, std::vector<Index>());
    searchNeighbors();

    // init surface reconstruction (for vizualisation)
    surfaceReconstruction();

    // compute fluid and boundary density (for vizualisation)
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

void IISPHsolver3D::sampleSurfaceNodes(Vec3i bottomLeft, Vec3i topRight) {
    for (float k = bottomLeft.z; k <= topRight.z - 1; k += _sGridHelper.cellSize())
        for (float j = bottomLeft.y; j <= topRight.y - 1; j += _sGridHelper.cellSize())
            for (float i = bottomLeft.x; i <= topRight.x - 1; i += _sGridHelper.cellSize()) {
                _sNodes.push_back(Vec3f(i, j, k));
            }
}

void IISPHsolver3D::update() {
    buildNeighborGrid();
    searchNeighbors();

    predictAdvection();
    pressureSolve();
    integration();

    visualizeFluidDensity();
    surfaceReconstruction();
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
        findFluidNeighbors(_fNeighbors[i], _fPosition[i], 2.0f * _h);

        // search for boundary neighbor particles
        size_t lastBoundarySize = _bNeighbors[i].size();
        _bNeighbors[i].clear();
        _bNeighbors[i].reserve(lastBoundarySize);
        findBoundaryNeighbors(_bNeighbors[i], _fPosition[i], 2.0f * _h);
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

void IISPHsolver3D::surfaceReconstruction() {
#pragma omp parallel for
    for (int i = 0; i < _surfaceCount; i++)
        computeSignedDistance(i, 2.0f * _h);

    generateSurface();
}



/*-------------------------------------------Neighbor search------------------------------------------------*/

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

void IISPHsolver3D::computePsi(int i) {
    _Psi[i] = 0.0f;
    Vec3f pos_ij;

    std::vector<Index> boundaryNeighbors;
    findBoundaryNeighbors(boundaryNeighbors, _bPosition[i], _h);

    for (Index& j : boundaryNeighbors) {
        pos_ij = _bPosition[i] - _bPosition[j];
        _Psi[i] += _pKernel.W(pos_ij);
    }

    _Psi[i] = _rho0 / _Psi[i];
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
            _Fadv[i] += 2 * _nu * (square(_m0) / _fDensity[j]) * vel_ij.dotProduct(pos_ij) * _pKernel.gradW(pos_ij) / (pos_ij.lengthSquare() + 0.01 * _h);
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
    else
        debugCrash(i);
}



/*---------------------------------------Surface reconstruction----------------------------------------------*/

void IISPHsolver3D::computeSignedDistance(int i, const float radius) {
    Vec3f sumX = Vec3f(0.0f);
    Real  sumK = 0.0f;
    Real  temp = 0.0f;
    Vec3f pos_ij;

    size_t lastSize = _sNeighbors[i].size();
    _sNeighbors[i].clear();
    _sNeighbors[i].reserve(lastSize);
    findFluidNeighbors(_sNeighbors[i], _sNodes[i], radius);

    for (Index& j : _sNeighbors[i]) {
        pos_ij = _sNodes[i] - _fPosition[j];
        temp = _sKernel.W(pos_ij);
        sumX += _fPosition[j] * temp;
        sumK += temp;
    }

    _Phi[i] = (_sNodes[i] - sumX / sumK).length() - _h / 2;
}

void IISPHsolver3D::generateSurface() {
    _isoSurface.GenerateSurface(
        _Phi.data(), 0.0f,
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
        _fColor[_fNeighbors[i][j]] = _greenColor;

    for (Index& j : _bNeighbors[i])
        _bColor[_bNeighbors[i][j]] = _pinkColor;

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

