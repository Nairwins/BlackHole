#pragma once
#include <glm/glm.hpp>
#include <vector>

// ------------------ Ray Structures ------------------
struct Ray2D {
    double r, phi;           // polar position
    double dr_dlambda, dphi_dlambda; // polar velocities
    glm::dvec2 pos;          // Cartesian for trail
};

struct Ray3D {
    glm::dvec3 pos;
    glm::dvec3 vel;
};

// ------------------ API Functions ------------------
// Initialize rays on GPU
void initRays2D(Ray2D* rays, int numRays, glm::dvec2 startPos, glm::dvec2 direction);
void initRays3D(Ray3D* rays, int numRays, glm::dvec3 startPos, glm::dvec3 direction);

// Run simulation for one step
void updateRays2D(Ray2D* rays, int numRays, double r_s, double step);
void updateRays3D(Ray3D* rays, int numRays, glm::dvec3 bhPos, double bhMass, double G, double dt);

// Copy results back to CPU
void copyRays2DToHost(Ray2D* rays, glm::dvec2* trail, int numRays, int trailLength);
void copyRays3DToHost(Ray3D* rays, glm::dvec3* trail, int numRays, int trailLength);
