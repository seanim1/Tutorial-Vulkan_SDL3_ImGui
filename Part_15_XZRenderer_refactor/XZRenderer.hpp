#pragma once

// ============================================================
//  XZRenderer — Vulkan + SDL3 + ImGui rendering module
//  Namespace : XZRenderer
//
//  User code only needs to #include "XZRenderer.hpp".
//  No Vulkan, SDL, ImGui or Assimp headers leak into this file.
// ============================================================

#include <string>
#include <vector>
#include <memory>
#include <glm/glm.hpp>

namespace XZRenderer {

// ----- Forward declarations of opaque implementation types -----
struct RendererImpl;
struct MeshObjectImpl;
struct CustomShaderQuadImpl;

// ---------------------------------------------------------------
//  Vertex  —  public vertex format for loadFromVertices()
// ---------------------------------------------------------------
struct Vertex {
    glm::vec3 position;
    glm::vec2 uv;
    glm::vec3 normal;
};

// ---------------------------------------------------------------
//  PointLight
// ---------------------------------------------------------------
class PointLight {
public:
    void setPosition(float x, float y, float z);
    void setPosition(const glm::vec3& pos);
    const glm::vec3& getPosition() const { return position_; }

private:
    glm::vec3 position_ = {2.0f, 2.0f, -2.0f};
};

// ---------------------------------------------------------------
//  MeshObject  —  a textured 3-D object with Blinn-Phong lighting.
//
//  Two construction paths:
//    obj.loadFromGLTF("mesh.glb", "texture.jpg")
//    obj.loadFromVertices(verts, indices, "texture.png")
// ---------------------------------------------------------------
class MeshObject {
public:
    // --- Loading ---
    void loadFromGLTF(const std::string& mesh_path,
                      const std::string& texture_path);

    void loadFromVertices(const std::vector<Vertex>&   vertices,
                          const std::vector<uint32_t>& indices,
                          const std::string&           texture_path);

    // --- Transform ---
    void setPosition(float x, float y, float z);
    void setPosition(const glm::vec3& pos);

    void setRotation(float x, float y, float z);   // degrees
    void setRotation(const glm::vec3& degrees);

    void setScale(float uniform_scale);
    void setScale(float x, float y, float z);
    void setScale(const glm::vec3& scale);

    // --- Shading ---
    // Only meaningful when loaded from raw vertices with pre-computed flat normals.
    // For glTF objects normals come from Assimp; this flag is ignored.
    void enableFlatShading(bool flat);
    bool isFlatShading() const { return flat_shading_; }

    // --- Accessors (read-only) ---
    const glm::vec3& getPosition() const { return position_; }
    const glm::vec3& getRotation() const { return rotation_; }
    const glm::vec3& getScale()    const { return scale_; }

    // Internal — do not call from user code
    MeshObjectImpl* impl() const { return impl_.get(); }

private:
    friend class Renderer;
    MeshObject() = default;

    glm::vec3 position_     = {0.0f, 0.0f, 0.0f};
    glm::vec3 rotation_     = {0.0f, 0.0f, 0.0f};  // degrees
    glm::vec3 scale_        = {1.0f, 1.0f, 1.0f};
    bool      flat_shading_ = false;

    std::unique_ptr<MeshObjectImpl> impl_;
};

// ---------------------------------------------------------------
//  CustomShaderQuad  —  a flat quad rendered with a user-supplied
//  compiled fragment shader (.spv).  Used for procedural overlays.
//  The shared vertex shader (vert.spv) is used automatically.
//
//  Usage:
//    auto& face = renderer.createCustomShaderQuad("face.spv");
//    face.setVertices(positions, uvs, indices);
//    face.setPosition(...);
// ---------------------------------------------------------------
class CustomShaderQuad {
public:
    // Supply baked quad geometry.
    // positions and uvs must have the same length.
    void setVertices(const std::vector<glm::vec3>& positions,
                     const std::vector<glm::vec2>& uvs,
                     const std::vector<uint32_t>&  indices);

    // --- Transform ---
    void setPosition(float x, float y, float z);
    void setPosition(const glm::vec3& pos);

