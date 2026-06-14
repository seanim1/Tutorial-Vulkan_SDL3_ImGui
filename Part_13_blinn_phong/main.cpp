#define SDL_MAIN_USE_CALLBACKS 1

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_main_impl.h>
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>
#include <memory>
#include <vector>
#include <cstring>

#ifdef __APPLE__
    #define VK_ENABLE_BETA_EXTENSIONS
    #include <vulkan/vulkan_metal.h>
    #include <vulkan/vulkan_beta.h>
#endif

// --- ImGui ---
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_vulkan.h"
// -------------

// --- glm ---
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
// -----------

// --- stb_image ---
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
// -----------------

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600

#define ENGINE_xzlog(vk_call) \
    SDL_Log("ENGINE_xz: %s [%s:%d]", vk_call, __FILE__, __LINE__)
#define ENGINE_xziter_log(vk_call) \
    SDL_Log("ENGINE_xz.iter: %s [%s:%d]", vk_call, __FILE__, __LINE__)

// --- Depth format ---
static const VkFormat DEPTH_FORMAT = VK_FORMAT_D32_SFLOAT;
// --------------------

// --- Vertex ---
struct Vertex {
    glm::vec3 position;
    glm::vec2 uv;
    glm::vec3 normal;
};

// Flat-shaded: 12 unique vertices (4 faces x 3 verts), each with the face normal
// Face normals computed from cross products, oriented outward
const Vertex vertices[] = {
    // left face  (0,1,2) — normal (-0.8321, 0.2773, 0.4804)
    {{ 0.0f,  1.0f,  0.0f},    {0.5f, 0.0f}, {-0.8321f,  0.2773f,  0.4804f}},
    {{ 0.0f, -1.0f,  1.1547f}, {0.0f, 1.0f}, {-0.8321f,  0.2773f,  0.4804f}},
    {{-1.0f, -1.0f, -0.5774f}, {1.0f, 1.0f}, {-0.8321f,  0.2773f,  0.4804f}},
    // back face  (0,2,3) — normal (0.0000, 0.2774, -0.9608)
    {{ 0.0f,  1.0f,  0.0f},    {0.5f, 0.0f}, { 0.0000f,  0.2774f, -0.9608f}},
    {{-1.0f, -1.0f, -0.5774f}, {0.0f, 1.0f}, { 0.0000f,  0.2774f, -0.9608f}},
    {{ 1.0f, -1.0f, -0.5774f}, {1.0f, 1.0f}, { 0.0000f,  0.2774f, -0.9608f}},
    // right face (0,3,1) — normal (0.8321, 0.2773, 0.4804)
    {{ 0.0f,  1.0f,  0.0f},    {0.5f, 0.0f}, { 0.8321f,  0.2773f,  0.4804f}},
    {{ 1.0f, -1.0f, -0.5774f}, {0.0f, 1.0f}, { 0.8321f,  0.2773f,  0.4804f}},
    {{ 0.0f, -1.0f,  1.1547f}, {1.0f, 1.0f}, { 0.8321f,  0.2773f,  0.4804f}},
    // bottom face(1,3,2) — normal (0.0000, -1.0000, 0.0000)
    {{ 0.0f, -1.0f,  1.1547f}, {0.0f, 0.0f}, { 0.0000f, -1.0000f,  0.0000f}},
    {{ 1.0f, -1.0f, -0.5774f}, {1.0f, 0.0f}, { 0.0000f, -1.0000f,  0.0000f}},
    {{-1.0f, -1.0f, -0.5774f}, {0.5f, 1.0f}, { 0.0000f, -1.0000f,  0.0000f}},
};

const uint32_t indices[] = {
    0,  1,  2,   // left face
    3,  4,  5,   // back face
    6,  7,  8,   // right face
    9,  10, 11,  // bottom face
};

// Smooth-shaded: 4 shared vertices with averaged normals
const Vertex smooth_vertices[] = {
    {{ 0.0f,  1.0f,  0.0f},    {0.5f, 0.0f}, { 0.0000f,  1.0000f,  0.0000f}},  // top
    {{ 0.0f, -1.0f,  1.1547f}, {0.0f, 1.0f}, { 0.0000f, -0.4205f,  0.9073f}},  // front
    {{-1.0f, -1.0f, -0.5774f}, {1.0f, 1.0f}, {-0.7857f, -0.4205f, -0.4536f}},  // back left
    {{ 1.0f, -1.0f, -0.5774f}, {0.5f, 0.5f}, { 0.7857f, -0.4205f, -0.4536f}},  // back right
};

const uint32_t smooth_indices[] = {
    0, 1, 2,  // left face
    0, 2, 3,  // back face
    0, 3, 1,  // right face
    1, 3, 2,  // bottom face
};
// --------------

// --- Overlay quad on front face (0,1,2), offset 0.01 along outward normal ---
struct QuadVertex {
    glm::vec3 position;
    glm::vec2 uv;
};

const QuadVertex quad_vertices[] = {
    {{-1.1963f, -1.4400f, -0.6427f}, {0.0f, 0.0f}},
    {{-0.0417f, -1.4400f,  1.3574f}, {1.0f, 0.0f}},
    {{ 0.5130f,  0.7788f,  1.0371f}, {1.0f, 1.0f}},
    {{-0.6416f,  0.7788f, -0.9629f}, {0.0f, 1.0f}},
};

const uint32_t quad_indices[] = {
    0, 1, 2,
    0, 2, 3,
};
// --------------

// --- Uniform ---
struct UniformData {
    glm::mat4 mvp;
    glm::mat4 model;
    glm::vec4 light_pos;  // vec4 for alignment
    glm::vec4 cam_pos;    // vec4 for alignment
};
// ---------------

struct AppState {
    SDL_Window* window = nullptr;
    VkInstance instance = nullptr;
    VkSurfaceKHR surface = nullptr;
    VkPhysicalDevice physical_device = nullptr;
    VkDevice device = nullptr;
    VkQueue graphics_queue = nullptr;
    VkSwapchainKHR swapchain = nullptr;
    std::vector<VkImage> images;
    std::vector<VkImageView> image_views;
    VkPipeline pipeline = nullptr;
    VkPipelineLayout pipeline_layout = nullptr;
    VkCommandPool command_pool = nullptr;
    std::vector<VkCommandBuffer> command_buffers;
    std::vector<VkCommandBuffer> imgui_command_buffers;
    VkSemaphore image_available_sem = nullptr;
    VkSemaphore render_finished_sem = nullptr;
    VkFence in_flight_fence = nullptr;
    uint32_t image_count = 0;
    uint32_t graphics_family = 0;
    VkExtent2D swapchain_extent = {};
    VkSurfaceFormatKHR surface_format = {};

    // --- Vertex buffers (flat + smooth) ---
    VkBuffer       vertex_buffer_flat   = nullptr;
    VkDeviceMemory vertex_memory_flat   = nullptr;
    VkBuffer       index_buffer_flat    = nullptr;
    VkDeviceMemory index_memory_flat    = nullptr;
    VkBuffer       vertex_buffer_smooth = nullptr;
    VkDeviceMemory vertex_memory_smooth = nullptr;
    VkBuffer       index_buffer_smooth  = nullptr;
    VkDeviceMemory index_memory_smooth  = nullptr;

    // --- Uniform buffer ---
    std::vector<VkBuffer>        uniform_buffers;
    std::vector<VkDeviceMemory>  uniform_buffers_memory;
    std::vector<void*>           uniform_buffers_mapped;
    VkDescriptorSetLayout        descriptor_set_layout = nullptr;
    VkDescriptorPool             descriptor_pool = nullptr;
    std::vector<VkDescriptorSet> descriptor_sets;

    // --- Texture ---
    VkImage        texture_image        = nullptr;
    VkDeviceMemory texture_image_memory = nullptr;
    VkImageView    texture_image_view   = nullptr;
    VkSampler      texture_sampler      = nullptr;

