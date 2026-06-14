add_executable(14_assimp_mesh_loading main.cpp)
target_link_libraries(14_assimp_mesh_loading PRIVATE sdl3_deps vulkan_deps imgui glm_deps stb_deps assimp_deps)

set(SHADER_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders/")
set(ASSET_OUTPUT_DIR  "${CMAKE_CURRENT_BINARY_DIR}/assets/")

add_custom_target(14_copy_shaders ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/shaders"
    COMMAND glslangValidator -V -S vert "${CMAKE_CURRENT_SOURCE_DIR}/shaders/shader.vert" -o "${CMAKE_CURRENT_BINARY_DIR}/shaders/vert.spv"
    COMMAND glslangValidator -V -S frag "${CMAKE_CURRENT_SOURCE_DIR}/shaders/shader.frag" -o "${CMAKE_CURRENT_BINARY_DIR}/shaders/frag.spv"
    COMMAND glslangValidator -V -S frag "${CMAKE_CURRENT_SOURCE_DIR}/shaders/face.frag" -o "${CMAKE_CURRENT_BINARY_DIR}/shaders/face.spv"
    DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/shader.vert"
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/shader.frag"
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/face.frag"
    COMMENT "Compiling shaders"
)

add_custom_target(14_copy_assets ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/assets"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/assets/uv_checker.png"
        "${CMAKE_CURRENT_BINARY_DIR}/assets/uv_checker.png"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/assets/sword_0.glb"
        "${CMAKE_CURRENT_BINARY_DIR}/assets/sword_0.glb"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/assets/sword_0.jpg"
        "${CMAKE_CURRENT_BINARY_DIR}/assets/sword_0.jpg"
    COMMENT "Copying assets"
)

add_dependencies(14_assimp_mesh_loading 14_copy_shaders 14_copy_assets)

target_compile_definitions(14_assimp_mesh_loading PRIVATE
    SHADER_OUTPUT_DIR="${SHADER_OUTPUT_DIR}"
    ASSET_OUTPUT_DIR="${ASSET_OUTPUT_DIR}"
)