    void setRotation(float x, float y, float z);
    void setRotation(const glm::vec3& degrees);

    void setScale(float x, float y, float z);
    void setScale(const glm::vec3& scale);

    // --- Accessors ---
    const glm::vec3& getPosition() const { return position_; }
    const glm::vec3& getRotation() const { return rotation_; }
    const glm::vec3& getScale()    const { return scale_; }

    // Internal
    CustomShaderQuadImpl* impl() const { return impl_.get(); }

private:
    friend class Renderer;
    explicit CustomShaderQuad(const std::string& frag_spv_path);

    std::string frag_spv_path_;
    glm::vec3   position_ = {0.0f, 0.0f, 0.0f};
    glm::vec3   rotation_ = {0.0f, 0.0f, 0.0f};
    glm::vec3   scale_    = {1.0f, 1.0f, 1.0f};

    std::unique_ptr<CustomShaderQuadImpl> impl_;
};

// ---------------------------------------------------------------
//  ImGuiLayer  —  exposes engine objects as ImGui widgets.
//  Obtain via Renderer::getGui().
//  All calls must be placed between beginFrame() and endFrame().
// ---------------------------------------------------------------
class ImGuiLayer {
public:
    // RGBA color picker that controls the renderer clear color
    void exposeClearColor(const std::string& label);

    // Position / Rotation / Scale sliders
    void exposeTransformation(MeshObject&       obj,  const std::string& label);
    void exposeTransformation(CustomShaderQuad& quad, const std::string& label);

    // Position-only sliders (scale/rotation irrelevant for lights)
    void exposeLight(PointLight& light, const std::string& label);

    // Checkbox for flat / smooth shading toggle
    void exposeFlatShading(MeshObject& obj, const std::string& label);

    // Utility
    void text(const std::string& str);
    void showFPS();

    // Internal
    RendererImpl* renderer_impl_ = nullptr;

private:
    friend class Renderer;
    ImGuiLayer() = default;
};

// ---------------------------------------------------------------
//  Renderer  —  top-level engine entry point.
//  One instance per application.
// ---------------------------------------------------------------
class Renderer {
public:
    Renderer();
    ~Renderer();

    // Non-copyable, non-movable
    Renderer(const Renderer&)            = delete;
    Renderer& operator=(const Renderer&) = delete;
    Renderer(Renderer&&)                 = delete;
    Renderer& operator=(Renderer&&)      = delete;

    // ---- Configuration  (call BEFORE init()) ----
    void setWindowSize(uint32_t width, uint32_t height);
    void setWindowTitle(const std::string& title);
    void setClearColor(float r, float g, float b, float a = 1.0f);
    void setCameraPosition(float x, float y, float z);

    // Enable / disable internal engine logging (off by default)
    void enableLogging(bool enable);

    // ---- Initialisation  (call ONCE) ----
    void init();

    // ---- Scene objects  (call AFTER init()) ----
    // The Renderer owns every object.  Returns a stable reference.
    MeshObject&       createMeshObject();
    CustomShaderQuad& createCustomShaderQuad(const std::string& frag_spv_path);
    PointLight&       createPointLight();

    // ---- ImGui ----
    ImGuiLayer& getGui();

    // ---- Frame loop ----
    // Returns false when the user closes the window.
    bool beginFrame();   // poll events, acquire image, update UBOs, begin ImGui frame
    void endFrame();     // render ImGui, submit, present

private:
    std::unique_ptr<RendererImpl>                  impl_;
    std::vector<std::unique_ptr<MeshObject>>       mesh_objects_;
    std::vector<std::unique_ptr<CustomShaderQuad>> quads_;
    std::vector<std::unique_ptr<PointLight>>       lights_;
    std::unique_ptr<ImGuiLayer>                    gui_;

    uint32_t    window_width_   = 800;
    uint32_t    window_height_  = 600;
    std::string window_title_   = "XZRenderer";
    float       clear_color_[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    bool        logging_enabled_ = false;
    glm::vec3   camera_pos_     = {0.0f, 0.0f, -6.0f};
};

} // namespace XZRenderer