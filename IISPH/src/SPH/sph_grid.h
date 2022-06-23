#pragma once

#include "sph_types.h"

#include <vector>

class GridHelper {
public:

    GridHelper() {}

    GridHelper(float cellSize, Vec3f dimensions) {
        _cellSize = cellSize;
        _gridSize = dimensions;
        _gridRes = cellPos(dimensions);
    }

    const inline int   cellCount() const { return _gridRes.x * _gridRes.y * _gridRes.z; }
    const inline float cellSize() const { return _cellSize; }

    const inline int  resX() const { return _gridRes.x; }
    const inline int  resY() const { return _gridRes.y; }
    const inline int  resZ() const { return _gridRes.z; }
    const inline Vec3i res() const { return _gridRes; }

    const inline Real sizeX() const { return _gridSize.x; }
    const inline Real sizeY() const { return _gridSize.y; }
    const inline Real sizeZ() const { return _gridSize.z; }
    const inline Vec3f size() const { return _gridSize; }

    void getNeighborCells(std::vector<Index>& neighbors, Vec3f particle, const float radius) {
        if (!isInsideGrid(particle)) {
            neighbors.clear();
            return;
        }

        Vec3i minCell = cellPos(particle - radius);
        Vec3i maxCell = cellPos(particle + radius);

        int imin = std::max(minCell.x, 0);
        int imax = std::min(maxCell.x, _gridRes.x - 1);
        int jmin = std::max(minCell.y, 0);
        int jmax = std::min(maxCell.y, _gridRes.y - 1);
        int kmin = std::max(minCell.z, 0);
        int kmax = std::min(maxCell.z, _gridRes.z - 1);

        int count = 0;
        int size = (kmax - kmin + 1) * (jmax - jmin + 1) * (imax - imin + 1);
        neighbors.resize(size);

        for (int k = kmin; k <= kmax; ++k)
            for (int j = jmin; j <= jmax; ++j)
                for (int i = imin; i <= imax; ++i) {
                        neighbors[count] = cellID(i, j, k);
                        count++;
                }
    }

    Index cellID(Vec3f particle) {
        Vec3i cell = cellPos(particle);
        return cellID(cell.x, cell.y, cell.z);
    }

    Index cellID(int i, int j, int k) {
        return i + j * _gridRes.x + k * _gridRes.x * _gridRes.y;
    }

    bool isInsideGrid(Vec3f particle) {
        Index id = cellID(particle);
        return isInsideGrid(id);
    }

    bool isInsideGrid(int id) {
        return id >= 0 && id < _gridRes.x * _gridRes.y * _gridRes.z;
    }

    Vec3i cellPos(Vec3f particle) {
        Vec3i cell;
        cell.x = std::floor(particle.x / _cellSize);
        cell.y = std::floor(particle.y / _cellSize);
        cell.z = std::floor(particle.z / _cellSize);
        return cell;
    }

private:
    Vec3i _gridRes;
    Vec3f _gridSize;
    Real  _cellSize = 1.0f;
};