    // --- Depth buffer ---
    VkImage        depth_image        = nullptr;
    VkDeviceMemory depth_image_memory = nullptr;
    VkImageView    depth_image_view   = nullptr;

    // --- Face overlay quad ---
    VkPipeline                   face_pipeline              = nullptr;
    VkPipelineLayout             face_pipeline_layout       = nullptr;
    VkDescriptorSetLayout        face_descriptor_set_layout = nullptr;
    VkDescriptorPool             face_descriptor_pool       = nullptr;
    std::vector<VkDescriptorSet> face_descriptor_sets;
    VkBuffer                     quad_vertex_buffer         = nullptr;
    VkDeviceMemory               quad_vertex_memory         = nullptr;
    VkBuffer                     quad_index_buffer          = nullptr;
    VkDeviceMemory               quad_index_memory          = nullptr;
    std::vector<VkBuffer>        quad_uniform_buffers;
    std::vector<VkDeviceMemory>  quad_uniform_memory;
    std::vector<void*>           quad_uniform_mapped;
    // -------------------------

    // --- ImGui ---
    VkDescriptorPool imgui_descriptor_pool = nullptr;
    bool show_gui    = true;
    bool flat_shade  = true;
    float rotation_x = 0.0f;
    float rotation_y = -147.0f;
    float rotation_z = 0.0f;
    float last_time  = 0.0f;
    glm::vec3 light_pos = {2.0f, 2.0f, -2.0f};
    // -------------
};

void check_vk(VkResult result, const char* op) {
    if (result != VK_SUCCESS) {
        SDL_Log("Vulkan error (%s): %d", op, result);
        exit(1);
    }
}

uint32_t* load_shader_file(const char* filename, size_t* out_size) {
    FILE* f = fopen(filename, "rb");
    if (!f) {
        SDL_Log("Failed to open shader: %s", filename);
        return nullptr;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint32_t* data = (uint32_t*)malloc(size);
    fread(data, 1, size, f);
    fclose(f);
    *out_size = size;
    return data;
}

uint32_t find_memory_type(VkPhysicalDevice physical_device,
                          uint32_t type_filter,
                          VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties mem_props;
    vkGetPhysicalDeviceMemoryProperties(physical_device, &mem_props);
    for (uint32_t i = 0; i < mem_props.memoryTypeCount; i++) {
        if ((type_filter & (1 << i)) &&
            (mem_props.memoryTypes[i].propertyFlags & properties) == properties)
            return i;
    }
    SDL_Log("Failed to find suitable memory type");
    exit(1);
}

void create_buffer(AppState* app, VkDeviceSize size, VkBufferUsageFlags usage,
                   VkMemoryPropertyFlags props, VkBuffer& buf, VkDeviceMemory& mem)
{
    VkBufferCreateInfo info = {
        .sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size        = size,
        .usage       = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    ENGINE_xzlog("vkCreateBuffer");
    check_vk(vkCreateBuffer(app->device, &info, nullptr, &buf), "create_buffer");
    VkMemoryRequirements reqs;
    vkGetBufferMemoryRequirements(app->device, buf, &reqs);
    VkMemoryAllocateInfo alloc = {
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize  = reqs.size,
        .memoryTypeIndex = find_memory_type(app->physical_device, reqs.memoryTypeBits, props),
    };
    ENGINE_xzlog("vkAllocateMemory");
    check_vk(vkAllocateMemory(app->device, &alloc, nullptr, &mem), "alloc_buffer");
    vkBindBufferMemory(app->device, buf, mem, 0);
}

void upload_buffer(AppState* app, VkBuffer buf, VkDeviceMemory mem,
                   const void* data, VkDeviceSize size)
{
    void* mapped;
    vkMapMemory(app->device, mem, 0, size, 0, &mapped);
    memcpy(mapped, data, size);
    vkUnmapMemory(app->device, mem);
}

void record_imgui_command_buffers(AppState* app, uint32_t i)
{
    VkCommandBufferBeginInfo begin = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,
    };
    check_vk(vkBeginCommandBuffer(app->imgui_command_buffers[i], &begin), "imgui begin");

    VkRenderingAttachmentInfo color_attachment = {
        .sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView   = app->image_views[i],
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp      = VK_ATTACHMENT_LOAD_OP_LOAD,
        .storeOp     = VK_ATTACHMENT_STORE_OP_STORE,
    };
    VkRenderingInfo rendering_info = {
        .sType                = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea           = {{0, 0}, app->swapchain_extent},
        .layerCount           = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments    = &color_attachment,
    };
    vkCmdBeginRendering(app->imgui_command_buffers[i], &rendering_info);
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), app->imgui_command_buffers[i]);
    vkCmdEndRendering(app->imgui_command_buffers[i]);

    VkImageMemoryBarrier barrier_to_present = {
        .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask       = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dstAccessMask       = 0,
        .oldLayout           = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .newLayout           = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image               = app->images[i],
        .subresourceRange    = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
    };
    vkCmdPipelineBarrier(app->imgui_command_buffers[i],
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        0, 0, nullptr, 0, nullptr, 1, &barrier_to_present);

    check_vk(vkEndCommandBuffer(app->imgui_command_buffers[i]), "imgui end");
}

void create_tetra_buffers(AppState* app)
{
    // --- Flat ---
    ENGINE_xzlog("create_tetra_buffers (flat)");
    create_buffer(app, sizeof(vertices),
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        app->vertex_buffer_flat, app->vertex_memory_flat);
    upload_buffer(app, app->vertex_buffer_flat, app->vertex_memory_flat, vertices, sizeof(vertices));

    create_buffer(app, sizeof(indices),
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        app->index_buffer_flat, app->index_memory_flat);
    upload_buffer(app, app->index_buffer_flat, app->index_memory_flat, indices, sizeof(indices));

    // --- Smooth ---
    ENGINE_xzlog("create_tetra_buffers (smooth)");
    create_buffer(app, sizeof(smooth_vertices),
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        app->vertex_buffer_smooth, app->vertex_memory_smooth);
    upload_buffer(app, app->vertex_buffer_smooth, app->vertex_memory_smooth, smooth_vertices, sizeof(smooth_vertices));

    create_buffer(app, sizeof(smooth_indices),
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        app->index_buffer_smooth, app->index_memory_smooth);
    upload_buffer(app, app->index_buffer_smooth, app->index_memory_smooth, smooth_indices, sizeof(smooth_indices));
}

void create_quad_buffers(AppState* app)
{
    ENGINE_xzlog("create_quad_buffers");
    create_buffer(app, sizeof(quad_vertices),
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        app->quad_vertex_buffer, app->quad_vertex_memory);
    upload_buffer(app, app->quad_vertex_buffer, app->quad_vertex_memory, quad_vertices, sizeof(quad_vertices));

    create_buffer(app, sizeof(quad_indices),
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        app->quad_index_buffer, app->quad_index_memory);
    upload_buffer(app, app->quad_index_buffer, app->quad_index_memory, quad_indices, sizeof(quad_indices));
}

void create_uniform_buffers(AppState* app)
{
    app->uniform_buffers.resize(app->image_count);
    app->uniform_buffers_memory.resize(app->image_count);
    app->uniform_buffers_mapped.resize(app->image_count);

    for (uint32_t i = 0; i < app->image_count; i++) {
        ENGINE_xzlog("create_buffer (uniform)");
        create_buffer(app, sizeof(UniformData),
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            app->uniform_buffers[i], app->uniform_buffers_memory[i]);
        vkMapMemory(app->device, app->uniform_buffers_memory[i], 0, sizeof(UniformData), 0,
                    &app->uniform_buffers_mapped[i]);
    }

    app->quad_uniform_buffers.resize(app->image_count);
    app->quad_uniform_memory.resize(app->image_count);
    app->quad_uniform_mapped.resize(app->image_count);

    for (uint32_t i = 0; i < app->image_count; i++) {
        ENGINE_xzlog("create_buffer (quad uniform)");
        create_buffer(app, sizeof(UniformData),
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            app->quad_uniform_buffers[i], app->quad_uniform_memory[i]);
        vkMapMemory(app->device, app->quad_uniform_memory[i], 0, sizeof(UniformData), 0,
                    &app->quad_uniform_mapped[i]);
    }
}

