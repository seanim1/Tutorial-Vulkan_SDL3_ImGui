$ErrorActionPreference = "Stop"

$source_dll = "third_party\SDL3\build\Debug\SDL3.dll"

if (-Not (Test-Path $source_dll)) {
    exit 1
}

$targets = @(
    "build\Part_00_init_SDL3\Debug\",
    "build\Part_01_window\Debug\",
    "build\Part_02_vk_instance\Debug\",
    "build\Part_03_minimal_triangle\Debug\",
    "build\Part_04_imgui\Debug\",
    "build\Part_05_vertex_buffer\Debug\",
    "build\Part_06_index_buffer\Debug\",
    "build\Part_07_uniform_buffer\Debug\",
    "build\Part_08_glm\Debug\",
    "build\Part_09_tetrahedron\Debug\",
    "build\Part_10_texture_stb_image\Debug\",
    "build\Part_11_depth_buffer\Debug\",
    "build\Part_12_overlay_quad\Debug\",
    "build\Part_13_blinn_phong\Debug\",
    "build\Part_14_assimp_mesh_loading\Debug\",
    "build\Part_15_XZRenderer_refactor\Debug\"
)

foreach ($target in $targets) {
    if (-Not (Test-Path $target)) {
        mkdir $target -Force | Out-Null
    }
    
    Copy-Item $source_dll "$target\SDL3.dll" -Force
}