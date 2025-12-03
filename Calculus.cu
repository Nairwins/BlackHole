#include "Calculus.h"
#include <glm/glm.hpp>
#include <math.h>

#define MAX_TRAIL 1000


// ------------------ 3D Kernel ------------------
__global__ 
void gravity3DKernel(Ray3D* rays, int numRays, glm::dvec3 bhPos, double bhMass, double G, double dt) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if(idx >= numRays) return;

    Ray3D& ray = rays[idx];
    glm::dvec3 dir = bhPos - ray.pos;
    double dist2 = glm::dot(dir, dir);
    if(dist2 < 1e-6) return;

    dir = glm::normalize(dir);
    double acc = G * bhMass / dist2;

    ray.vel += dir * acc * dt;
    ray.pos += ray.vel * dt;
}

// ------------------ 2D Kernel ------------------
__global__ 
void geodesic2DKernel(Ray2D* rays, int numRays, double r_s, double step) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if(idx >= numRays) return;

    Ray2D& ray = rays[idx];
    if(ray.r <= r_s) return;

    double r = ray.r;
    double dr = ray.dr_dlambda;
    double dphi = ray.dphi_dlambda;

    double d2r = r*dphi*dphi*(1.0 - r_s/r) - r_s*dr*dr / (2.0*r*(r - r_s));
    double d2phi = -2.0 * dr * dphi / r;

    ray.dr_dlambda += d2r * step;
    ray.dphi_dlambda += d2phi * step;

    ray.r += ray.dr_dlambda * step;
    ray.phi += ray.dphi_dlambda * step;

    // Update Cartesian position
    ray.pos.x = r * cos(ray.phi);
    ray.pos.y = r * sin(ray.phi);
}

// ------------------ Host Wrappers ------------------
void updateRays2D(Ray2D* rays, int numRays, double r_s, double step) {
    int blockSize = 256;
    int numBlocks = (numRays + blockSize - 1) / blockSize;
    geodesic2DKernel<<<numBlocks, blockSize>>>(rays, numRays, r_s, step);
    cudaDeviceSynchronize();
}

void updateRays3D(Ray3D* rays, int numRays, glm::dvec3 bhPos, double bhMass, double G, double dt) {
    int blockSize = 256;
    int numBlocks = (numRays + blockSize - 1) / blockSize;
    gravity3DKernel<<<numBlocks, blockSize>>>(rays, numRays, bhPos, bhMass, G, dt);
    cudaDeviceSynchronize();
}
