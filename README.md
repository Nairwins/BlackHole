# 🌌 Black Hole Simulations

> Experience gravity, light bending, and the beauty of black holes in 2D & 3D!


This repository contains **three high-end black hole simulations** in **C++** using **OpenGL**, **GLFW**, and **GLM**. Each simulation explores different visual and physical effects, from ray-marched volumetric disks to realistic Schwarzschild ray tracing.

---

## 🚀 Simulations

### 3D Ray-Marched Black Hole
- **File:** `BlackHoleSimulation1.cpp`
- **Features:**
  - Volumetric accretion disk with Doppler effect
  - Grid floor & starry background
  - Relativistic light bending
- **Controls:**  
  - Orbit: Left Mouse Drag  
  - Zoom: Scroll Wheel  
![Sim1](screenshots/sim1.gif)

---

### 3D Particle Rays Around a Black Hole
- **File:** `BlackHoleSimulation2.cpp`
- **Features:**
  - Particle-based light rays
  - Trails show light paths
  - Reset simulation: `R`
- **Controls:**  
  - Orbit: Left Mouse Drag  
  - Zoom: Scroll Wheel  
![Sim2](screenshots/sim2.gif)

---

### 2D Schwarzschild Ray Tracing
- **File:** `BlackHoleSimulation3.cpp`
- **Features:**
  - 2D visualization of null geodesics
  - Ray trails falling into the event horizon
  - Simplified Schwarzschild physics
![Sim3](screenshots/sim3.gif)

---

## ⚙️ **Dependencies:**
   - GLFW, GLAD, GLM
   - OpenGL 3.3+
