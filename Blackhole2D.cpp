#define _USE_MATH_DEFINES

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <iostream>
#include <math.h>
#include <vector>

using namespace glm;

// Gravitational Constant (m^3 kg^-1 s^-2)
const double G = 6.67430e-11;
// Speed of Light (m s^-1)
const double c = 299792458.0;

// Affine parameter step size (arbitrary for visual simulation)
const double GEODESIC_STEP = 0.02; 

class Window {
private:
    int width, height;

public:
    GLFWwindow* handle = nullptr;

    bool create(int w, int h, const char* title) {
        width = w;
        height = h;

        if (!glfwInit()) {
            std::cerr << "GLFW init failed\n";
            return false;
        }

        handle = glfwCreateWindow(w, h, title, nullptr, nullptr);
        if (!handle) {
            std::cerr << "Window creation failed\n";
            glfwTerminate();
            return false;
        }

        glfwMakeContextCurrent(handle);

        // Set up proper 2D coordinate system to prevent stretching
        setup2DView();

        return true;
    }

    void setup2DView() {
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();

        // Maintain aspect ratio - this keeps circles circular!
        float aspect = (float)width / (float)height;
        if (aspect > 1.0f) {
            glOrtho(-aspect, aspect, -1.0f, 1.0f, -1.0f, 1.0f);
        } else {
            glOrtho(-1.0f, 1.0f, -1.0f/aspect, 1.0f/aspect, -1.0f, 1.0f);
        }

        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
    }

    void beginFrame() {
        // Set background to black for space theme
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    void endFrame() {
        glfwSwapBuffers(handle);
        glfwPollEvents();
    }

    bool shouldClose() {
        return glfwWindowShouldClose(handle);
    }

    void cleanup() {
        glfwDestroyWindow(handle);
        glfwTerminate();
    }

    int getWidth() const { return width; }
    int getHeight() const { return height; }
};

struct BlackHole {
    vec2 position;
    double mass;
    double r_s; // Schwarzschild Radius

    // Constructor using a normalized radius for the simulation scale
    BlackHole(vec2 pos, float normalized_radius) : position(pos), r_s(normalized_radius), mass(1.0) {}

    // Constructor to calculate r_s from mass
    BlackHole(vec2 pos, double m) : position(pos), mass(m) {
        // In a realistic simulation, r_s would be tiny. 
        // We're using a scaled value here for visualization.
        // r_s = 2.0 * G * mass / (c * c); 
        // For visualization, we'll keep the direct radius or use a scaled mass:
        r_s = 2.0 * m / 1000.0; // Example scaling
    }

    void draw() {
        glColor3f(0.0f, 0.0f, 0.0f); // Make the hole black
        
        glBegin(GL_TRIANGLE_FAN);
        glVertex2f(position.x, position.y);

        for (int i = 0; i <= 100; i++) {
            float angle = i * 2.0f * M_PI / 100.0f;
            float x = position.x + cosf(angle) * r_s;
            float y = position.y + sinf(angle) * r_s;
            glVertex2f(x, y);
        }

        glEnd();
        
        // Draw a white ring for the event horizon
        glColor3f(1.0f, 1.0f, 1.0f);
        glBegin(GL_LINE_LOOP);
        for (int i = 0; i <= 100; i++) {
            float angle = i * 2.0f * M_PI / 100.0f;
            float x = position.x + cosf(angle) * r_s;
            float y = position.y + sinf(angle) * r_s;
            glVertex2f(x, y);
        }
        glEnd();
    }

    float getSchwarzschildRadius() const { return (float)r_s; }
};

struct Ray {
    double r, phi; // Polar coordinates
    double dr_dlambda, dphi_dlambda; // Polar velocity (derivatives w.r.t affine parameter lambda)

    std::vector<vec2> trail;
    const size_t maxTrailLength = 1000;

    Ray(vec2 pos, vec2 dir) {
        // Initial Cartesian to Polar conversion
        r = length(pos);
        phi = atan2(pos.y, pos.x);

        // Initial Polar Velocity Calculation (Normalized for simulation scale)
        // dir is the initial tangent vector (dx, dy). We need (dr/dlambda, dphi/dlambda).
        double magnitude_inv = 1.0 / length(dir);
        
        // dr/dlambda = (x dx + y dy) / r
        dr_dlambda = (pos.x * dir.x + pos.y * dir.y) / r;
        
        // dphi/dlambda = (x dy - y dx) / r^2
        dphi_dlambda = (pos.x * dir.y - pos.y * dir.x) / (r * r);

        // Normalize the initial polar velocity components for a consistent step size
        double current_mag = sqrt(dr_dlambda * dr_dlambda + (r * r) * dphi_dlambda * dphi_dlambda);
        double factor = magnitude_inv / current_mag;

        dr_dlambda *= factor;
        dphi_dlambda *= factor;

        trail.push_back(pos);
    }

