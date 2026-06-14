
## User Guide:

## Updated:
main.cpp

## New:
Vertex gains glm::vec3 normal
QuadVertex is a separate struct (position + uv only) — face pipeline uses its own vertex input desc
Two vertex + index buffer pairs for flat/smooth
UniformData gains model, light_pos, cam_pos
record_scene_command_buffers extracted from init so it can be called again when toggling shading mode
ImGui checkbox toggles flat/smooth, calls vkDeviceWaitIdle then re-records
AppState gains flat_shade and light_pos
UBO binding stageFlags changed to VERTEX_BIT | FRAGMENT_BIT for tetra (fragment needs light/cam pos)