void transition_image_layout(VkCommandBuffer cmd, VkImage image,
                              VkImageLayout old_layout, VkImageLayout new_layout)
{
    VkImageMemoryBarrier barrier = {
        .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout           = old_layout,
        .newLayout           = new_layout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image               = image,
        .subresourceRange    = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
    };
    VkPipelineStageFlags src_stage, dst_stage;
    if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED &&
        new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    vkCmdPipelineBarrier(cmd, src_stage, dst_stage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

void create_texture(AppState* app)
{
    int width, height, channels;
    stbi_uc* pixels = stbi_load(ASSET_OUTPUT_DIR "uv_checker.png",
                                 &width, &height, &channels, STBI_rgb_alpha);
    if (!pixels) { SDL_Log("Failed to load texture: %s", stbi_failure_reason()); exit(1); }
    VkDeviceSize image_size = width * height * 4;
    SDL_Log("Loaded texture: %dx%d", width, height);

    VkBuffer staging_buffer; VkDeviceMemory staging_memory;
    ENGINE_xzlog("create_buffer (texture staging)");
    create_buffer(app, image_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        staging_buffer, staging_memory);
    upload_buffer(app, staging_buffer, staging_memory, pixels, image_size);
    stbi_image_free(pixels);

    VkImageCreateInfo image_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, .imageType = VK_IMAGE_TYPE_2D,
        .format = VK_FORMAT_R8G8B8A8_SRGB,
        .extent = {(uint32_t)width, (uint32_t)height, 1},
        .mipLevels = 1, .arrayLayers = 1, .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE, .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    ENGINE_xzlog("vkCreateImage (texture)");
    check_vk(vkCreateImage(app->device, &image_info, nullptr, &app->texture_image), "vkCreateImage");

    VkMemoryRequirements mem_reqs;
    vkGetImageMemoryRequirements(app->device, app->texture_image, &mem_reqs);
    VkMemoryAllocateInfo img_alloc = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, .allocationSize = mem_reqs.size,
        .memoryTypeIndex = find_memory_type(app->physical_device, mem_reqs.memoryTypeBits,
                               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
    };
    ENGINE_xzlog("vkAllocateMemory (texture)");
    check_vk(vkAllocateMemory(app->device, &img_alloc, nullptr, &app->texture_image_memory), "img mem");
    vkBindImageMemory(app->device, app->texture_image, app->texture_image_memory, 0);

    VkCommandBufferAllocateInfo cmd_alloc = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = app->command_pool, .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    VkCommandBuffer cmd;
    ENGINE_xzlog("vkAllocateCommandBuffers (texture upload)");
    vkAllocateCommandBuffers(app->device, &cmd_alloc, &cmd);
    VkCommandBufferBeginInfo begin = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    vkBeginCommandBuffer(cmd, &begin);
    transition_image_layout(cmd, app->texture_image,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    VkBufferImageCopy copy = {
        .bufferOffset = 0, .bufferRowLength = 0, .bufferImageHeight = 0,
        .imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
        .imageOffset = {0, 0, 0}, .imageExtent = {(uint32_t)width, (uint32_t)height, 1},
    };
    vkCmdCopyBufferToImage(cmd, staging_buffer, app->texture_image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
    transition_image_layout(cmd, app->texture_image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    vkEndCommandBuffer(cmd);
    VkSubmitInfo submit = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, .commandBufferCount = 1, .pCommandBuffers = &cmd,
    };
    ENGINE_xzlog("vkQueueSubmit (texture upload)");
    vkQueueSubmit(app->graphics_queue, 1, &submit, VK_NULL_HANDLE);
    vkQueueWaitIdle(app->graphics_queue);
    vkFreeCommandBuffers(app->device, app->command_pool, 1, &cmd);
    vkDestroyBuffer(app->device, staging_buffer, nullptr);
    vkFreeMemory(app->device, staging_memory, nullptr);

    VkImageViewCreateInfo view_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, .image = app->texture_image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D, .format = VK_FORMAT_R8G8B8A8_SRGB,
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
    };
    ENGINE_xzlog("vkCreateImageView (texture)");
    check_vk(vkCreateImageView(app->device, &view_info, nullptr, &app->texture_image_view), "tex view");

    VkSamplerCreateInfo sampler_info = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_LINEAR, .minFilter = VK_FILTER_LINEAR,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .minLod = 0.0f, .maxLod = 0.0f,
    };
    ENGINE_xzlog("vkCreateSampler");
    check_vk(vkCreateSampler(app->device, &sampler_info, nullptr, &app->texture_sampler), "sampler");
}

void create_depth_buffer(AppState* app)
{
    VkImageCreateInfo image_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, .imageType = VK_IMAGE_TYPE_2D,
        .format = DEPTH_FORMAT,
        .extent = {app->swapchain_extent.width, app->swapchain_extent.height, 1},
        .mipLevels = 1, .arrayLayers = 1, .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE, .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    ENGINE_xzlog("vkCreateImage (depth)");
    check_vk(vkCreateImage(app->device, &image_info, nullptr, &app->depth_image), "depth image");

    VkMemoryRequirements mem_reqs;
    vkGetImageMemoryRequirements(app->device, app->depth_image, &mem_reqs);
    VkMemoryAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, .allocationSize = mem_reqs.size,
        .memoryTypeIndex = find_memory_type(app->physical_device, mem_reqs.memoryTypeBits,
                               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
    };
    ENGINE_xzlog("vkAllocateMemory (depth)");
    check_vk(vkAllocateMemory(app->device, &alloc_info, nullptr, &app->depth_image_memory), "depth mem");
    vkBindImageMemory(app->device, app->depth_image, app->depth_image_memory, 0);

    VkImageViewCreateInfo view_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, .image = app->depth_image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D, .format = DEPTH_FORMAT,
        .subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1},
    };
    ENGINE_xzlog("vkCreateImageView (depth)");
    check_vk(vkCreateImageView(app->device, &view_info, nullptr, &app->depth_image_view), "depth view");
}

