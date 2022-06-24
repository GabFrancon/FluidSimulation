#pragma once

#include "sph_types.h"
#include "sph_grid.h"
#include <vector>
#include <unordered_map>

class Sampler {
public:

	static void cubeSurface(std::vector<Vec3f>& positions, Real cellSize, Vec3f bottomLeft, Vec3f topRight, int thickness = 1) {
        Real offset25  = 0.25f * cellSize;
        Real offset50  = 0.50f * cellSize;
        Real offset75  = 0.75f * cellSize;
        Real offset100 = 1.00f * cellSize;

        switch (thickness) {
        case 1:
            for (float i = bottomLeft.x + offset50; i < topRight.x; i += offset50)
                for (float k = bottomLeft.z + offset50; k < topRight.z; k += offset50) {
                    positions.push_back(Vec3f(i, bottomLeft.y + offset50, k)); // bottom
                    positions.push_back(Vec3f(i, topRight.y - offset50, k));   // top
                }

            for (float i = bottomLeft.x + offset50; i < topRight.x; i += offset50)
                for (float j = bottomLeft.y + offset100; j < topRight.y - offset50; j += offset50) {
                    positions.push_back(Vec3f(i, j, bottomLeft.z + offset50)); // back
                    positions.push_back(Vec3f(i, j, topRight.z - offset50));   // front
                }

            for (float j = bottomLeft.y + offset100; j < topRight.y - offset50; j += offset50)
                for (float k = bottomLeft.z + offset100; k < topRight.z - offset50; k += offset50) {
                    positions.push_back(Vec3f(bottomLeft.x + offset50, j, k)); // left
                    positions.push_back(Vec3f(topRight.x - offset50, j, k));   // right
                }
            break;

        case 2:
            for (float i = bottomLeft.x + offset25; i < topRight.x; i += offset50)
                for (float k = bottomLeft.z + offset25; k < topRight.z; k += offset50) {
                    positions.push_back(Vec3f(i, bottomLeft.y + offset25, k)); // bottom
                    positions.push_back(Vec3f(i, bottomLeft.y + offset75, k)); // bottom
                    positions.push_back(Vec3f(i, topRight.y - offset25, k));   // top
                    positions.push_back(Vec3f(i, topRight.y - offset75, k));   // top

                }

            for (float i = bottomLeft.x + offset25; i < topRight.x; i += offset50)
                for (float j = bottomLeft.y + offset25 + offset100; j < topRight.y - offset100; j += offset50) {
                    positions.push_back(Vec3f(i, j, bottomLeft.z + offset25)); // back
                    positions.push_back(Vec3f(i, j, bottomLeft.z + offset75)); // back
                    positions.push_back(Vec3f(i, j, topRight.z - offset25));   // front
                    positions.push_back(Vec3f(i, j, topRight.z - offset75));   // front
                }

            for (float j = bottomLeft.y + offset25 + offset100; j < topRight.y - offset100; j += offset50)
                for (float k = bottomLeft.z + offset25 + offset100; k < topRight.z - offset100; k += offset50) {
                    positions.push_back(Vec3f(bottomLeft.x + offset25, j, k)); // left
                    positions.push_back(Vec3f(bottomLeft.x + offset75, j, k)); // left
                    positions.push_back(Vec3f(topRight.x - offset25, j, k));   // right
                    positions.push_back(Vec3f(topRight.x - offset75, j, k));   // right
                }
            break;
        }
	}

    static void cubeVolume(std::vector<Vec3f>& positions, Real cellSize, Vec3f bottomLeft, Vec3f topRight) {
        Real offset25 = 0.25f * cellSize;
        Real offset50 = 0.50f * cellSize;

        for (float k = bottomLeft.z + offset25; k < topRight.z; k += offset50)
            for (float j = bottomLeft.y + offset25; j < topRight.y; j += offset50)
                for (float i = bottomLeft.x + offset25; i < topRight.x; i += offset50) {
                    positions.push_back(Vec3f(i, j, k));
                }
    }

    static void gridNodes(std::vector<Vec3f>& positions, Real cellSize, Vec3f bottomLeft, Vec3f topRight) {
        Real offset100 = cellSize;

        for (float k = bottomLeft.z; k <= topRight.z; k += offset100)
            for (float j = bottomLeft.y; j <= topRight.y; j += offset100)
                for (float i = bottomLeft.x; i <= topRight.x; i += offset100) {
                    positions.push_back(Vec3f(i, j, k));
                }
    }

