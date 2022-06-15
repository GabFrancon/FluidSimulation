#pragma once

#include "sph_kernel.h"
#include "sph_grid.h"

#include "../Surface/IsoSurface.h"

#include <numeric>
#include <algorithm>
#include <vector>
#include <omp.h>


class IISPHsolver3D
{
public:
    explicit IISPHsolver3D(
        const Real h    = 0.5f,    // particle spacing
        const Real rho0 = 1e3f,    // rest density
        const Real nu   = 0.08f,   // kinematic viscosity
        const Real eta  = 0.01f)   // compressibility
    {
        // fluid properties
        _h    = h;
        _rho0 = rho0;
        _nu   = nu;
        _eta  = eta;

        // fixed constants
        _dt = 0.00835f;
        _g  = Vec3f(0.0f, -9.81f, 0.0f);
        _omega = 0.5f;

        // derived properties
        _m0 = _rho0 * cube(_h);
    }

    void prepare();
    void sampleFluidCube(Vec3f bottomLeft, Vec3f topRight);
    void sampleFluidBall(Vec3f center, Real radius, Real precision = 0.5f);
    void sampleBoundaryBox(Vec3f bottomLeft, Vec3f topRight, int thickness = 1);
    void sampleDistanceField(Vec3f bottomLeft, Vec3f topRight);
    void sampleMesh(std::vector<Vec3f> vertices, std::vector<Index> indices);
    void solveSimulation();
    void reconstructSurface();

    inline void setParticleHelper(Real cellSize, Vec3f gridSize) { _pGridHelper = GridHelper(cellSize, gridSize); }
    inline void setSurfaceHelper (Real cellSize, Vec3f gridSize) { _sGridHelper = GridHelper(cellSize, gridSize); }

    const inline Index  fluidCount() const { return _fluidCount; }
    const inline Vec3f& fluidPosition(const Index i) const { return _fPosition[i]; }
    const inline Vec3f& fluidColor(const Index i) const { return _fColor[i]; }

    const inline Index  boundaryCount() const { return _boundaryCount; }
    const inline Vec3f& boundaryPosition(const Index i) const { return _bPosition[i]; }
    const inline Vec3f& boundaryColor(const Index i) const { return _bColor[i]; }

    const inline Real sizeX()    const { return _pGridHelper.sizeX(); }
    const inline Real sizeY()    const { return _pGridHelper.sizeY(); }
    const inline Real sizeZ()    const { return _pGridHelper.sizeZ(); }
    const inline Real cellSize() const { return _pGridHelper.cellSize(); }
    const inline Real particleSpacing() const { return _h; };

    const inline Index verticesCount() const { return _isoSurface.m_nVertices; }
    const inline Index indicesCount()  const { return _isoSurface.m_nTriangles * 3; }

    const inline POINT3D*      vertices() const { return _isoSurface.m_ppt3dVertices; }
    const inline unsigned int* indices()  const { return _isoSurface.m_piTriangleIndices; }
    

private:
    /*--------------------------------------------Main functions--------------------------------------------------*/

    void buildNeighborGrid();
    void searchNeighbors();
    void predictAdvection();
    void pressureSolve();
    void integration();


    /*-------------------------------------------Neighbor search------------------------------------------------*/

    void  fillFluidGrid(int i);
    void  fillBoundaryGrid(int i);
    void  findFluidNeighbors(std::vector< Index >& neighbors, Vec3f position, const float radius);
    void  findBoundaryNeighbors(std::vector< Index >& neighbors, Vec3f position, const float radius);


    /*-----------------------------------------Particle simulation------------------------------------------------*/

    void computePsi(int i);
    void computeDensity(int i);
    void computeAdvectionForces(int i);
    void addBodyForce(int i);
    void addViscousForce(int i);
    void predictVelocity(int i);
    void storeDii(int i);

    void predictDensity(int i);
    void initPressure(int i);
    void storeAii(int i);

    void storeSumDijPj(int i);
    void computePressure(int i);
    void computeError();

    void computePressureForces(int i);
    void updateVelocity(int i);
    void updatePosition(int i);


    /*---------------------------------------Surface reconstruction----------------------------------------------*/

    void computeDistanceField(int i, const float radius);
    void generateIsoSurface();


    /*----------------------------------------Debug / visualization-----------------------------------------------*/

    void visualizeFluidDensity();
    void visualizeBoundaryDensity();
    void visualizeFluidNeighbors(int i);
    void debugCrash(int i);


    /*-------------------------------------------Class members---------------------------------------------------*/

    // smooth kernels
    CubicSpline  _pKernel;
    SimpleKernel _sKernel;

    // fluid particles data
    std::vector<Vec3f> _fPosition;
    std::vector<Vec3f> _fVelocity;
    std::vector<Real>  _fPressure;
    std::vector<Real>  _fDensity;
    std::vector<Vec3f> _fColor;

    // boundary particles data
    std::vector<Vec3f> _bPosition;
    std::vector<Vec3f> _bColor;

    // surface data
    std::vector<Vec3f> _sPosition;
    std::vector<Real>  _distanceField;
    IsoSurface<Real>   _isoSurface;

    // temporary data
    std::vector<Real>  _Psi;
    std::vector<Vec3f> _Dii;
    std::vector<Real>  _Aii;
    std::vector<Vec3f> _sumDijPj;
    std::vector<Vec3f> _Vadv;
    std::vector<Real>  _Dadv;
    std::vector<Real>  _Pl;
    std::vector<Real>  _Dcorr;
    std::vector<Vec3f> _Fadv;
    std::vector<Vec3f> _Fp;

    // neigboring structures
    GridHelper _pGridHelper;
    GridHelper _sGridHelper;
    std::vector< std::vector<Index> > _fGrid;
    std::vector< std::vector<Index> > _fNeighbors;
    std::vector< std::vector<Index> > _bGrid;
    std::vector< std::vector<Index> > _bNeighbors;

    // visualization
    Vec3f _wallColor  = { 195 / 255.0f,  50 / 255.0f,  30 / 255.0f };
    Vec3f _lightColor = {  79 / 255.0f, 132 / 255.0f, 237 / 255.0f };
    Vec3f _denseColor = {  10 / 255.0f,  47 / 255.0f, 119 / 255.0f };
    Vec3f _redColor   = { 255 / 255.0f,   0 / 255.0f,   0 / 255.0f };
    Vec3f _greenColor = {   0 / 255.0f, 255 / 255.0f,   0 / 255.0f };
    Vec3f _pinkColor  = { 255 / 255.0f,   0 / 255.0f, 255 / 255.0f };


    // simulation
    int  _fluidCount    = 0;      // number of fluid particles
    int  _boundaryCount = 0;      // number of boundary particles
    int  _surfaceCount  = 0;      // number of surface nodes
    Real _avgDensity    = 0.0f;   // average density of fluid

    // SPH coefficients
    Real  _dt;                    // time step
    Real  _nu;                    // kinematic viscosity
    Real  _eta;                   // compressibility
    Real  _rho0;                  // rest density
    Real  _h;                     // particle spacing
    Vec3f _g;                     // gravity
    Real  _m0;                    // rest mass
    Real  _omega;                 // Jacobi's relaxed coeff
};

