add_executable(07_uniform_buffer main.cpp)
target_link_libraries(07_uniform_buffer PRIVATE sdl3_deps vulkan_deps imgui)

set(SHADER_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders/")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/shaders")

add_custom_target(07_copy_shaders ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/shaders"
    COMMAND glslangValidator -V -S vert "${CMAKE_CURRENT_SOURCE_DIR}/shaders/shader.vert" -o "${CMAKE_CURRENT_BINARY_DIR}/shaders/vert.spv"
    COMMAND glslangValidator -V -S frag "${CMAKE_CURRENT_SOURCE_DIR}/shaders/shader.frag" -o "${CMAKE_CURRENT_BINARY_DIR}/shaders/frag.spv"
    DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/shader.vert"
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/shader.frag"
    COMMENT "Compiling shaders"
)

add_dependencies(07_uniform_buffer 07_copy_shaders)

target_compile_definitions(07_uniform_buffer PRIVATE SHADER_OUTPUT_DIR="${SHADER_OUTPUT_DIR}")