void create_descriptors(AppState* app)
{
    // --- Tetrahedron: UBO + sampler ---
    VkDescriptorSetLayoutBinding ubo_binding = {
        .binding = 0, .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
    };
    VkDescriptorSetLayoutBinding sampler_binding = {
        .binding = 1, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
    };
    VkDescriptorSetLayoutBinding bindings[] = {ubo_binding, sampler_binding};
    VkDescriptorSetLayoutCreateInfo layout_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 2, .pBindings = bindings,
    };
    ENGINE_xzlog("vkCreateDescriptorSetLayout (tetra)");
    check_vk(vkCreateDescriptorSetLayout(app->device, &layout_info, nullptr, &app->descriptor_set_layout), "tetra layout");

    VkDescriptorPoolSize pool_sizes[] = {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         app->image_count},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, app->image_count},
    };
    VkDescriptorPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = app->image_count, .poolSizeCount = 2, .pPoolSizes = pool_sizes,
    };
    ENGINE_xzlog("vkCreateDescriptorPool (tetra)");
    check_vk(vkCreateDescriptorPool(app->device, &pool_info, nullptr, &app->descriptor_pool), "tetra pool");

    std::vector<VkDescriptorSetLayout> layouts(app->image_count, app->descriptor_set_layout);
    VkDescriptorSetAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = app->descriptor_pool,
        .descriptorSetCount = app->image_count, .pSetLayouts = layouts.data(),
    };
    app->descriptor_sets.resize(app->image_count);
    ENGINE_xzlog("vkAllocateDescriptorSets (tetra)");
    check_vk(vkAllocateDescriptorSets(app->device, &alloc_info, app->descriptor_sets.data()), "tetra desc sets");

    for (uint32_t i = 0; i < app->image_count; i++) {
        VkDescriptorBufferInfo buf_info = {
            .buffer = app->uniform_buffers[i], .offset = 0, .range = sizeof(UniformData),
        };
        VkDescriptorImageInfo img_info = {
            .sampler = app->texture_sampler, .imageView = app->texture_image_view,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        };
        VkWriteDescriptorSet writes[2] = {
            {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
             .dstSet = app->descriptor_sets[i], .dstBinding = 0,
             .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
             .pBufferInfo = &buf_info},
            {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
             .dstSet = app->descriptor_sets[i], .dstBinding = 1,
             .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
             .pImageInfo = &img_info},
        };
        vkUpdateDescriptorSets(app->device, 2, writes, 0, nullptr);
    }

    // --- Face quad: UBO only ---
    // Note: face quad UBO only needs mvp so we reuse the same layout binding
    // but stageFlags only vertex for the face quad's simpler UBO needs
    VkDescriptorSetLayoutBinding face_ubo_binding = {
        .binding = 0, .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
    };
    VkDescriptorSetLayoutCreateInfo face_layout_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1, .pBindings = &face_ubo_binding,
    };
    ENGINE_xzlog("vkCreateDescriptorSetLayout (face)");
    check_vk(vkCreateDescriptorSetLayout(app->device, &face_layout_info, nullptr, &app->face_descriptor_set_layout), "face layout");

    VkDescriptorPoolSize face_pool_size = {
        .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = app->image_count,
    };
    VkDescriptorPoolCreateInfo face_pool_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = app->image_count, .poolSizeCount = 1, .pPoolSizes = &face_pool_size,
    };
    ENGINE_xzlog("vkCreateDescriptorPool (face)");
    check_vk(vkCreateDescriptorPool(app->device, &face_pool_info, nullptr, &app->face_descriptor_pool), "face pool");

    std::vector<VkDescriptorSetLayout> face_layouts(app->image_count, app->face_descriptor_set_layout);
    VkDescriptorSetAllocateInfo face_alloc = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = app->face_descriptor_pool,
        .descriptorSetCount = app->image_count, .pSetLayouts = face_layouts.data(),
    };
    app->face_descriptor_sets.resize(app->image_count);
    ENGINE_xzlog("vkAllocateDescriptorSets (face)");
    check_vk(vkAllocateDescriptorSets(app->device, &face_alloc, app->face_descriptor_sets.data()), "face desc sets");

    for (uint32_t i = 0; i < app->image_count; i++) {
        VkDescriptorBufferInfo buf_info = {
            .buffer = app->quad_uniform_buffers[i], .offset = 0, .range = sizeof(UniformData),
        };
        VkWriteDescriptorSet write = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = app->face_descriptor_sets[i], .dstBinding = 0,
            .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pBufferInfo = &buf_info,
        };
        vkUpdateDescriptorSets(app->device, 1, &write, 0, nullptr);
    }
}

