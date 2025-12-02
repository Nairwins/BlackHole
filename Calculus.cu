__global__
void 3DGravity() {
    if (finished) return;

        // 1. Calculate physics
        glm::vec3 toCenter = spherePos - position;
        float dist = glm::length(toCenter);

        if (dist < EVENT_HORIZON) {
            hasHit = true;
        } 

        else {
            glm::vec3 dir = glm::normalize(toCenter);
            float acceleration = (SIMULATION_G * BH_MASS) / (dist * dist);
            
            velocity += dir * acceleration * dt;
            velocity = glm::normalize(velocity) * LIGHT_SPEED; // Constant speed
            position += velocity * dt;
        }

        // 2. Update Trail
        if(!finished) {
            for (int i = TRAIL_LENGTH - 1; i > 0; i--) {
                trail[i] = trail[i - 1];
            }
            trail[0] = position;
        }

        // 3. Update GPU
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, TRAIL_LENGTH * sizeof(glm::vec3), trail.data());
    }

    
void 2DGravity(Ray& ray, double r_s){
    double r = ray.r;
    double dr = ray.dr_dlambda;
    double dphi = ray.dphi_dlambda;

    if (r <= 0.0) return; 

    // d^2r/dlambda^2 (Radial acceleration)
    double d2r_dlambda2 = r * dphi * dphi * (1.0 - r_s / r) - 
                          r_s * dr * dr / (2.0 * r * (r - r_s)); 

    // d^2phi/dlambda^2 (Angular acceleration)
    double d2phi_dlambda2 = -2.0 * dr * dphi / r; 

    // Euler Integration Step: Update velocities
    ray.dr_dlambda += d2r_dlambda2 * GEODESIC_STEP;
    ray.dphi_dlambda += d2phi_dlambda2 * GEODESIC_STEP;

    // Euler Integration Step: Update positions
    ray.r += ray.dr_dlambda * GEODESIC_STEP;
    ray.phi += ray.dphi_dlambda * GEODESIC_STEP;
}