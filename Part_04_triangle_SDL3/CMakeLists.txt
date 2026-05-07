add_executable(04_triangle_SDL3 main.cpp)
target_link_libraries(04_triangle_SDL3 PRIVATE sdl3_deps vulkan_deps)

# Look for glslangValidator - optional
find_program(GLSLANG_VALIDATOR glslangValidator)

if(GLSLANG_VALIDATOR)
    message(STATUS "Found glslangValidator: ${GLSLANG_VALIDATOR}")
    
    set(SHADER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders")
    set(SHADER_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders")
    file(MAKE_DIRECTORY "${SHADER_OUTPUT_DIR}")

    add_custom_command(
        OUTPUT "${SHADER_OUTPUT_DIR}/vert.spv"
        COMMAND ${GLSLANG_VALIDATOR} -V "${SHADER_DIR}/shader.vert" -o "${SHADER_OUTPUT_DIR}/vert.spv"
        DEPENDS "${SHADER_DIR}/shader.vert"
    )

    add_custom_command(
        OUTPUT "${SHADER_OUTPUT_DIR}/frag.spv"
        COMMAND ${GLSLANG_VALIDATOR} -V "${SHADER_DIR}/shader.frag" -o "${SHADER_OUTPUT_DIR}/frag.spv"
        DEPENDS "${SHADER_DIR}/shader.frag"
    )

    add_custom_target(04_shaders DEPENDS "${SHADER_OUTPUT_DIR}/vert.spv" "${SHADER_OUTPUT_DIR}/frag.spv")
    add_dependencies(04_triangle_SDL3 04_shaders)
    target_compile_definitions(04_triangle_SDL3 PRIVATE SHADER_OUTPUT_DIR="${SHADER_OUTPUT_DIR}")
else()
    message(WARNING "glslangValidator not found - shader compilation disabled")
endif()