void update_uniform_buffer(AppState* app, uint32_t image_index)
{
    float current_time = (float)SDL_GetTicks() / 1000.0f;
    app->last_time = current_time;

    glm::mat4 model = glm::mat4(1.0f);
    model = glm::rotate(model, glm::radians(app->rotation_x), glm::vec3(1.0f, 0.0f, 0.0f));
    model = glm::rotate(model, glm::radians(app->rotation_y), glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::rotate(model, glm::radians(app->rotation_z), glm::vec3(0.0f, 0.0f, 1.0f));

    glm::mat4 view = glm::lookAt(
        glm::vec3(0.0f, 0.0f, -4.0f),
        glm::vec3(0.0f, 0.0f,  0.0f),
        glm::vec3(0.0f, 1.0f,  0.0f)
    );

    glm::mat4 proj = glm::perspective(
        glm::radians(45.0f),
        (float)app->swapchain_extent.width / (float)app->swapchain_extent.height,
        0.1f, 100.0f
    );
    proj[1][1] *= -1;

    UniformData ubo = {};
    ubo.mvp       = proj * view * model;
    ubo.model     = model;
    ubo.light_pos = glm::vec4(app->light_pos, 1.0f);
    ubo.cam_pos   = glm::vec4(0.0f, 0.0f, -4.0f, 1.0f);

    memcpy(app->uniform_buffers_mapped[image_index], &ubo, sizeof(ubo));

    // Face quad uses same MVP, lighting not applied there
    memcpy(app->quad_uniform_mapped[image_index], &ubo, sizeof(ubo));
}

// --- record_scene_command_buffer: called each frame so we can swap vertex buffers ---
void record_scene_command_buffers(AppState* app)
{
    VkBuffer  vbuf  = app->flat_shade ? app->vertex_buffer_flat  : app->vertex_buffer_smooth;
    VkBuffer  ibuf  = app->flat_shade ? app->index_buffer_flat   : app->index_buffer_smooth;
    uint32_t  icnt  = app->flat_shade ? 12 : 12;  // both have 12 indices

    for (uint32_t i = 0; i < app->image_count; i++) {
        ENGINE_xzlog("vkBeginCommandBuffer (scene)");
        VkCommandBufferBeginInfo begin_info = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        check_vk(vkBeginCommandBuffer(app->command_buffers[i], &begin_info), "vkBeginCommandBuffer");

        VkImageMemoryBarrier barrier_to_render = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = 0, .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = app->images[i],
            .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
        };
        vkCmdPipelineBarrier(app->command_buffers[i],
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            0, 0, nullptr, 0, nullptr, 1, &barrier_to_render);

        VkRenderingAttachmentInfo color_attachment = {
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .imageView = app->image_views[i],
            .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR, .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .clearValue = {{0.0f, 0.0f, 0.0f, 1.0f}},
        };
        VkRenderingAttachmentInfo depth_attachment = {
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .imageView = app->depth_image_view,
            .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR, .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .clearValue = {.depthStencil = {1.0f, 0}},
        };
        VkRenderingInfo rendering_info = {
            .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .renderArea = {{0, 0}, app->swapchain_extent},
            .layerCount = 1, .colorAttachmentCount = 1,
            .pColorAttachments = &color_attachment,
            .pDepthAttachment  = &depth_attachment,
        };

        ENGINE_xzlog("vkCmdBeginRendering");
        vkCmdBeginRendering(app->command_buffers[i], &rendering_info);

        // --- Tetrahedron ---
        VkDeviceSize offsets[] = {0};
        ENGINE_xzlog("vkCmdBindPipeline (tetra)");
        vkCmdBindPipeline(app->command_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, app->pipeline);
        vkCmdBindVertexBuffers(app->command_buffers[i], 0, 1, &vbuf, offsets);
        vkCmdBindIndexBuffer(app->command_buffers[i], ibuf, 0, VK_INDEX_TYPE_UINT32);
        vkCmdBindDescriptorSets(app->command_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS,
            app->pipeline_layout, 0, 1, &app->descriptor_sets[i], 0, nullptr);
        ENGINE_xzlog("vkCmdDrawIndexed (tetra)");
        vkCmdDrawIndexed(app->command_buffers[i], icnt, 1, 0, 0, 0);

        // --- Face overlay quad ---
        ENGINE_xzlog("vkCmdBindPipeline (face)");
        vkCmdBindPipeline(app->command_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, app->face_pipeline);
        vkCmdBindVertexBuffers(app->command_buffers[i], 0, 1, &app->quad_vertex_buffer, offsets);
        vkCmdBindIndexBuffer(app->command_buffers[i], app->quad_index_buffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdBindDescriptorSets(app->command_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS,
            app->face_pipeline_layout, 0, 1, &app->face_descriptor_sets[i], 0, nullptr);
        ENGINE_xzlog("vkCmdDrawIndexed (face)");
        vkCmdDrawIndexed(app->command_buffers[i], 6, 1, 0, 0, 0);

        ENGINE_xzlog("vkCmdEndRendering");
        vkCmdEndRendering(app->command_buffers[i]);
        ENGINE_xzlog("vkEndCommandBuffer");
        check_vk(vkEndCommandBuffer(app->command_buffers[i]), "vkEndCommandBuffer");
    }
}

void init_vulkan(AppState* app) {
    uint32_t apiVersion = 0;
    ENGINE_xzlog("vkEnumerateInstanceVersion");
    vkEnumerateInstanceVersion(&apiVersion);
    SDL_Log("Vulkan API version supported: %d.%d.%d",
        VK_VERSION_MAJOR(apiVersion), VK_VERSION_MINOR(apiVersion), VK_VERSION_PATCH(apiVersion));

    std::vector<const char*> requestedInstanceExtensions = {
        VK_KHR_SURFACE_EXTENSION_NAME,
#ifdef VK_USE_PLATFORM_WIN32_KHR
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#elif defined(VK_USE_PLATFORM_XLIB_KHR)
        VK_KHR_XLIB_SURFACE_EXTENSION_NAME,
#elif defined(__APPLE__)
        "VK_EXT_metal_surface", "VK_MVK_ios_surface",
        VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME,
#endif
    };

    uint32_t extCount = 0;
    ENGINE_xzlog("vkEnumerateInstanceExtensionProperties");
    vkEnumerateInstanceExtensionProperties(NULL, &extCount, NULL);
    std::vector<VkExtensionProperties> supportedInstanceExtensions(extCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &extCount, supportedInstanceExtensions.data());

    std::vector<const char*> enabledInstanceExtensions;
    for (const char* extension : requestedInstanceExtensions) {
        bool found = false;
        for (const auto& s : supportedInstanceExtensions) {
            if (strcmp(extension, s.extensionName) == 0) {
                enabledInstanceExtensions.push_back(extension);
                found = true; break;
            }
        }
        if (!found) SDL_Log("\tInstance Extension '%s' is NOT supported.", extension);
    }

    uint32_t layerCount = 0;
    ENGINE_xzlog("vkEnumerateInstanceLayerProperties");
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    std::vector<const char*> requestedInstanceLayers = {"VK_LAYER_KHRONOS_validation"};
    std::vector<const char*> enabledInstanceLayers;
    for (const char* layer : requestedInstanceLayers) {
        for (const auto& s : availableLayers) {
            if (strcmp(layer, s.layerName) == 0) {
                enabledInstanceLayers.push_back(layer);
                SDL_Log("\tInstance Layer '%s' is supported.", layer);
                break;
            }
        }
    }

    VkApplicationInfo appInfo{
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "Tetrahedron Phong",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "No Engine",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_3,
    };
    VkInstanceCreateInfo instanceCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &appInfo,
        .enabledLayerCount = static_cast<uint32_t>(enabledInstanceLayers.size()),
        .ppEnabledLayerNames = enabledInstanceLayers.data(),
        .enabledExtensionCount = static_cast<uint32_t>(enabledInstanceExtensions.size()),
        .ppEnabledExtensionNames = enabledInstanceExtensions.data(),
    };
#ifdef __APPLE__
    instanceCreateInfo.flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif

    ENGINE_xzlog("vkCreateInstance");
    VkResult result = vkCreateInstance(&instanceCreateInfo, NULL, &app->instance);
    if (result != VK_SUCCESS) { SDL_Log("Failed vkCreateInstance! Error code: %d", result); exit(1); }

    bool surfaceResult = SDL_Vulkan_CreateSurface(app->window, app->instance, nullptr, &app->surface);
    if (!surfaceResult) { SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_Vulkan_CreateSurface Error: %s", SDL_GetError()); exit(1); }

    uint32_t device_count = 0;
    ENGINE_xzlog("vkEnumeratePhysicalDevices");
    check_vk(vkEnumeratePhysicalDevices(app->instance, &device_count, NULL), "enumerate count");
    std::vector<VkPhysicalDevice> devices(device_count);
    check_vk(vkEnumeratePhysicalDevices(app->instance, &device_count, devices.data()), "enumerate devices");
    app->physical_device = devices[0];

    uint32_t qfam_count = 0;
    ENGINE_xzlog("vkGetPhysicalDeviceQueueFamilyProperties");
    vkGetPhysicalDeviceQueueFamilyProperties(app->physical_device, &qfam_count, NULL);
    std::vector<VkQueueFamilyProperties> qfams(qfam_count);
    vkGetPhysicalDeviceQueueFamilyProperties(app->physical_device, &qfam_count, qfams.data());
    for (uint32_t i = 0; i < qfam_count; i++) {
        if (qfams[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) { app->graphics_family = i; break; }
    }

    VkSurfaceCapabilitiesKHR surf_caps;
    ENGINE_xzlog("vkGetPhysicalDeviceSurfaceCapabilitiesKHR");
    check_vk(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(app->physical_device, app->surface, &surf_caps), "surface caps");
    app->swapchain_extent = surf_caps.currentExtent;
    app->image_count = surf_caps.minImageCount;
    if ((surf_caps.maxImageCount > 0) && (app->image_count > surf_caps.maxImageCount))
        app->image_count = surf_caps.maxImageCount;

    uint32_t format_count = 0;
    ENGINE_xzlog("vkGetPhysicalDeviceSurfaceFormatsKHR");
    check_vk(vkGetPhysicalDeviceSurfaceFormatsKHR(app->physical_device, app->surface, &format_count, NULL), "format count");
    std::vector<VkSurfaceFormatKHR> formats(format_count);
    check_vk(vkGetPhysicalDeviceSurfaceFormatsKHR(app->physical_device, app->surface, &format_count, formats.data()), "formats");

    app->surface_format = formats[0];
    VkFormat desired_formats[] = {VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_A8B8G8R8_UNORM_PACK32};
    for (int i = 0; i < 3; i++) {
        for (uint32_t j = 0; j < format_count; j++) {
            if (desired_formats[i] == formats[j].format) {
                app->surface_format = formats[j];
                const char* fmt_names[] = {"B8G8R8A8_UNORM", "R8G8B8A8_UNORM", "A8B8G8R8_UNORM"};
                SDL_Log("Selected Surface Format: %s", fmt_names[i]);
                i = 3; break;
            }
        }
    }

    uint32_t mode_count = 0;
    ENGINE_xzlog("vkGetPhysicalDeviceSurfacePresentModesKHR");
    check_vk(vkGetPhysicalDeviceSurfacePresentModesKHR(app->physical_device, app->surface, &mode_count, NULL), "mode count");
    std::vector<VkPresentModeKHR> modes(mode_count);
    check_vk(vkGetPhysicalDeviceSurfacePresentModesKHR(app->physical_device, app->surface, &mode_count, modes.data()), "modes");
    VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;

    float priority = 1.0f;
    VkDeviceQueueCreateInfo queue_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = app->graphics_family, .queueCount = 1, .pQueuePriorities = &priority,
    };
    std::vector<const char*> deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
#ifdef __APPLE__
    deviceExtensions.push_back(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
#endif
    VkPhysicalDeviceDynamicRenderingFeatures dynamic_rendering_feature = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES,
        .dynamicRendering = VK_TRUE,
    };
    VkDeviceCreateInfo device_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &dynamic_rendering_feature,
        .queueCreateInfoCount = 1, .pQueueCreateInfos = &queue_info,
        .enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()),
        .ppEnabledExtensionNames = deviceExtensions.data(),
    };
    ENGINE_xzlog("vkCreateDevice");
    check_vk(vkCreateDevice(app->physical_device, &device_info, NULL, &app->device), "vkCreateDevice");
    ENGINE_xzlog("vkGetDeviceQueue");
    vkGetDeviceQueue(app->device, app->graphics_family, 0, &app->graphics_queue);

    VkCompositeAlphaFlagBitsKHR alphaMode = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    if (surf_caps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR)
        alphaMode = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    else if (surf_caps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR)
        alphaMode = VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR;
    else if (surf_caps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR)
        alphaMode = VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR;

    VkImageUsageFlags imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if (surf_caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    if (surf_caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    VkSwapchainCreateInfoKHR swapchain_info = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = app->surface, .minImageCount = app->image_count,
        .imageFormat = app->surface_format.format,
        .imageColorSpace = app->surface_format.colorSpace,
        .imageExtent = app->swapchain_extent, .imageArrayLayers = 1,
        .imageUsage = imageUsage, .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0, .preTransform = surf_caps.currentTransform,
        .compositeAlpha = alphaMode, .presentMode = present_mode, .clipped = VK_TRUE,
    };
    ENGINE_xzlog("vkCreateSwapchainKHR");
    check_vk(vkCreateSwapchainKHR(app->device, &swapchain_info, NULL, &app->swapchain), "vkCreateSwapchainKHR");

    ENGINE_xzlog("vkGetSwapchainImagesKHR");
    check_vk(vkGetSwapchainImagesKHR(app->device, app->swapchain, &app->image_count, NULL), "image count");
    app->images.resize(app->image_count);
    app->image_views.resize(app->image_count);
    check_vk(vkGetSwapchainImagesKHR(app->device, app->swapchain, &app->image_count, app->images.data()), "images");

    for (uint32_t i = 0; i < app->image_count; i++) {
        VkImageViewCreateInfo view_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = app->images[i], .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = app->surface_format.format,
            .components = {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A},
            .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
        };
        ENGINE_xzlog("vkCreateImageView (swapchain)");
        check_vk(vkCreateImageView(app->device, &view_info, NULL, &app->image_views[i]), "vkCreateImageView");
    }

    SDL_Log("<SwapChain>: %ux%u, count=%d", app->swapchain_extent.width, app->swapchain_extent.height, app->image_count);

