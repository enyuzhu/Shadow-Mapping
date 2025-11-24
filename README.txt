This project implements real-time shadow mapping in OpenGL, covering depth-map generation, shadow rendering, and common artifacts like shadow acne and Peter Panning. It follows a basic graphics pipeline and demonstrates how light-space depth information is used to determine fragment visibility.

**Features**
- Depth map generation from the light’s perspective  
- Shadow mapping using a depth comparison  
- Basic scene rendering with shadow application  
- Adjustable light direction and camera view  
- Handles common artifacts (shadow acne, Peter Panning)

**How to Run**
1. Clone the repository  
2. Build using CMake  
3. Run the executable from your build directory  
4. Use the mouse/keyboard to move the camera and adjust lighting

**Project Structure**
- src/: main rendering logic  
- shaders/: vertex + fragment shaders for depth and shadow passes  
- includes/: utilities for shader compilation and camera control  
- resources/: geometry + textures (if used)

**Dependencies**
- OpenGL 3.3+  
- GLFW  
- GLAD  
- glm math library  
- CMake

**Controls**
- WASD → move camera  
- Mouse → look around  
- Q/E → raise or lower the light  
- R → reset camera  

**Future Improvements**
- Soft shadows (PCF)  
- Cascaded shadow maps  
- Point-light shadow mapping (cubemap shadows)
