#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <vector>

// ------------------- Global Variables -------------------
float lastX = 400, lastY = 300;
bool leftMousePressed = false;
float orbitYaw = 0.0f;
float orbitPitch = 10.0f; 
float camDist = 12.0f;   

// ------------------- Input Callbacks  -------------------
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods){
    if(button==GLFW_MOUSE_BUTTON_LEFT)
        leftMousePressed = (action==GLFW_PRESS);
}

void cursor_position_callback(GLFWwindow* window, double xpos, double ypos){
    static float sensitivity = 0.3f;
    if(leftMousePressed){
        float dx = (float)xpos - lastX;
        float dy = (float)ypos - lastY;
        orbitYaw += dx * sensitivity;
        orbitPitch += dy * sensitivity;
        if(orbitPitch > 89.0f) orbitPitch = 89.0f;
        if(orbitPitch < -89.0f) orbitPitch = -89.0f;
    }
    lastX = (float)xpos;
    lastY = (float)ypos;
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset){
    camDist -= (float)yoffset * 2.0f;
    if(camDist < 3.0f) camDist = 3.0f;
    if(camDist > 100.0f) camDist = 100.0f;
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

// ------------------- Shaders -------------------
const char* vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec2 aPos;
out vec2 TexCoords;
void main() {
    TexCoords = aPos;
    gl_Position = vec4(aPos, 0.0, 1.0);
}
)";

const char* fragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;
in vec2 TexCoords;

uniform vec2 u_resolution;
uniform vec3 u_camPos;
uniform mat4 u_viewInv;
uniform float u_time;

#define MAX_STEPS 50
#define STEP_SIZE 0.08
#define BH_RADIUS 1.0
#define DISK_INNER 1.8
#define DISK_OUTER 5.5

// --- Noise Functions for Gas Texture ---
float hash(float n) { return fract(sin(n) * 43758.5453123); }
float noise(vec3 x) {
    vec3 p = floor(x);
    vec3 f = fract(x);
    f = f * f * (3.0 - 2.0 * f);
    float n = p.x + p.y * 57.0 + 113.0 * p.z;
    return mix(mix(mix(hash(n + 0.0), hash(n + 1.0), f.x),
                   mix(hash(n + 57.0), hash(n + 58.0), f.x), f.y),
               mix(mix(hash(n + 113.0), hash(n + 114.0), f.x),
                   mix(hash(n + 170.0), hash(n + 171.0), f.x), f.y), f.z);
}

// Fractal Brownian Motion (layered noise)
float fbm(vec3 p) {
    float f = 0.0;
    float w = 0.5;
    for (int i = 0; i < 4; i++) {
        f += w * noise(p);
        p *= 2.0;
        w *= 0.5;
    }
    return f;
}

// --- Background: Stars & Floor Grid ---
vec3 getBackground(vec3 dir, vec3 pos) {
    // 1. Stars (White speckles based on direction)
    float starDensity = noise(dir * 150.0); // High freq noise
    vec3 bg = vec3(0.0);
    if(starDensity > 0.96) bg = vec3(pow((starDensity-0.96)*25.0, 4.0));
    
    // 2. Floor Grid (Infinite plane at Y = -2.0)
    // We project the ray to hit the plane y = -2
    // Ray equation: P = O + t*D. We want P.y = -2.
    // -2 = O.y + t*D.y  =>  t = (-2 - O.y) / D.y
    if (dir.y < -0.01) {
        float t = (-2.0 - pos.y) / dir.y;
        if (t > 0.0) {
            vec3 hitPos = pos + dir * t;
            // Create grid lines
            float gridSize = 1.0;
            float lineThickness = 0.05;
            
            // Grid math
            float gx = fract(hitPos.x * gridSize);
            float gz = fract(hitPos.z * gridSize);
            
            // Fade grid into distance
            float fade = max(0.0, 1.0 - length(hitPos.xz) / 30.0);
            
            if ((gx < lineThickness || gz < lineThickness) && fade > 0.0) {
                bg += vec3(0.0, 0.8, 1.0) * 0.5 * fade; // Cyan grid
            }
        }
    }
    return bg;
}

