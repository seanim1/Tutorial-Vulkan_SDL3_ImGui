
## User Guide:
Change the color parameter on GUI and watch the color of the triangle update in real-time via uniform

## Updated:
main.cpp
shaders/shader.frag
shaders/shader.vert

## New:
New functions:

create_uniform_buffers — one buffer per swapchain image, kept permanently mapped
create_descriptors — descriptor set layout, pool, and sets wiring the uniform buffer to binding 0 in the fragment shader
update_uniform_buffer — called every frame, writes clear_r/g/b into the mapped buffer for the current image index

In init_vulkan:

create_uniform_buffers and create_descriptors called before pipeline layout
Pipeline layout updated to include descriptor_set_layout
vkCmdBindDescriptorSets added to command buffer recording

In render_frame:

update_uniform_buffer(app, image_index) called each frame

Shaders:

Vertex shader — unchanged, just positions
Fragment shader — reads ubo.color from layout(binding = 0) and outputs it

CMakeLists.txt:

Replaced file(COPY) with add_custom_target that compiles shaders via glslangValidator directly into the build directory on every build