#pragma once

#include "sph_kernel.h"

#include <numeric>
#include <algorithm>
#include <vector>


class IISPHsolver2D
{
public:
    explicit IISPHsolver2D(
        const Real h    = 0.5f,     // particle spacing
        const Real rho0 = 1e3f,     // rest density
        const Real nu   = 0.08f,    // kinematic viscosity
        const Real eta  = 0.01f)    // compressibility
    {
        // fluid properties
        _h    = h;
        _rho0 = rho0;
        _nu   = nu;
        _eta  = eta;

        // fixed constants
        _dt = 0.01f;
        _g  = Vec2f(0.0f, -9.8f);
        _omega = 0.5f;

        // derived properties
        _m0 = _rho0 * square(_h);
        _kernel = CubicSpline(_h, 2);
    }

    void init(const int gridX, const int gridY, const int fluidWidth, const int fluidHeight);
    void sampleFluidCube(int bottomX, int bottomY, int topX, int topY);
    void sampleBoundaryCube(int bottomX, int bottomY, int topX, int topY);
    void update();

    const inline Index  fluidCount() const { return _fluidCount; }
    const inline Vec2f& fluidPosition(const Index i) const { return _fPosition[i]; }
    const inline Vec3f& fluidColor(const Index i) const { return _fColor[i]; }

    const inline Index  boundaryCount() const { return _boundaryCount; }
    const inline Vec2f& boundaryPosition(const Index i) const { return _bPosition[i]; }
    const inline Vec3f& boundaryColor(const Index i) const { return _bColor[i]; }

    const inline int resX() const { return _resX; }
    const inline int resY() const { return _resY; }


private:
    /*--------------------------------------------Main functions--------------------------------------------------*/

    void initNeighbors();
    void updateNeighbors();
    void predictAdvection();
    void pressureSolve();
    void integration();


    /*-------------------------------------------Neighbor search------------------------------------------------*/

    void  buildNeighborGrid();
    void  getNeighborCells(std::vector<Index>& neighbors, Vec2f particle, const int radius);
    Index cellID(Vec2f particle);
    Index cellID(int i, int j);
    bool  isInsideGrid(Vec2f particle);
    bool  isInsideGrid(int id);
    Vec2i cellPos(Vec2f particle);
    void  fillFluidGrid(int i);
    void  fillBoundaryGrid(int i);
    void  findFluidNeighbors(int i, const int radius);
    void  findBoundaryNeighbors(std::vector< Index >& neighbors, int i, const int radius);


    /*------------------------------------------Fluid simulation-------------------------------------------------*/

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
   

    /*----------------------------------------Debug / visualization-----------------------------------------------*/

    void visualizeFluidDensity();
    void visualizeBoundaryDensity();
    void viualizeFluidNeighbors(int i);
    void debugCrash(int i);


    /*-------------------------------------------Class members---------------------------------------------------*/

    // smooth kernel
    CubicSpline _kernel;

    // fluid particles data
    std::vector<Vec2f> _fPosition;
    std::vector<Vec2f> _fVelocity;
    std::vector<Real>  _fPressure;
    std::vector<Real>  _fDensity;
    std::vector<Vec3f> _fColor;

    // boundary particles data
    std::vector<Vec2f> _bPosition;
    std::vector<Vec3f> _bColor;

    // temporary data
    std::vector<Real>  _Psi;
    std::vector<Vec2f> _Dii;
    std::vector<Real>  _Aii;
    std::vector<Vec2f> _sumDijPj;
    std::vector<Vec2f> _Vadv;
    std::vector<Real>  _Dadv;
    std::vector<Real>  _Pl;
    std::vector<Real>  _Dcorr;
    std::vector<Vec2f> _Fadv;
    std::vector<Vec2f> _Fp;

    // neigboring structures
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
    int  _resX = 0;               // grid resolution on x-axis
    int  _resY = 0;               // grid resolution on y-axis
    int  _fluidCount    = 0;      // number of fluid particles
    int  _boundaryCount = 0;      // number of boundary particles
    Real _avgDensity    = 0.0f;   // average density of fluid

    // SPH coefficients
    Real _dt;                     // time step
    Real _nu;                     // kinematic viscosity
    Real _eta;                    // compressibility
    Real _rho0;                   // rest density
    Real _h;                      // particle spacing
    Vec2f _g;                     // gravity
    Real _m0;                     // rest mass
    Real _omega;                  // Jacobi's relaxed coeff
};

