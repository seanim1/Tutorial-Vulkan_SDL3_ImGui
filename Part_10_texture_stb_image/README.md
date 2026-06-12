
## User Guide:
Try moving sliders on the GUI

## Updated:
main.cpp
CMakeLists.txt
shaders/shader.frag
shaders/shader.vert

## New:
stb_image include added
Vertex — added glm::vec2 uv
vertices — UV coordinates added per vertex
AppState — 4 new texture members
transition_image_layout — new helper function
create_texture — new function, loads PNG, uploads via staging buffer, creates image view and sampler
create_descriptors — now has 2 bindings (UBO + sampler) and 2 pool sizes
init_vulkan — command pool moved earlier (needed by create_texture), create_texture called before create_descriptors
attr_descs — now 2 attributes (position + uv), vertexAttributeDescriptionCount = 2
cleanup_vulkan — texture cleanup added

## Motivating next step:
Currently, changing VK_CULL_MODE_BACK_BIT to VK_CULL_MODE_NONE will see back faces bleeding through the front faces. Next step is depth bufferring 