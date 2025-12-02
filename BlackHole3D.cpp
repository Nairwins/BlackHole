#define _USE_MATH_DEFINES
#include <math.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <iostream>

// ------------------- Config -------------------
const int GRID_SIZE = 50;
const float SIMULATION_G = 1.0f;       
const float BH_MASS = 13200.0f;         
const float LIGHT_SPEED = 50.0f;       // Slightly faster for better visuals
const float EVENT_HORIZON = 1.0f;      

// Sphere
float SPHERE_RADIUS = 2.0f;
glm::vec3 spherePos(0.0f, 0.0f, 0.0f); 

// Camera
glm::vec3 orbitCenter = spherePos;
float camDist = 60.0f;
float orbitYaw = -90.0f, orbitPitch = 10.0f;

// Ray Config
float RAY_WIDTH = 2.0f;
int TRAIL_LENGTH = 1000; // Shorter trail for better performance

// Input
bool leftMousePressed = false;
float lastX = 640, lastY = 360;

// ------------------- Shader Helpers -------------------
unsigned int createShaderProgram(const char* vsSrc, const char* fsSrc) {
    unsigned int vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vsSrc, NULL);
    glCompileShader(vs);
    unsigned int fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fsSrc, NULL);
    glCompileShader(fs);
    unsigned int prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glDeleteShader(vs);
    glDeleteShader(fs);
    return prog;
}

void setMat4(unsigned int shader, const char* name, glm::mat4& mat) {
    glUniformMatrix4fv(glGetUniformLocation(shader,name),1,GL_FALSE,glm::value_ptr(mat));
}

void setVec3(unsigned int shader, const char* name, glm::vec3& vec) {
    glUniform3fv(glGetUniformLocation(shader,name),1,&vec[0]);
}

// ------------------- Input Callbacks -------------------
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
    if(camDist < 5.0f) camDist = 5.0f;
    if(camDist > 200.0f) camDist = 200.0f;
}

// ------------------- BlackHole Class -------------------
struct BlackHole {
    unsigned int VAO, VBO, EBO;
    unsigned int indexCount;
    float radius;

    BlackHole(float r, int stacks=30, int slices=30) : radius(r) {
        std::vector<float> verts;
        std::vector<unsigned int> inds;
        for(int i=0; i<=stacks; i++){
            float v = (float)i/stacks;
            float phi = v * M_PI;
            for(int j=0; j<=slices; j++){
                float u = (float)j/slices;
                float theta = u * 2 * M_PI;
                float x = radius * sin(phi) * cos(theta);
                float y = radius * cos(phi);
                float z = radius * sin(phi) * sin(theta);
                verts.push_back(x); verts.push_back(y); verts.push_back(z);
            }
        }
        for(int i=0; i<stacks; i++){
            for(int j=0; j<slices; j++){
                int a = i*(slices+1)+j;
                int b = a+slices+1;
                inds.push_back(a); inds.push_back(b); inds.push_back(a+1);
                inds.push_back(b); inds.push_back(b+1); inds.push_back(a+1);
            }
        }
        indexCount = inds.size();
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);
        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, verts.size()*sizeof(float), verts.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, inds.size()*sizeof(unsigned int), inds.data(), GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3*sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glBindVertexArray(0);
    }

    void draw(){ 
        glBindVertexArray(VAO); 
        glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0); 
    }
};

// ------------------- Ray Point -------------------
struct RayPoint {
    glm::vec3 position;
    glm::vec3 velocity;
    glm::vec3 originalPos;
    glm::vec3 originalVel;

    std::vector<glm::vec3> trail;
    unsigned int VAO, VBO;
    unsigned int shader;
    
    bool finished = false;
    bool hasHit = false;

    RayPoint(glm::vec3 startPos, glm::vec3 startVel)
        : position(startPos), velocity(startVel), originalPos(startPos), originalVel(startVel)
    {
        trail.resize(TRAIL_LENGTH, position);

        // GPU buffers
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, TRAIL_LENGTH * sizeof(glm::vec3), trail.data(), GL_DYNAMIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
        glEnableVertexAttribArray(0);
        glBindVertexArray(0);

        // Shader
        const char* vs = R"(
            #version 330 core
            layout(location=0) in vec3 aPos;
            uniform mat4 view;
            uniform mat4 proj;
            void main() {
                gl_Position = proj * view * vec4(aPos, 1.0);
            }
        )";
        const char* fs = R"(
            #version 330 core
            out vec4 FragColor;
            uniform int trailLen;
            uniform int vertIdx; // NOTE: This requires drawing one by one or instancing. 
                                 // For simplicity here we use a solid color that fades by Z-buffer fighting or simple blend
            void main() {
                FragColor = vec4(1.0, 0.8, 0.3, 1.0);
            }
        )";
        shader = createShaderProgram(vs, fs);
    }

    // Reset function to restart simulation
    void reset() {
        position = originalPos;
        velocity = originalVel;
        std::fill(trail.begin(), trail.end(), originalPos);
        finished = false;
        hasHit = false;
        
        // Force immediate buffer update so we don't see old trails
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, TRAIL_LENGTH * sizeof(glm::vec3), trail.data());
    }

    void update(float dt) {
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

    void draw(glm::mat4& view, glm::mat4& proj) {
        glUseProgram(shader);
        setMat4(shader, "view", view);
        setMat4(shader, "proj", proj);
        glBindVertexArray(VAO);
        glDrawArrays(GL_LINE_STRIP, 0, TRAIL_LENGTH);
    }
};