    void draw() {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glLineWidth(2.0f);

        size_t N = trail.size();
        if (N < 2) return;

        glBegin(GL_LINE_STRIP);
        for (size_t i = 0; i < N; ++i) {
            float alpha = float(i) / float(N - 1);
            // Yellow color, fades out at the tail
            glColor4f(1.0f, 1.0f, 0.0f, std::max(alpha, 0.05f)); 
            glVertex2f(trail[i].x, trail[i].y);
        }
        glEnd();
        
        glDisable(GL_BLEND);
    }

    void update(float r_s) {
        // Only update if outside the event horizon
        if (r < r_s) return; 

        // Convert polar velocity back to cartesian direction for simple position update
        // We use the full geodesic equations for the *change* in velocity (dr_dlambda, dphi_dlambda)
        
        // Convert to Cartesian position for drawing
        double x = r * cos(phi);
        double y = r * sin(phi);

        // Add to trail
        trail.push_back(vec2(x, y));

        // Limit trail length
        if (trail.size() > maxTrailLength) {
            trail.erase(trail.begin());
        }
    }
};

/**
 * @brief Calculates the acceleration terms for the null geodesic (light ray)
 * using the Schwarzschild metric.
 * * This function modifies the ray's velocity (dr/dlambda and dphi/dlambda)
 * using the Runge-Kutta integration, which is simplified here to a single
 * Euler step for demonstration, using the calculated second derivatives.
 * * @param ray The Ray object to update.
 * @param r_s The Schwarzschild radius of the Black Hole.
 */
void geodesic(Ray& ray, double r_s) {
    double r = ray.r;
    double dr = ray.dr_dlambda;
    double dphi = ray.dphi_dlambda;

    // --- Schwarzschild Null Geodesic Equations (d^2/dlambda^2) ---
    
    // d^2r/dlambda^2 
    // This is the radial acceleration term
    // d^2r/dlambda^2 = r * (dphi/dlambda)^2 * (1 - r_s/r) + r_s * c^2 / (2 * r^2) * (d_tau/d_lambda)^2 * (1 - r_s/r)
    // For null geodesics, the equation simplifies to:
    double d2r_dlambda2 = r * dphi * dphi * (1.0 - r_s / r) - 
                          r_s * dr * dr / (2.0 * r * (r - r_s)); 

    // d^2phi/dlambda^2
    // This is the angular acceleration term (Conservation of Angular Momentum)
    // d^2phi/dlambda^2 = -2/r * dr/dlambda * dphi/dlambda
    double d2phi_dlambda2 = -2.0 * dr * dphi / r; 

    // --- Euler Integration Step ---
    // Update velocities (dr/dlambda, dphi/dlambda) using the calculated acceleration
    ray.dr_dlambda += d2r_dlambda2 * GEODESIC_STEP;
    ray.dphi_dlambda += d2phi_dlambda2 * GEODESIC_STEP;

    // Update position (r, phi) using the new velocity
    ray.r += ray.dr_dlambda * GEODESIC_STEP;
    ray.phi += ray.dphi_dlambda * GEODESIC_STEP;
}


int main() {
    Window window;
    if (!window.create(800, 600, "Black Hole Raytracing")) {
        return -1;
    }

    // Create black hole at the center
    // We use a normalized radius of 0.2 units for a good visual scale on the [-1, 1] x [-aspect, aspect] view.
    BlackHole Ton68(vec2(0.0f, 0.0f), 0.2f); 

    // Create rays
    std::vector<Ray> rays;
    
    // Add rays pointing right from the left side
    int numRays = 30;
    float start_x = -1.0f * (float)window.getWidth() / window.getHeight(); // Start outside the view
    
    for (int i = 0; i < numRays; ++i) {
        float y = -0.9f + (1.8f * i) / (numRays - 1); // Spread rays vertically
        // Start from left side, pointing right (direction = (1.0, 0.0))
        rays.push_back(Ray(vec2(start_x, y), normalize(vec2(1.0f, 0.0f))));
    }

    while (!window.shouldClose()) {
        window.beginFrame();

        Ton68.draw();

        // Update and draw all rays
        for (auto& ray : rays) {
            // Check if the ray has fallen into the event horizon
            if (ray.r < Ton68.getSchwarzschildRadius()) {
                // Ray is inside, stop tracing
            } else {
                geodesic(ray, Ton68.getSchwarzschildRadius());
                ray.update(Ton68.getSchwarzschildRadius());
            }
            ray.draw();
        }

        window.endFrame();
    }

    window.cleanup();
    return 0;
}