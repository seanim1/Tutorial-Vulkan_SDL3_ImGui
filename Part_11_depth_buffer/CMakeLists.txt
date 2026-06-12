add_executable(11_depth_buffer main.cpp)
target_link_libraries(11_depth_buffer PRIVATE sdl3_deps vulkan_deps imgui glm_deps stb_deps)

set(SHADER_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders/")
set(ASSET_OUTPUT_DIR  "${CMAKE_CURRENT_BINARY_DIR}/assets/")

add_custom_target(11_copy_shaders ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/shaders"
    COMMAND glslangValidator -V -S vert "${CMAKE_CURRENT_SOURCE_DIR}/shaders/shader.vert" -o "${CMAKE_CURRENT_BINARY_DIR}/shaders/vert.spv"
    COMMAND glslangValidator -V -S frag "${CMAKE_CURRENT_SOURCE_DIR}/shaders/shader.frag" -o "${CMAKE_CURRENT_BINARY_DIR}/shaders/frag.spv"
    DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/shader.vert"
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/shader.frag"
    COMMENT "Compiling shaders"
)

add_custom_target(11_copy_assets ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/assets"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/assets/uv_checker.png"
        "${CMAKE_CURRENT_BINARY_DIR}/assets/uv_checker.png"
    COMMENT "Copying assets"
)

add_dependencies(11_depth_buffer 11_copy_shaders 11_copy_assets)

target_compile_definitions(11_depth_buffer PRIVATE
    SHADER_OUTPUT_DIR="${SHADER_OUTPUT_DIR}"
    ASSET_OUTPUT_DIR="${ASSET_OUTPUT_DIR}"
)