    static void cylinderSurface(std::vector<Vec3f>& positions, Real spacing, Vec3f bottomCenter, Real radius, Real height, bool vertical = true) {
        if (radius < spacing)
            return;

        Vec3f newPoint{};

        Real r     = radius;
        Real h     = spacing / 2;
        Real x     = 3 * square(h) * (1 - square(h) / (3 * r)) / (square(r) - square(h) / 4);
        Real alpha = acos(1 - x);
        Index n    = (Index)(2 * M_PI / alpha) - 1;

        alpha = 2 * M_PI / (n + 1);

        for (Real offset = 0; offset < height; offset += spacing) {

            for (Index i = 0; i <= n; i++) {
                if (vertical) {
                    newPoint.x = r * cos(alpha * i) + bottomCenter.x;
                    newPoint.y = offset             + bottomCenter.y;
                    newPoint.z = r * sin(alpha * i) + bottomCenter.z;
                } 
                else {
                    newPoint.x = offset             + bottomCenter.x;
                    newPoint.y = r * sin(alpha * i) + bottomCenter.y;
                    newPoint.z = r * cos(alpha * i) + bottomCenter.z;
                }
                positions.push_back(newPoint);
            }
        }
    }

    static void glassSurface(std::vector<Vec3f>& positions, Real spacing, Vec3f bottomCenter, Real minRadius, Real maxRadius, Real height) {
        Vec3f newPoint{bottomCenter};
        Real r{minRadius}, h{ spacing / 2 }, x{}, alpha{};
        Index n{};
        Real bend = 0.4f * height;
        positions.push_back(newPoint);

        // bottom
        while (r > spacing + h) {
            r -= spacing;

            x = 3 * square(h) * (1 - square(h) / (3 * r)) / (square(r) - square(h) / 4);
            if (x <= 0 || x >= 2)
                break;

            alpha = acos(1 - x);
            n = (Index)(2 * M_PI / alpha) - 1;
            alpha = 2 * M_PI / (n + 1);

            for (Index i = 0; i <= n; i++) {
                newPoint.x = r * cos(alpha * i) + bottomCenter.x;
                newPoint.y = bottomCenter.y;
                newPoint.z = r * sin(alpha * i) + bottomCenter.z;

                positions.push_back(newPoint);
            }
        }

        // side
        for (Real offset = 0; offset < height; offset += 0.7f * spacing * r / maxRadius) {

            if (offset < bend)
                r = minRadius + (maxRadius - minRadius) * (1 - exp(-5 * offset / bend));
            else
                r = maxRadius - 2.0f * maxRadius * (1 - exp(-0.2f * (offset - bend) / (height - bend)));

            x  = 3 * square(h) * (1 - square(h) / (3 * r)) / (square(r) - square(h) / 4);
            if (x <= 0 || x >= 2)
                break;

            alpha = acos(1 - x);
            n     = (Index)(2 * M_PI / alpha) - 1;
            alpha = 2 * M_PI / (n + 1);

            for (Index i = 0; i <= n; i++) {
                newPoint.x = r * cos(alpha * i) + bottomCenter.x;
                newPoint.y = offset + bottomCenter.y;
                newPoint.z = r * sin(alpha * i) + bottomCenter.z;
                
                positions.push_back(newPoint);
            }
        }
    }

    static void meshSurface(std::vector<Vec3f>& positions, std::vector<Vec3f> vertices, std::vector<uint32_t> indices, GridHelper grid) {
        /*std::unordered_map<int, uint32_t> uniqueCells{};
        Vec3f size = Vec3f(grid.cellSize());
        Vec3f offset{};

        for (Vec3f& p : vertices) {
            int id = grid.cellID(p);

            if (uniqueCells.count(id) == 0) {
                uniqueCells[id] = static_cast<uint32_t>(positions.size());
                offset = grid.cellSize() * (Vec3f) grid.cellPos(p);
                cubeVolume(positions, grid.cellSize(), offset, offset + size);
            }
        }*/

        for (int i = 0; i < indices.size(); i += 3) {
            Vec3f p1 = positions[i];
            Vec3f p2 = positions[i + 1];
            Vec3f p3 = positions[i + 2];


        }


    }
};

