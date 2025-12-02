#define _USE_MATH_DEFINES

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <iostream>
#include <math.h>
#include <vector>

using namespace glm;

// Constants Units
const double G = 6.67430e-11;
const double c = 299792458.0;
const double GEODESIC_STEP = 0.02;


//-----Window----
class Window {
private:
    int width, height;

public:
    GLFWwindow* handle = nullptr;

    bool create(int w, int h, const char* title) {
        width = w;
        height = h;

        if (!glfwInit()) {
            return false;
        }

        handle = glfwCreateWindow(w, h, title, nullptr, nullptr);
        if (!handle) {
            glfwTerminate();
            return false;
        }

        glfwMakeContextCurrent(handle);
        setup2DView();

        return true;
    }

    void setup2DView() {
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();

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

    bool isKeyPressed(int key) {
        return glfwGetKey(handle, key) == GLFW_PRESS;
    }

    void cleanup() {
        glfwDestroyWindow(handle);
        glfwTerminate();
    }

    int getWidth() const { return width; }
    int getHeight() const { return height; }
};


//----BlackHole Sphere------
struct BlackHole {
    vec2 position;
    double mass;
    double r_s;

    BlackHole(vec2 pos, float normalized_radius) : position(pos), r_s(normalized_radius), mass(1.0) {}

    BlackHole(vec2 pos, double m) : position(pos), mass(m) {
        r_s = 2.0 * m / 1000.0; // Scaled radius
    }

    void draw() {
        glColor3f(0.0f, 0.0f, 0.0f); // Black hole fill
        
        glBegin(GL_TRIANGLE_FAN);
        glVertex2f(position.x, position.y);

        for (int i = 0; i <= 100; i++) {
            float angle = i * 2.0f * M_PI / 100.0f;
            float x = position.x + cosf(angle) * r_s;
            float y = position.y + sinf(angle) * r_s;
            glVertex2f(x, y);
        }

        glEnd();
        
        glColor3f(1.0f, 1.0f, 1.0f); // White event horizon ring
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


//-----Ray Simulation-----
struct Ray {
    double r, phi; // Polar coordinates
    double dr_dlambda, dphi_dlambda; // Polar velocity
    std::vector<vec2> trail;
    const size_t maxTrailLength = 1000;

    Ray(vec2 pos, vec2 dir) {
        // Cartesian to Polar conversion
        r = length(pos);
        phi = atan2(pos.y, pos.x);

        // Polar velocity calculation
        double magnitude_inv = 1.0 / length(dir);
        
        dr_dlambda = (pos.x * dir.x + pos.y * dir.y) / r; 
        dphi_dlambda = (pos.x * dir.y - pos.y * dir.x) / (r * r); 

        // Normalize polar velocity 
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
            glColor4f(1.0f, 1.0f, 0.0f, std::max(alpha, 0.05f)); // Fading yellow color
            glVertex2f(trail[i].x, trail[i].y);
        }
        glEnd();
        
        glDisable(GL_BLEND);
    }

    void update(float r_s) {
        if (r < r_s) return; // Inside the event horizon

        double x = r * cos(phi);
        double y = r * sin(phi);

        trail.push_back(vec2(x, y));

        if (trail.size() > maxTrailLength) {
            trail.erase(trail.begin());
        }
    }
};



//------Geodesic Integration-----
void geodesic(Ray& ray, double r_s) {
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

// Initialize Rays
void initRays(std::vector<Ray>& rays, const Window& window) {
    rays.clear();

    int numRays = 30;
    float aspect_ratio = (float)window.getWidth() / window.getHeight();
    float start_x = -1.0f * aspect_ratio - 0.1f; // Start outside the view
    
    for (int i = 0; i < numRays; ++i) {
        float y = -0.9f + (1.8f * i) / (numRays - 1); // Spread rays vertically
        rays.push_back(Ray(vec2(start_x, y), normalize(vec2(1.0f, 0.0f))));
    }
}



// Main
int main() {
    Window window;
    if (!window.create(800, 600, "2D Black Hole Raytracing")) {
        return -1;
    }

    BlackHole Ton68(vec2(0.0f, 0.0f), 0.2f); 

    std::vector<Ray> rays;
    initRays(rays, window);

    while (!window.shouldClose()) {
        
        // Restart Simulation
        if (window.isKeyPressed(GLFW_KEY_R)) {
            initRays(rays, window);
        }
        
        window.beginFrame();

        Ton68.draw();

        // Update and draw all rays
        for (auto& ray : rays) {
            if (ray.r >= Ton68.getSchwarzschildRadius()) {
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