#version 450

layout(binding = 0) uniform UniformData {
    mat4 mvp;
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inUV;

layout(location = 0) out vec2 outUV;

void main() {
    gl_Position = ubo.mvp * vec4(inPosition, 1.0);
    outUV = inUV;
}