// ------------------- Grid -------------------
unsigned int gridVAO, gridVBO;
void createGrid(int size){
    std::vector<float> grid;
    for(int i=-size;i<=size;i+=5){
        grid.push_back((float)-size); grid.push_back(-10.0f); grid.push_back((float)i);
        grid.push_back((float)size);  grid.push_back(-10.0f); grid.push_back((float)i);
        grid.push_back((float)i);     grid.push_back(-10.0f); grid.push_back((float)-size);
        grid.push_back((float)i);     grid.push_back(-10.0f); grid.push_back((float)size);
    }
    glGenVertexArrays(1,&gridVAO);
    glGenBuffers(1,&gridVBO);
    glBindVertexArray(gridVAO);
    glBindBuffer(GL_ARRAY_BUFFER,gridVBO);
    glBufferData(GL_ARRAY_BUFFER,grid.size()*sizeof(float),grid.data(),GL_STATIC_DRAW);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(float),(void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}


// ------------------- Main -------------------
int main(){
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
    glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);
    
    GLFWwindow* window = glfwCreateWindow(1280, 720, "3D BlackHole Simulation", NULL, NULL);
    glfwMakeContextCurrent(window);
    glfwSetCursorPosCallback(window,cursor_position_callback);
    glfwSetMouseButtonCallback(window,mouse_button_callback);
    glfwSetScrollCallback(window,scroll_callback);

    if(!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)){
        return -1;
    }
    glEnable(GL_DEPTH_TEST);
    
    // Shader Init
    const char* gridVS="#version 330 core\nlayout(location=0) in vec3 aPos; uniform mat4 view; uniform mat4 proj; void main(){gl_Position=proj*view*vec4(aPos,1.0);}";
    const char* gridFS="#version 330 core\nout vec4 FragColor; void main(){FragColor=vec4(0.2,0.2,0.2,1.0);}";
    unsigned int gridShader=createShaderProgram(gridVS,gridFS);

    const char* sphereVS="#version 330 core\nlayout(location=0) in vec3 aPos; uniform mat4 model; uniform mat4 view; uniform mat4 proj; void main(){gl_Position=proj*view*model*vec4(aPos,1.0);}";
    const char* sphereFS="#version 330 core\nout vec4 FragColor; uniform vec3 color; void main(){FragColor=vec4(color,1.0);}";
    unsigned int sphereShader=createShaderProgram(sphereVS,sphereFS);

    createGrid(GRID_SIZE);
    BlackHole blackHole(SPHERE_RADIUS);

    // Initialize Rays
    std::vector<RayPoint> rays;
    int cols = 25;
    int rows = 25;
    float separation = 1.0f;

    for(int i=0; i<rows; i++){
        for (int j=0; j<cols; j++){
            float z = (i - rows/2.0f) * separation;
            float y = (j - cols/2.0f) * separation;
            float x = 70.0f; // Start far right

            glm::vec3 pos(x, y, z);
            glm::vec3 vel(-LIGHT_SPEED, 0.0f, 0.0f); // Move left

            rays.emplace_back(pos, vel);
        }
    }

    float lastFrame = 0.0f;

    while(!glfwWindowShouldClose(window)){
        float currentFrame = (float)glfwGetTime();
        float deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // --- FIX: CLAMP DELTA TIME ---
        // This prevents the "explosion" bug on the first frame or during lag spikes
        if (deltaTime > 0.05f) deltaTime = 0.05f; 

        // --- RESET INPUT ---
        if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) {
            for(auto &ray : rays) ray.reset();
        }

        // Camera
        glm::vec3 camPos;
        camPos.x = orbitCenter.x + camDist * cos(glm::radians(orbitYaw)) * cos(glm::radians(orbitPitch));
        camPos.y = orbitCenter.y + camDist * sin(glm::radians(orbitPitch));
        camPos.z = orbitCenter.z + camDist * sin(glm::radians(orbitYaw)) * cos(glm::radians(orbitPitch));

        glm::mat4 view = glm::lookAt(camPos, orbitCenter, glm::vec3(0,1,0));
        glm::mat4 proj = glm::perspective(glm::radians(45.0f), 1280.f/720.f, 0.1f, 500.f);

        glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Draw Grid
        glUseProgram(gridShader);
        setMat4(gridShader,"view",view);
        setMat4(gridShader,"proj",proj);
        glBindVertexArray(gridVAO);
        glDrawArrays(GL_LINES, 0, (GRID_SIZE+1)*8);

        // Draw Black Hole
        glUseProgram(sphereShader);
        glm::mat4 model = glm::translate(glm::mat4(1.0f), spherePos);
        setMat4(sphereShader,"model",model);
        setMat4(sphereShader,"view",view);
        setMat4(sphereShader,"proj",proj);
        glm::vec3 black(0.0f); 
        setVec3(sphereShader,"color",black);
        blackHole.draw();

        // Draw Rays
        glLineWidth(RAY_WIDTH);
        for(auto &ray : rays){
            ray.update(deltaTime);
            ray.draw(view, proj);
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1,&gridVAO);
    glDeleteBuffers(1,&gridVBO);
    glfwTerminate();
    return 0;
}