#ifdef TARGET_OS_IPHONE
    #define SHADER_OUTPUT_DIR ""
    #define ASSET_OUTPUT_DIR ""
#endif

    size_t vert_size, frag_size, face_size;
    uint32_t* vert_spv = load_shader_file(SHADER_OUTPUT_DIR "vert.spv", &vert_size);
    uint32_t* frag_spv = load_shader_file(SHADER_OUTPUT_DIR "frag.spv", &frag_size);
    uint32_t* face_spv = load_shader_file(SHADER_OUTPUT_DIR "face.spv", &face_size);
    if (!vert_spv || !frag_spv || !face_spv) return;

    VkShaderModule vert_shader, frag_shader, face_shader;
    VkShaderModuleCreateInfo vert_mod = {.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, .codeSize = vert_size, .pCode = vert_spv};
    VkShaderModuleCreateInfo frag_mod = {.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, .codeSize = frag_size, .pCode = frag_spv};
    VkShaderModuleCreateInfo face_mod = {.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, .codeSize = face_size, .pCode = face_spv};
    ENGINE_xzlog("vkCreateShaderModule (vert)");
    check_vk(vkCreateShaderModule(app->device, &vert_mod, NULL, &vert_shader), "vert shader");
    ENGINE_xzlog("vkCreateShaderModule (frag)");
    check_vk(vkCreateShaderModule(app->device, &frag_mod, NULL, &frag_shader), "frag shader");
    ENGINE_xzlog("vkCreateShaderModule (face)");
    check_vk(vkCreateShaderModule(app->device, &face_mod, NULL, &face_shader), "face shader");

    VkCommandPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = app->graphics_family,
    };
    ENGINE_xzlog("vkCreateCommandPool");
    check_vk(vkCreateCommandPool(app->device, &pool_info, NULL, &app->command_pool), "vkCreateCommandPool");

    create_texture(app);
    create_depth_buffer(app);
    create_tetra_buffers(app);
    create_quad_buffers(app);
    create_uniform_buffers(app);
    create_descriptors(app);

    /* --- Shared pipeline state --- */
    VkVertexInputBindingDescription binding_desc = {
        .binding = 0, .stride = sizeof(Vertex), .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    };
    VkVertexInputAttributeDescription attr_descs[3] = {
        {.location = 0, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(Vertex, position)},
        {.location = 1, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT,    .offset = offsetof(Vertex, uv)},
        {.location = 2, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(Vertex, normal)},
    };
    VkPipelineVertexInputStateCreateInfo vertex_input = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1, .pVertexBindingDescriptions = &binding_desc,
        .vertexAttributeDescriptionCount = 3, .pVertexAttributeDescriptions = attr_descs,
    };
    VkPipelineInputAssemblyStateCreateInfo input_assembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    };
    VkViewport viewport = {.x = 0, .y = 0,
        .width = (float)app->swapchain_extent.width, .height = (float)app->swapchain_extent.height,
        .minDepth = 0.0f, .maxDepth = 1.0f};
    VkRect2D scissor = {.offset = {0, 0}, .extent = app->swapchain_extent};
    VkPipelineViewportStateCreateInfo viewport_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1, .pViewports = &viewport, .scissorCount = 1, .pScissors = &scissor,
    };
    VkPipelineRasterizationStateCreateInfo rasterizer = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL, .cullMode = VK_CULL_MODE_BACK_BIT,
        .frontFace = VK_FRONT_FACE_CLOCKWISE, .lineWidth = 1.0f,
    };
    VkPipelineMultisampleStateCreateInfo multisampling = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    };
    VkPipelineDepthStencilStateCreateInfo depth_stencil = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = VK_TRUE, .depthWriteEnable = VK_TRUE,
        .depthCompareOp = VK_COMPARE_OP_LESS,
    };
    VkPipelineRenderingCreateInfo pipeline_rendering_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = 1, .pColorAttachmentFormats = &app->surface_format.format,
        .depthAttachmentFormat = DEPTH_FORMAT,
    };
    VkPipelineColorBlendAttachmentState opaque_blend = {
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    };
    VkPipelineColorBlendStateCreateInfo opaque_blending = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1, .pAttachments = &opaque_blend,
    };

    /* --- Tetrahedron pipeline --- */
    VkPipelineLayoutCreateInfo layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1, .pSetLayouts = &app->descriptor_set_layout,
    };
    ENGINE_xzlog("vkCreatePipelineLayout (tetra)");
    check_vk(vkCreatePipelineLayout(app->device, &layout_info, NULL, &app->pipeline_layout), "tetra layout");

    VkPipelineShaderStageCreateInfo tetra_stages[2] = {
        {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
         .stage = VK_SHADER_STAGE_VERTEX_BIT,   .module = vert_shader, .pName = "main"},
        {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
         .stage = VK_SHADER_STAGE_FRAGMENT_BIT, .module = frag_shader, .pName = "main"},
    };
    VkGraphicsPipelineCreateInfo tetra_pipeline_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &pipeline_rendering_info, .stageCount = 2, .pStages = tetra_stages,
        .pVertexInputState = &vertex_input, .pInputAssemblyState = &input_assembly,
        .pViewportState = &viewport_state, .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisampling, .pDepthStencilState = &depth_stencil,
        .pColorBlendState = &opaque_blending, .layout = app->pipeline_layout,
    };
    ENGINE_xzlog("vkCreateGraphicsPipelines (tetra)");
    check_vk(vkCreateGraphicsPipelines(app->device, VK_NULL_HANDLE, 1, &tetra_pipeline_info, NULL, &app->pipeline), "tetra pipeline");

    /* --- Face overlay pipeline --- */
    // Face quad uses QuadVertex (position + uv, no normal)
    VkVertexInputBindingDescription quad_binding_desc = {
        .binding = 0, .stride = sizeof(QuadVertex), .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    };
    VkVertexInputAttributeDescription quad_attr_descs[2] = {
        {.location = 0, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(QuadVertex, position)},
        {.location = 1, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT,    .offset = offsetof(QuadVertex, uv)},
    };
    VkPipelineVertexInputStateCreateInfo quad_vertex_input = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1, .pVertexBindingDescriptions = &quad_binding_desc,
        .vertexAttributeDescriptionCount = 2, .pVertexAttributeDescriptions = quad_attr_descs,
    };

    VkPipelineLayoutCreateInfo face_layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1, .pSetLayouts = &app->face_descriptor_set_layout,
    };
    ENGINE_xzlog("vkCreatePipelineLayout (face)");
    check_vk(vkCreatePipelineLayout(app->device, &face_layout_info, NULL, &app->face_pipeline_layout), "face layout");

    VkPipelineColorBlendAttachmentState alpha_blend = {
        .blendEnable = VK_TRUE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        .alphaBlendOp = VK_BLEND_OP_ADD,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    };
    VkPipelineColorBlendStateCreateInfo alpha_blending = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1, .pAttachments = &alpha_blend,
    };
    VkPipelineDepthStencilStateCreateInfo face_depth = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = VK_TRUE, .depthWriteEnable = VK_FALSE,
        .depthCompareOp = VK_COMPARE_OP_LESS,
    };
    VkPipelineRasterizationStateCreateInfo face_rasterizer = rasterizer;
    face_rasterizer.cullMode = VK_CULL_MODE_NONE;

    VkPipelineShaderStageCreateInfo face_stages[2] = {
        {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
         .stage = VK_SHADER_STAGE_VERTEX_BIT,   .module = vert_shader, .pName = "main"},
        {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
         .stage = VK_SHADER_STAGE_FRAGMENT_BIT, .module = face_shader, .pName = "main"},
    };
    VkGraphicsPipelineCreateInfo face_pipeline_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &pipeline_rendering_info, .stageCount = 2, .pStages = face_stages,
        .pVertexInputState = &quad_vertex_input, .pInputAssemblyState = &input_assembly,
        .pViewportState = &viewport_state, .pRasterizationState = &face_rasterizer,
        .pMultisampleState = &multisampling, .pDepthStencilState = &face_depth,
        .pColorBlendState = &alpha_blending, .layout = app->face_pipeline_layout,
    };
    ENGINE_xzlog("vkCreateGraphicsPipelines (face)");
    check_vk(vkCreateGraphicsPipelines(app->device, VK_NULL_HANDLE, 1, &face_pipeline_info, NULL, &app->face_pipeline), "face pipeline");

    /* --- Synchronization --- */
    VkSemaphoreCreateInfo sem_info = {.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkFenceCreateInfo fence_info = {.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags = VK_FENCE_CREATE_SIGNALED_BIT};
    ENGINE_xzlog("vkCreateSemaphore (image_available)");
    check_vk(vkCreateSemaphore(app->device, &sem_info, NULL, &app->image_available_sem), "image_available");
    ENGINE_xzlog("vkCreateSemaphore (render_finished)");
    check_vk(vkCreateSemaphore(app->device, &sem_info, NULL, &app->render_finished_sem), "render_finished");
    ENGINE_xzlog("vkCreateFence");
    check_vk(vkCreateFence(app->device, &fence_info, NULL, &app->in_flight_fence), "in_flight");

    app->command_buffers.resize(app->image_count);
    VkCommandBufferAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = app->command_pool, .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = app->image_count,
    };
    ENGINE_xzlog("vkAllocateCommandBuffers (scene)");
    check_vk(vkAllocateCommandBuffers(app->device, &alloc_info, app->command_buffers.data()), "vkAllocateCommandBuffers");

    app->imgui_command_buffers.resize(app->image_count);
    VkCommandBufferAllocateInfo imgui_alloc_info = alloc_info;
    ENGINE_xzlog("vkAllocateCommandBuffers (imgui)");
    check_vk(vkAllocateCommandBuffers(app->device, &imgui_alloc_info, app->imgui_command_buffers.data()), "imgui cmd bufs");

    // Record initial command buffers
    record_scene_command_buffers(app);

    free(vert_spv); free(frag_spv); free(face_spv);
    ENGINE_xzlog("vkDestroyShaderModule (vert)");
    vkDestroyShaderModule(app->device, vert_shader, NULL);
    ENGINE_xzlog("vkDestroyShaderModule (frag)");
    vkDestroyShaderModule(app->device, frag_shader, NULL);
    ENGINE_xzlog("vkDestroyShaderModule (face)");
    vkDestroyShaderModule(app->device, face_shader, NULL);

    /* --- ImGui --- */
    VkDescriptorPoolSize imgui_pool_sizes[] = {
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, IMGUI_IMPL_VULKAN_MINIMUM_SAMPLED_IMAGE_POOL_SIZE},
        {VK_DESCRIPTOR_TYPE_SAMPLER,       IMGUI_IMPL_VULKAN_MINIMUM_SAMPLER_POOL_SIZE},
    };
    VkDescriptorPoolCreateInfo dp_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets = IMGUI_IMPL_VULKAN_MINIMUM_SAMPLED_IMAGE_POOL_SIZE + IMGUI_IMPL_VULKAN_MINIMUM_SAMPLER_POOL_SIZE,
        .poolSizeCount = 2, .pPoolSizes = imgui_pool_sizes,
    };
    check_vk(vkCreateDescriptorPool(app->device, &dp_info, nullptr, &app->imgui_descriptor_pool), "imgui descriptor pool");

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplSDL3_InitForVulkan(app->window);

    ImGui_ImplVulkan_InitInfo imgui_vk_info = {};
    imgui_vk_info.ApiVersion          = VK_API_VERSION_1_3;
    imgui_vk_info.Instance            = app->instance;
    imgui_vk_info.PhysicalDevice      = app->physical_device;
    imgui_vk_info.Device              = app->device;
    imgui_vk_info.QueueFamily         = app->graphics_family;
    imgui_vk_info.Queue               = app->graphics_queue;
    imgui_vk_info.DescriptorPool      = app->imgui_descriptor_pool;
    imgui_vk_info.MinImageCount       = app->image_count;
    imgui_vk_info.ImageCount          = app->image_count;
    imgui_vk_info.UseDynamicRendering = true;
    imgui_vk_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    imgui_vk_info.PipelineInfoMain.PipelineRenderingCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = 1, .pColorAttachmentFormats = &app->surface_format.format,
    };
    ImGui_ImplVulkan_Init(&imgui_vk_info);
}

