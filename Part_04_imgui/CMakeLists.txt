add_executable(04_imgui main.cpp)
target_link_libraries(04_imgui PRIVATE sdl3_deps vulkan_deps imgui)

set(SHADER_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders/")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/shaders")

# Copy pre-compiled shaders
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/vert.spv" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/shaders/")
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/frag.spv" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/shaders/")

target_compile_definitions(04_imgui PRIVATE SHADER_OUTPUT_DIR="${SHADER_OUTPUT_DIR}")