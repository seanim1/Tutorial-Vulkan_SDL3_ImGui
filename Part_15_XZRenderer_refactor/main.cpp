// ============================================================
//  Part 15 — XZRenderer engine refactor
//
//  main.cpp knows nothing about Vulkan, SDL, or ImGui.
//  All rendering is driven through XZRenderer.hpp.
// ============================================================

#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_main_impl.h>

#include "XZRenderer.hpp"

// Tetrahedron vertex data — flat-shaded normals baked in
static const XZRenderer::Vertex flat_vertices[] = {
    {{ 0.0f,  1.0f,  0.0f},    {0.5f, 0.0f}, {-0.8321f,  0.2773f,  0.4804f}},
    {{ 0.0f, -1.0f,  1.1547f}, {0.0f, 1.0f}, {-0.8321f,  0.2773f,  0.4804f}},
    {{-1.0f, -1.0f, -0.5774f}, {1.0f, 1.0f}, {-0.8321f,  0.2773f,  0.4804f}},
    {{ 0.0f,  1.0f,  0.0f},    {0.5f, 0.0f}, { 0.0000f,  0.2774f, -0.9608f}},
    {{-1.0f, -1.0f, -0.5774f}, {0.0f, 1.0f}, { 0.0000f,  0.2774f, -0.9608f}},
    {{ 1.0f, -1.0f, -0.5774f}, {1.0f, 1.0f}, { 0.0000f,  0.2774f, -0.9608f}},
    {{ 0.0f,  1.0f,  0.0f},    {0.5f, 0.0f}, { 0.8321f,  0.2773f,  0.4804f}},
    {{ 1.0f, -1.0f, -0.5774f}, {0.0f, 1.0f}, { 0.8321f,  0.2773f,  0.4804f}},
    {{ 0.0f, -1.0f,  1.1547f}, {1.0f, 1.0f}, { 0.8321f,  0.2773f,  0.4804f}},
    {{ 0.0f, -1.0f,  1.1547f}, {0.0f, 0.0f}, { 0.0000f, -1.0000f,  0.0000f}},
    {{ 1.0f, -1.0f, -0.5774f}, {1.0f, 0.0f}, { 0.0000f, -1.0000f,  0.0000f}},
    {{-1.0f, -1.0f, -0.5774f}, {0.5f, 1.0f}, { 0.0000f, -1.0000f,  0.0000f}},
};
static const uint32_t flat_indices[] = { 0,1,2, 3,4,5, 6,7,8, 9,10,11 };

// Overlay quad vertices — baked in tetrahedron local space
static const glm::vec3 quad_positions[] = {
    {-1.1963f, -1.4400f, -0.6427f},
    {-0.0417f, -1.4400f,  1.3574f},
    { 0.5130f,  0.7788f,  1.0371f},
    {-0.6416f,  0.7788f, -0.9629f},
};
static const glm::vec2 quad_uvs[]     = {{0,0},{1,0},{1,1},{0,1}};
static const uint32_t  quad_indices[] = {0,1,2, 0,2,3};

// ============================================================
//  App state — only engine objects, no Vulkan
// ============================================================
struct App {
    XZRenderer::Renderer renderer;

    XZRenderer::MeshObject*       tetra = nullptr;
    XZRenderer::MeshObject*       sword = nullptr;
    XZRenderer::CustomShaderQuad* face  = nullptr;
    XZRenderer::PointLight*       light = nullptr;

    App() : renderer(800, 600, "XZRenderer") {}
};

// ============================================================
//  SDL3 app callbacks
// ============================================================
SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[])
{
    App* app = new App();
    *appstate  = app;

    app->renderer.init();

    // --- Tetrahedron ---
    app->tetra = &app->renderer.createMeshObject(
        std::vector<XZRenderer::Vertex>(flat_vertices, flat_vertices + 12),
        std::vector<uint32_t>(flat_indices, flat_indices + 12),
        ASSET_OUTPUT_DIR "uv_checker.png"
    );
    app->tetra->setPosition(-2.0f, 0.0f, 0.0f);

    app->sword = &app->renderer.createMeshObject(
        ASSET_OUTPUT_DIR "sword_0.glb",
        ASSET_OUTPUT_DIR "sword_0.jpg"
    );
    app->sword->setPosition(2.0f, 0.0f, 0.0f);
    app->sword->setRotation(90.0f, 0.0f, 0.0f);
    app->sword->setScale(0.3f);

    // --- Face overlay quad ---
    app->face = &app->renderer.createCustomShaderQuad(SHADER_OUTPUT_DIR "face.spv");
    app->face->setPosition(-0.4f, 0.4f, -1.0f);
    app->face->setRotation(0.0f, 0.0f, 180.0f);
    app->face->setScale(3.0f);

    // --- Light ---
    app->light = &app->renderer.createPointLight();
    app->light->setPosition(2.0f, 2.0f, -2.0f);

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event)
{
    auto* app = static_cast<App*>(appstate);
    if (app->renderer.handleEvent(event)) return SDL_APP_SUCCESS;
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void* appstate)
{
    auto* app = static_cast<App*>(appstate);
    XZRenderer::Renderer& r = app->renderer;

    if (!r.beginFrame()) return SDL_APP_SUCCESS;

    auto& gui = r.getGui();
    gui.beginWindow("Scene");
    gui.text("Press T to toggle GUI");
    gui.exposeCamera("Camera");
    gui.separator();

    gui.exposeTransformation(*app->tetra, "Tetrahedron");
    gui.separator();

    gui.exposeTransformation(*app->face, "Face");
    gui.separator();

    gui.exposeTransformation(*app->sword, "Sword");
    gui.separator();

    gui.exposeLight(*app->light, "Light");
    gui.separator();

    gui.exposeClearColor("Background");
    gui.showFPS();
    gui.endWindow();

    r.endFrame();
    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* appstate, SDL_AppResult result)
{
    auto* app = static_cast<App*>(appstate);
    delete app;
}