void cleanup_vulkan(AppState* app) {
    ENGINE_xzlog("vkDeviceWaitIdle");
    vkDeviceWaitIdle(app->device);

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    vkDestroyDescriptorPool(app->device, app->imgui_descriptor_pool, nullptr);

    // --- Face overlay ---
    ENGINE_xzlog("vkDestroyPipeline (face)");
    vkDestroyPipeline(app->device, app->face_pipeline, nullptr);
    vkDestroyPipelineLayout(app->device, app->face_pipeline_layout, nullptr);
    vkDestroyDescriptorPool(app->device, app->face_descriptor_pool, nullptr);
    vkDestroyDescriptorSetLayout(app->device, app->face_descriptor_set_layout, nullptr);
    for (uint32_t i = 0; i < app->image_count; i++) {
        vkDestroyBuffer(app->device, app->quad_uniform_buffers[i], nullptr);
        vkFreeMemory(app->device, app->quad_uniform_memory[i], nullptr);
    }
    vkDestroyBuffer(app->device, app->quad_vertex_buffer, nullptr);
    vkFreeMemory(app->device, app->quad_vertex_memory, nullptr);
    vkDestroyBuffer(app->device, app->quad_index_buffer, nullptr);
    vkFreeMemory(app->device, app->quad_index_memory, nullptr);

    // --- Depth ---
    ENGINE_xzlog("vkDestroyImageView (depth)");
    vkDestroyImageView(app->device, app->depth_image_view, nullptr);
    vkDestroyImage(app->device, app->depth_image, nullptr);
    vkFreeMemory(app->device, app->depth_image_memory, nullptr);

    // --- Texture ---
    ENGINE_xzlog("vkDestroySampler");
    vkDestroySampler(app->device, app->texture_sampler, nullptr);
    vkDestroyImageView(app->device, app->texture_image_view, nullptr);
    vkDestroyImage(app->device, app->texture_image, nullptr);
    vkFreeMemory(app->device, app->texture_image_memory, nullptr);

    // --- Uniforms ---
    for (uint32_t i = 0; i < app->image_count; i++) {
        vkDestroyBuffer(app->device, app->uniform_buffers[i], nullptr);
        vkFreeMemory(app->device, app->uniform_buffers_memory[i], nullptr);
    }
    vkDestroyDescriptorPool(app->device, app->descriptor_pool, nullptr);
    vkDestroyDescriptorSetLayout(app->device, app->descriptor_set_layout, nullptr);

    // --- Tetra buffers ---
    ENGINE_xzlog("vkDestroyBuffer (tetra flat/smooth)");
    vkDestroyBuffer(app->device, app->vertex_buffer_flat,   nullptr); vkFreeMemory(app->device, app->vertex_memory_flat,   nullptr);
    vkDestroyBuffer(app->device, app->index_buffer_flat,    nullptr); vkFreeMemory(app->device, app->index_memory_flat,    nullptr);
    vkDestroyBuffer(app->device, app->vertex_buffer_smooth, nullptr); vkFreeMemory(app->device, app->vertex_memory_smooth, nullptr);
    vkDestroyBuffer(app->device, app->index_buffer_smooth,  nullptr); vkFreeMemory(app->device, app->index_memory_smooth,  nullptr);

    ENGINE_xzlog("vkDestroySemaphore");
    vkDestroySemaphore(app->device, app->image_available_sem, NULL);
    vkDestroySemaphore(app->device, app->render_finished_sem, NULL);
    ENGINE_xzlog("vkDestroyFence");
    vkDestroyFence(app->device, app->in_flight_fence, NULL);
    ENGINE_xzlog("vkDestroyCommandPool");
    vkDestroyCommandPool(app->device, app->command_pool, NULL);
    ENGINE_xzlog("vkDestroyPipeline (tetra)");
    vkDestroyPipeline(app->device, app->pipeline, NULL);
    vkDestroyPipelineLayout(app->device, app->pipeline_layout, NULL);
    for (auto& iv : app->image_views) vkDestroyImageView(app->device, iv, NULL);
    ENGINE_xzlog("vkDestroySwapchainKHR");
    vkDestroySwapchainKHR(app->device, app->swapchain, NULL);
    ENGINE_xzlog("vkDestroyDevice");
    vkDestroyDevice(app->device, NULL);
    vkDestroySurfaceKHR(app->instance, app->surface, NULL);
    ENGINE_xzlog("vkDestroyInstance");
    vkDestroyInstance(app->instance, NULL);
}

