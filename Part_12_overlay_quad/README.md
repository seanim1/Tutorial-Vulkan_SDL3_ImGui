
## User Guide:

## Updated:
main.cpp
## added:
shaders/face.frag

## New:
quad_vertices — mathematically computed from the front face centroid + tangent frame, offset 0.01 along the outward normal
create_buffer helper — avoids repeating the same buffer creation pattern
create_quad_buffers — uploads quad geometry
quad_uniform_buffers — same MVP as tetrahedron, separate descriptor sets
face_pipeline + face_pipeline_layout — alpha blending enabled, depth write off, no backface culling
face_descriptor_set_layout + face_descriptor_pool + face_descriptor_sets — UBO only, no sampler
Two draw calls in the command buffer — tetrahedron first, quad second
face.spv loaded and used as fragment shader for the face pipeline