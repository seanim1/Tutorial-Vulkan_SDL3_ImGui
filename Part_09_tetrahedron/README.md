
## User Guide:
Try moving sliders on the GUI

## Updated:
main.cpp
CMakeLists.txt
shaders/shader.frag
shaders/shader.vert

## New:
Geometry — replaced the flat 2D triangle with 4 vertices and 12 indices forming a proper tetrahedron with 4 triangular faces.
MVP matrix — replaced the color uniform with a glm::mat4 mvp. Each frame update_uniform_buffer computes model (rotation), view (camera looking at origin from z=-4), and projection (perspective with Y flipped for Vulkan), multiplies them together, and uploads the result to the GPU.
Shaders — vertex shader now reads the MVP matrix from the uniform and applies it to each vertex position. Fragment shader hardcodes an orange color since per-vertex color isn't needed yet.
ImGui — replaced the color picker with three rotation sliders (X, Y, Z). Y auto-increments each frame to drive the animation; X and Z are user-controlled.
stageFlags — changed from VK_SHADER_STAGE_FRAGMENT_BIT to VK_SHADER_STAGE_VERTEX_BIT