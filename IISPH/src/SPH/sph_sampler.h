#pragma once

#include "sph_types.h"
#include "sph_grid.h"
#include <vector>	


class Sampler {
public:

	static void cubeSurface(std::vector<Vec3f>& positions, Real cellSize, Vec3f bottomLeft, Vec3f topRight) {
        Real offset25  = 0.25f * cellSize;
        Real offset50  = 0.50f * cellSize;
        Real offset75  = 0.75f * cellSize;
        Real offset100 = 1.00f * cellSize;
        int thickness  = 1;


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

    static void cylinderSurface(std::vector<Vec3f>& positions, Real spacing, Vec3f bottomBase, Real radius, Real height) {
        if (radius < spacing)
            return;

        Real r = radius;
        Real h = spacing;
        Vec3f center = bottomBase;

        Real x = 3 * square(h) * (1 - square(h) / (3 * r)) / (square(r) - square(h) / 4);
        Real alpha = acos(1 - x);
        Index n = (Index)(2 * M_PI / alpha) - 1;
        alpha = 2 * M_PI / (n + 1);

        std::cout << "n : " << n << "\n" << " alpha : " << alpha << std::endl;


        for (Real y = 0; y < height; y += h) {
            center.y += y;

            for (Index i = 0; i < n; i++)
                positions.push_back(Vec3f(r * cos(alpha), 0.0f, r * sin(alpha)) + center);
        }
    }


};