void update_gui(AppState* app)
{
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    if (app->show_gui) {
        ImGui::Begin("Tetrahedron", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::Text("Press T to toggle GUI");
        ImGui::Separator();
        ImGui::SliderFloat("Rotation X", &app->rotation_x, -180.0f, 180.0f);
        ImGui::SliderFloat("Rotation Y", &app->rotation_y, -180.0f, 180.0f);
        ImGui::SliderFloat("Rotation Z", &app->rotation_z, -180.0f, 180.0f);
        ImGui::Separator();
        ImGui::SliderFloat("Light X", &app->light_pos.x, -10.0f, 10.0f);
        ImGui::SliderFloat("Light Y", &app->light_pos.y, -10.0f, 10.0f);
        ImGui::SliderFloat("Light Z", &app->light_pos.z, -10.0f, 10.0f);
        ImGui::Separator();

        bool prev_flat = app->flat_shade;
        ImGui::Checkbox("Flat Shading", &app->flat_shade);
        if (app->flat_shade != prev_flat) {
            // Shading mode changed — re-record command buffers with new vertex buffer
            vkDeviceWaitIdle(app->device);
            record_scene_command_buffers(app);
        }

        ImGui::Text("Application average %.1f FPS", ImGui::GetIO().Framerate);
        ImGui::End();
    }
    ImGui::Render();
}

void render_frame(AppState* app) {
    ENGINE_xziter_log("vkWaitForFences");
    check_vk(vkWaitForFences(app->device, 1, &app->in_flight_fence, VK_TRUE, UINT64_MAX), "vkWaitForFences");
    ENGINE_xziter_log("vkResetFences");
    check_vk(vkResetFences(app->device, 1, &app->in_flight_fence), "vkResetFences");

    uint32_t image_index;
    ENGINE_xziter_log("vkAcquireNextImageKHR");
    check_vk(vkAcquireNextImageKHR(app->device, app->swapchain, UINT64_MAX,
        app->image_available_sem, VK_NULL_HANDLE, &image_index), "vkAcquireNextImageKHR");

    update_gui(app);
    update_uniform_buffer(app, image_index);
    record_imgui_command_buffers(app, image_index);

    VkCommandBuffer cmd_bufs[] = {
        app->command_buffers[image_index],
        app->imgui_command_buffers[image_index],
    };
    VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1, .pWaitSemaphores = &app->image_available_sem,
        .pWaitDstStageMask = wait_stages,
        .commandBufferCount = 2, .pCommandBuffers = cmd_bufs,
        .signalSemaphoreCount = 1, .pSignalSemaphores = &app->render_finished_sem,
    };
    ENGINE_xziter_log("vkQueueSubmit");
    check_vk(vkQueueSubmit(app->graphics_queue, 1, &submit_info, app->in_flight_fence), "vkQueueSubmit");

    VkPresentInfoKHR present_info = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1, .pWaitSemaphores = &app->render_finished_sem,
        .swapchainCount = 1, .pSwapchains = &app->swapchain, .pImageIndices = &image_index,
    };
    ENGINE_xziter_log("vkQueuePresentKHR");
    check_vk(vkQueuePresentKHR(app->graphics_queue, &present_info), "vkQueuePresentKHR");
}

SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[]) {
    auto app = std::make_unique<AppState>();
    *appstate = app.release();
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    app = std::unique_ptr<AppState>((AppState*)*appstate);
    app->window = SDL_CreateWindow("Part 13 - Blinn-Phong", WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_VULKAN);
    if (!app->window) {
        SDL_Log("Window creation failed: %s", SDL_GetError());
        SDL_Quit();
        return SDL_APP_FAILURE;
    }
    init_vulkan(app.get());
    *appstate = app.release();
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
    ImGui_ImplSDL3_ProcessEvent(event);
    if (event->type == SDL_EVENT_KEY_DOWN && event->key.scancode == SDL_SCANCODE_T)
        ((AppState*)appstate)->show_gui = !((AppState*)appstate)->show_gui;
    if (event->type == SDL_EVENT_QUIT)
        return SDL_APP_SUCCESS;
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void* appstate) {
    render_frame((AppState*)appstate);
    SDL_Delay(16);
    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* appstate, SDL_AppResult result) {
    auto app = std::unique_ptr<AppState>((AppState*)appstate);
    cleanup_vulkan(app.get());
    SDL_DestroyWindow(app->window);
    SDL_Quit();
}