void main() {
    // Standard normalized coordinates
    vec2 uv = TexCoords;
    uv.x *= u_resolution.x / u_resolution.y;

    float fov = 1.4;
    vec3 rayDirView = normalize(vec3(uv, -1.0 / tan(fov / 2.0)));
    vec3 rayDir = normalize(mat3(u_viewInv) * rayDirView);
    vec3 rayPos = u_camPos;

    vec3 finalColor = vec3(0.0);
    vec3 diskAccum = vec3(0.0);
    
    bool hitEventHorizon = false;

    // --- Ray Marching Physics Loop ---
    for(int i = 0; i < MAX_STEPS; i++) {
        float r = length(rayPos);
        
        // 1. Event Horizon (Black Hole center)
        if(r < BH_RADIUS) {
            hitEventHorizon = true;
            break;
        }
        
        // 2. Accretion Disk Physics (Volumetric)
        // Check if we are close to the Y=0 plane
        float distToPlane = abs(rayPos.y);
        
        // If we are inside the disk's vertical thickness and radial bounds
        if(distToPlane < 0.2 && r > DISK_INNER && r < DISK_OUTER) {
            
            // Calculate rotational coordinates for texture
            float angle = atan(rayPos.z, rayPos.x);
            // Spin the noise over time
            float rotOffset = u_time * (2.0 / r); // Inner parts spin faster
            
            // Get Noise density
            float gas = fbm(vec3(r * 2.0, angle * 3.0 + rotOffset, 0.0));
            
            // Shape the disk: Fade out edges
            float radialFade = smoothstep(DISK_INNER, DISK_INNER + 0.5, r) * (1.0 - smoothstep(DISK_OUTER - 1.0, DISK_OUTER, r));
            float verticalFade = 1.0 - (distToPlane / 0.2); // Fades as you go up/down from center
            
            float density = gas * radialFade * verticalFade * 0.2; // 0.2 is opacity factor
            
            // --- RELATIVISTIC BEAMING (DOPPLER) ---
            // Gas rotates Counter-Clockwise around Y. Tangent vector is (-z, 0, x)
            vec3 diskVel = normalize(vec3(-rayPos.z, 0.0, rayPos.x));
            float doppler = dot(diskVel, rayDir); // Dot product with view ray
            // If doppler > 0 (gas coming at us), it's brighter/bluer.
            // If doppler < 0 (gas moving away), it's dimmer/redder.
            float beamIntensity = 1.0 + doppler * 0.6; 
            
            // Color Palette (Temperature)
            // Hot (inner) = White/Blue, Cool (outer) = Orange/Red
            vec3 hotColor = vec3(0.6, 0.8, 1.0);
            vec3 coolColor = vec3(1.0, 0.2, 0.05);
            vec3 baseColor = mix(hotColor, coolColor, (r - DISK_INNER) / (DISK_OUTER - DISK_INNER));
            
            // Accumulate light (Additive blending)
            diskAccum += baseColor * density * beamIntensity;
        }

        // 3. Gravity (Curve the light)
        // Force F = 1/r^2. Simple Euler integration for direction.
        // We multiply by a large constant (1.5 * BH_RADIUS) to exaggerate visual bending
        vec3 toCenter = normalize(-rayPos);
        float force = (1.5 * BH_RADIUS) / (r * r); 
        
        // Bend the ray direction
        rayDir = normalize(rayDir + toCenter * force * STEP_SIZE);
        
        // Move Ray
        rayPos += rayDir * STEP_SIZE * min(r, 5.0); // Adaptive step size helps performance
        
        if(r > 30.0) break; // Escape to infinity
    }

    if(!hitEventHorizon) {
        // Sample background if we didn't hit the black circle
        // Pass 'rayPos' so grid knows where the ray ended up
        finalColor = getBackground(rayDir, rayPos); 
    }

    // Add the glowing disk on top (it glows even in front of the black hole)
    // Apply simple tone mapping
    finalColor += diskAccum;
    
    FragColor = vec4(finalColor, 1.0);
}
)";

unsigned int createShader(const char* vSource, const char* fSource) {
    unsigned int vShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vShader, 1, &vSource, NULL);
    glCompileShader(vShader);
    
    // Debug compilation
    int success; char infoLog[512];
    glGetShaderiv(vShader, GL_COMPILE_STATUS, &success);
    if(!success) { glGetShaderInfoLog(vShader, 512, NULL, infoLog); std::cout << "Vertex Error: " << infoLog << std::endl; }

    unsigned int fShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fShader, 1, &fSource, NULL);
    glCompileShader(fShader);
    
    glGetShaderiv(fShader, GL_COMPILE_STATUS, &success);
    if(!success) { glGetShaderInfoLog(fShader, 512, NULL, infoLog); std::cout << "Fragment Error: " << infoLog << std::endl; }

    unsigned int prog = glCreateProgram();
    glAttachShader(prog, vShader);
    glAttachShader(prog, fShader);
    glLinkProgram(prog);
    glDeleteShader(vShader);
    glDeleteShader(fShader);
    return prog;
}

int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1200, 900, "Advanced Black Hole Simulation", NULL, NULL);
    if (window == NULL) { glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);
    
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetCursorPosCallback(window, cursor_position_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) { return -1; }

    unsigned int shaderProgram = createShader(vertexShaderSource, fragmentShaderSource);

    float quadVertices[] = { -1.0f, -1.0f,  1.0f, -1.0f,  -1.0f,  1.0f,
                             -1.0f,  1.0f,  1.0f, -1.0f,   1.0f,  1.0f };
    unsigned int VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);

    while (!glfwWindowShouldClose(window)) {
        float time = (float)glfwGetTime();

        // Camera Logic
        float radYaw = glm::radians(orbitYaw);
        float radPitch = glm::radians(orbitPitch);
        float camX = camDist * cos(radPitch) * sin(radYaw);
        float camY = camDist * sin(radPitch);
        float camZ = camDist * cos(radPitch) * cos(radYaw);
        glm::vec3 cameraPos = glm::vec3(camX, camY, camZ);
        
        glm::mat4 view = glm::lookAt(cameraPos, glm::vec3(0,0,0), glm::vec3(0,1,0));
        glm::mat4 viewInv = glm::inverse(view);

        glUseProgram(shaderProgram);
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        glUniform2f(glGetUniformLocation(shaderProgram, "u_resolution"), (float)width, (float)height);
        glUniform3f(glGetUniformLocation(shaderProgram, "u_camPos"), cameraPos.x, cameraPos.y, cameraPos.z);
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "u_viewInv"), 1, GL_FALSE, glm::value_ptr(viewInv));
        glUniform1f(glGetUniformLocation(shaderProgram, "u_time"), time);

        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}