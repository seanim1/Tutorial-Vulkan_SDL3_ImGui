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

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600

#define ENGINE_xzlog(vk_call) \
    SDL_Log("ENGINE_xz: %s [%s:%d]", vk_call, __FILE__, __LINE__)
#define ENGINE_xziter_log(vk_call) \
    SDL_Log("ENGINE_xz.iter: %s [%s:%d]", vk_call, __FILE__, __LINE__)

// --- Vertex ---
struct Vertex {
    float position[3];
};

const Vertex vertices[] = {
    {{ 0.0f, -0.5f, 0.0f}},
    {{ 0.5f,  0.5f, 0.0f}},
    {{-0.5f,  0.5f, 0.0f}},
};

const uint32_t indices[] = { 0, 1, 2 };
// --------------

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
    std::vector<VkCommandBuffer> command_buffers;       // scene (no final barrier)
    std::vector<VkCommandBuffer> imgui_command_buffers; // imgui + final barrier
    VkSemaphore image_available_sem = nullptr;
    VkSemaphore render_finished_sem = nullptr;
    VkFence in_flight_fence = nullptr;
    uint32_t image_count = 0;
    uint32_t graphics_family = 0;
    VkExtent2D swapchain_extent = {};
    VkSurfaceFormatKHR surface_format = {};

    // --- Vertex buffer ---
    VkBuffer vertex_buffer = nullptr;
    VkDeviceMemory vertex_buffer_memory = nullptr;

    // --- Index buffer ---
    VkBuffer index_buffer = nullptr;
    VkDeviceMemory index_buffer_memory = nullptr;

    // --- ImGui ---
    VkDescriptorPool imgui_descriptor_pool = nullptr;
    bool show_gui = true;
    float clear_r = 0.0f, clear_g = 0.0f, clear_b = 0.0f;
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
    ENGINE_xzlog("vkAllocateMemory");
    vkGetPhysicalDeviceMemoryProperties(physical_device, &mem_props);
    for (uint32_t i = 0; i < mem_props.memoryTypeCount; i++) {
        if ((type_filter & (1 << i)) &&
            (mem_props.memoryTypes[i].propertyFlags & properties) == properties)
            return i;
    }
    SDL_Log("Failed to find suitable memory type");
    exit(1);
}

void record_imgui_command_buffers(AppState* app, uint32_t i)
{
    VkCommandBufferBeginInfo begin = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,
    };
    check_vk(vkBeginCommandBuffer(app->imgui_command_buffers[i], &begin),
             "imgui begin");

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
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(),
                                    app->imgui_command_buffers[i]);
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

    check_vk(vkEndCommandBuffer(app->imgui_command_buffers[i]),
             "imgui end");
}

void create_vertex_buffer(AppState* app)
{
    VkBufferCreateInfo buf_info = {
        .sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size        = sizeof(vertices),
        .usage       = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    ENGINE_xzlog("vkCreateBuffer");
    check_vk(vkCreateBuffer(app->device, &buf_info, nullptr, &app->vertex_buffer), "vkCreateBuffer");

    VkMemoryRequirements mem_reqs;
    vkGetBufferMemoryRequirements(app->device, app->vertex_buffer, &mem_reqs);

    VkMemoryAllocateInfo alloc_mem_info = {
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize  = mem_reqs.size,
        .memoryTypeIndex = find_memory_type(app->physical_device, mem_reqs.memoryTypeBits,
                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                               VK_MEMORY_PROPERTY_HOST_COHERENT_BIT),
    };
    ENGINE_xzlog("vkAllocateMemory");
    check_vk(vkAllocateMemory(app->device, &alloc_mem_info, nullptr, &app->vertex_buffer_memory), "vkAllocateMemory");
    check_vk(vkBindBufferMemory(app->device, app->vertex_buffer, app->vertex_buffer_memory, 0), "vkBindBufferMemory");

    void* mapped;
    ENGINE_xzlog("vkMapMemory");
    vkMapMemory(app->device, app->vertex_buffer_memory, 0, sizeof(vertices), 0, &mapped);
    memcpy(mapped, vertices, sizeof(vertices));
    ENGINE_xzlog("vkUnmapMemory");
    vkUnmapMemory(app->device, app->vertex_buffer_memory);
}

void create_index_buffer(AppState* app)
{
    VkBufferCreateInfo buf_info = {
        .sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size        = sizeof(indices),
        .usage       = VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    ENGINE_xzlog("vkCreateBuffer (index)");
    check_vk(vkCreateBuffer(app->device, &buf_info, nullptr, &app->index_buffer), "vkCreateBuffer index");

    VkMemoryRequirements mem_reqs;
    vkGetBufferMemoryRequirements(app->device, app->index_buffer, &mem_reqs);

    VkMemoryAllocateInfo alloc_mem_info = {
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize  = mem_reqs.size,
        .memoryTypeIndex = find_memory_type(app->physical_device, mem_reqs.memoryTypeBits,
                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                               VK_MEMORY_PROPERTY_HOST_COHERENT_BIT),
    };
    ENGINE_xzlog("vkAllocateMemory (index)");
    check_vk(vkAllocateMemory(app->device, &alloc_mem_info, nullptr, &app->index_buffer_memory), "vkAllocateMemory index");
    check_vk(vkBindBufferMemory(app->device, app->index_buffer, app->index_buffer_memory, 0), "vkBindBufferMemory index");

    void* mapped;
    vkMapMemory(app->device, app->index_buffer_memory, 0, sizeof(indices), 0, &mapped);
    memcpy(mapped, indices, sizeof(indices));
    vkUnmapMemory(app->device, app->index_buffer_memory);
}

void init_vulkan(AppState* app) {
    /* --- Vulkan API Version --- */
    uint32_t apiVersion = 0;
    ENGINE_xzlog("vkEnumerateInstanceVersion");
    vkEnumerateInstanceVersion(&apiVersion);
    SDL_Log("Vulkan API version supported: %d.%d.%d",
        VK_VERSION_MAJOR(apiVersion),
        VK_VERSION_MINOR(apiVersion),
        VK_VERSION_PATCH(apiVersion));

    /* --- Requested Extensions --- */
    std::vector<const char*> requestedInstanceExtensions = {
        VK_KHR_SURFACE_EXTENSION_NAME,
#ifdef VK_USE_PLATFORM_WIN32_KHR
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#elif defined(VK_USE_PLATFORM_XLIB_KHR)
        VK_KHR_XLIB_SURFACE_EXTENSION_NAME,
#elif defined(__APPLE__)
        "VK_EXT_metal_surface",
        "VK_MVK_ios_surface",
        VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME,
#endif
    };

    /* --- Enumerate Supported Extensions --- */
    uint32_t extCount = 0;
    ENGINE_xzlog("vkEnumerateInstanceExtensionProperties");
    vkEnumerateInstanceExtensionProperties(NULL, &extCount, NULL);
    std::vector<VkExtensionProperties> supportedInstanceExtensions(extCount);
    ENGINE_xzlog("vkEnumerateInstanceExtensionProperties");
    vkEnumerateInstanceExtensionProperties(nullptr, &extCount, supportedInstanceExtensions.data());
    SDL_Log("Supported Instance Extensions (%d count):", extCount);
    for (uint32_t i = 0; i < extCount; i++) {
        SDL_Log("\t%d: %s", i, supportedInstanceExtensions[i].extensionName);
    }

    /* --- Filter Enabled Extensions --- */
    std::vector<const char*> enabledInstanceExtensions;
    for (const char* extension : requestedInstanceExtensions) {
        bool found = false;
        for (const auto& supportedExtension : supportedInstanceExtensions) {
            if (strcmp(extension, supportedExtension.extensionName) == 0) {
                enabledInstanceExtensions.push_back(extension);
                SDL_Log("\tInstance Extension '%s' is supported.", extension);
                found = true;
                break;
            }
        }
        if (!found) {
            SDL_Log("\tInstance Extension '%s' is NOT supported.", extension);
        }
    }

    /* --- Enumerate Validation Layers --- */
    uint32_t layerCount = 0;
    ENGINE_xzlog("vkEnumerateInstanceLayerProperties");
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    std::vector<VkLayerProperties> availableLayers(layerCount);
    ENGINE_xzlog("vkEnumerateInstanceLayerProperties");
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());
    SDL_Log("Supported Validation Layers (%d count):", layerCount);
    for (uint32_t i = 0; i < layerCount; i++) {
        const VkLayerProperties& layer = availableLayers[i];
    }

    /* --- Requested Layers --- */
    std::vector<const char*> requestedInstanceLayers;
    #ifdef _DEBUG
    requestedInstanceLayers.push_back("VK_LAYER_KHRONOS_validation");
    #endif

    /* --- Filter Enabled Layers --- */
    std::vector<const char*> enabledInstanceLayers;
    for (const char* layer : requestedInstanceLayers) {
        bool found = false;
        for (const auto& supportedLayer : availableLayers) {
            if (strcmp(layer, supportedLayer.layerName) == 0) {
                enabledInstanceLayers.push_back(layer);
                SDL_Log("\tInstance Layer '%s' is supported.", layer);
                found = true;
                break;
            }
        }
        if (!found) {
            SDL_Log("\tInstance Layer '%s' is NOT supported.", layer);
        }
    }

    /* --- Create Instance --- */
    VkApplicationInfo appInfo{
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "Hello Triangle",
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
    if (result != VK_SUCCESS) {
        SDL_Log("Failed vkCreateInstance! Error code: %d", result);
        exit(1);
    }

    /* --- Create Surface (platform agnostic via SDL) --- */
    bool surfaceResult = SDL_Vulkan_CreateSurface(app->window, app->instance, nullptr, &app->surface);
    if (!surfaceResult) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_Vulkan_CreateSurface Error: %s", SDL_GetError());
        exit(1);
    }

    /* --- Physical Device --- */
    uint32_t device_count = 0;
    ENGINE_xzlog("vkEnumeratePhysicalDevices");
    check_vk(vkEnumeratePhysicalDevices(app->instance, &device_count, NULL), "enumerate count");
    std::vector<VkPhysicalDevice> devices(device_count);
    ENGINE_xzlog("vkEnumeratePhysicalDevices");
    check_vk(vkEnumeratePhysicalDevices(app->instance, &device_count, devices.data()), "enumerate devices");
    app->physical_device = devices[0];

    /* --- Queue Family --- */
    uint32_t qfam_count = 0;
    ENGINE_xzlog("vkGetPhysicalDeviceQueueFamilyProperties");
    vkGetPhysicalDeviceQueueFamilyProperties(app->physical_device, &qfam_count, NULL);
    std::vector<VkQueueFamilyProperties> qfams(qfam_count);
    ENGINE_xzlog("vkGetPhysicalDeviceQueueFamilyProperties");
    vkGetPhysicalDeviceQueueFamilyProperties(app->physical_device, &qfam_count, qfams.data());
    for (uint32_t i = 0; i < qfam_count; i++) {
        if (qfams[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            app->graphics_family = i;
            break;
        }
    }

    /* --- Surface Capabilities --- */
    VkSurfaceCapabilitiesKHR surf_caps;
    ENGINE_xzlog("vkGetPhysicalDeviceSurfaceCapabilitiesKHR");
    check_vk(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(app->physical_device, app->surface, &surf_caps), "surface caps");
    app->swapchain_extent = surf_caps.currentExtent;
    app->image_count = surf_caps.minImageCount;
    if ((surf_caps.maxImageCount > 0) && (app->image_count > surf_caps.maxImageCount)) {
        app->image_count = surf_caps.maxImageCount;
    }
    SDL_Log("Surface capabilities - minImageCount: %d", app->image_count);

    /* --- Surface Format Selection --- */
    uint32_t format_count = 0;
    ENGINE_xzlog("vkGetPhysicalDeviceSurfaceFormatsKHR");
    check_vk(vkGetPhysicalDeviceSurfaceFormatsKHR(app->physical_device, app->surface, &format_count, NULL), "format count");
    std::vector<VkSurfaceFormatKHR> formats(format_count);
    ENGINE_xzlog("vkGetPhysicalDeviceSurfaceFormatsKHR");
    check_vk(vkGetPhysicalDeviceSurfaceFormatsKHR(app->physical_device, app->surface, &format_count, formats.data()), "formats");
    SDL_Log("Supported Surface Formats (%d count):", format_count);
    for (uint32_t i = 0; i < format_count; i++) {
        SDL_Log("\t%d: Format: %d, ColorSpace: %d", i, formats[i].format, formats[i].colorSpace);
    }

    app->surface_format = formats[0];
    VkFormat desired_formats[] = {
        VK_FORMAT_B8G8R8A8_UNORM,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_FORMAT_A8B8G8R8_UNORM_PACK32
    };

    for (int i = 0; i < 3; i++) {
        for (uint32_t j = 0; j < format_count; j++) {
            if (desired_formats[i] == formats[j].format) {
                app->surface_format = formats[j];
                const char* format_names[] = { "B8G8R8A8_UNORM", "R8G8B8A8_UNORM", "A8B8G8R8_UNORM" };
                SDL_Log("Selected Surface Format: %s", format_names[i]);
                i = 3;
                break;
            }
        }
    }

    /* --- Present Mode Selection --- */
    uint32_t mode_count = 0;
    ENGINE_xzlog("vkGetPhysicalDeviceSurfacePresentModesKHR");
    check_vk(vkGetPhysicalDeviceSurfacePresentModesKHR(app->physical_device, app->surface, &mode_count, NULL), "mode count");
    std::vector<VkPresentModeKHR> modes(mode_count);
    ENGINE_xzlog("vkGetPhysicalDeviceSurfacePresentModesKHR");
    check_vk(vkGetPhysicalDeviceSurfacePresentModesKHR(app->physical_device, app->surface, &mode_count, modes.data()), "modes");
    VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;

    /* --- Logical Device --- */
    float priority = 1.0f;
    VkDeviceQueueCreateInfo queue_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = app->graphics_family,
        .queueCount = 1,
        .pQueuePriorities = &priority,
    };

    std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };
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
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queue_info,
        .enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()),
        .ppEnabledExtensionNames = deviceExtensions.data(),
    };

    ENGINE_xzlog("vkCreateDevice");
    check_vk(vkCreateDevice(app->physical_device, &device_info, NULL, &app->device), "vkCreateDevice");
    ENGINE_xzlog("vkGetDeviceQueue");
    vkGetDeviceQueue(app->device, app->graphics_family, 0, &app->graphics_queue);

    /* --- Swapchain --- */
    VkCompositeAlphaFlagBitsKHR alphaMode = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    if (surf_caps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR) {
        alphaMode = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    } else if (surf_caps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR) {
        alphaMode = VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR;
    } else if (surf_caps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR) {
        alphaMode = VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR;
    }

    VkImageUsageFlags imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if (surf_caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) {
        imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    }
    if (surf_caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) {
        imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    }

    VkSwapchainCreateInfoKHR swapchain_info = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = app->surface,
        .minImageCount = app->image_count,
        .imageFormat = app->surface_format.format,
        .imageColorSpace = app->surface_format.colorSpace,
        .imageExtent = app->swapchain_extent,
        .imageArrayLayers = 1,
        .imageUsage = imageUsage,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .preTransform = surf_caps.currentTransform,
        .compositeAlpha = alphaMode,
        .presentMode = present_mode,
        .clipped = VK_TRUE,
    };

    ENGINE_xzlog("vkCreateSwapchainKHR");
    check_vk(vkCreateSwapchainKHR(app->device, &swapchain_info, NULL, &app->swapchain), "vkCreateSwapchainKHR");

    /* --- Swapchain Images & Image Views --- */
    ENGINE_xzlog("vkGetSwapchainImagesKHR");
    check_vk(vkGetSwapchainImagesKHR(app->device, app->swapchain, &app->image_count, NULL), "image count");
    app->images.resize(app->image_count);
    app->image_views.resize(app->image_count);
    ENGINE_xzlog("vkGetSwapchainImagesKHR");
    check_vk(vkGetSwapchainImagesKHR(app->device, app->swapchain, &app->image_count, app->images.data()), "images");

    for (uint32_t i = 0; i < app->image_count; i++) {
        VkImageViewCreateInfo view_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = app->images[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = app->surface_format.format,
            .components = {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A},
            .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
        };
        ENGINE_xzlog("vkCreateImageView");
        check_vk(vkCreateImageView(app->device, &view_info, NULL, &app->image_views[i]), "vkCreateImageView");
    }

    SDL_Log("<SwapChain>:");
    SDL_Log("\tSwapchain ImageSize: %u x %u", app->swapchain_extent.width, app->swapchain_extent.height);
    SDL_Log("\tSwapchain ImageCount: %d", app->image_count);
    SDL_Log("\tSwapchain Present Mode: %d", present_mode);

#ifdef TARGET_OS_IPHONE
    #define SHADER_OUTPUT_DIR ""
#endif
    /* --- Shaders --- */
    size_t vert_size, frag_size;
    uint32_t* vert_spv = load_shader_file(SHADER_OUTPUT_DIR "vert.spv", &vert_size);
    uint32_t* frag_spv = load_shader_file(SHADER_OUTPUT_DIR "frag.spv", &frag_size);
    if (!vert_spv || !frag_spv) return;

    VkShaderModuleCreateInfo vert_info = {.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, .codeSize = vert_size, .pCode = vert_spv};
    VkShaderModuleCreateInfo frag_info = {.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, .codeSize = frag_size, .pCode = frag_spv};

    VkShaderModule vert_shader, frag_shader;
    ENGINE_xzlog("vkCreateShaderModule");
    check_vk(vkCreateShaderModule(app->device, &vert_info, NULL, &vert_shader), "vert shader");
    ENGINE_xzlog("vkCreateShaderModule");
    check_vk(vkCreateShaderModule(app->device, &frag_info, NULL, &frag_shader), "frag shader");

    /* --- Pipeline Layout --- */
    VkPipelineLayoutCreateInfo layout_info = {.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    ENGINE_xzlog("vkCreatePipelineLayout");
    check_vk(vkCreatePipelineLayout(app->device, &layout_info, NULL, &app->pipeline_layout), "vkCreatePipelineLayout");

    /* --- Graphics Pipeline --- */
    VkPipelineShaderStageCreateInfo stages[2] = {
        {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_VERTEX_BIT, .module = vert_shader, .pName = "main"},
        {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_FRAGMENT_BIT, .module = frag_shader, .pName = "main"},
    };

    /* --- Vertex Input --- */
    VkVertexInputBindingDescription binding_desc = {
        .binding   = 0,
        .stride    = sizeof(Vertex),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    };
    VkVertexInputAttributeDescription attr_descs[1] = {
        {.location = 0, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(Vertex, position)},
    };
    VkPipelineVertexInputStateCreateInfo vertex_input = {
        .sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount   = 1,
        .pVertexBindingDescriptions      = &binding_desc,
        .vertexAttributeDescriptionCount = 1,
        .pVertexAttributeDescriptions    = attr_descs,
    };

    VkPipelineInputAssemblyStateCreateInfo input_assembly = {.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};

    VkViewport viewport = {.x = 0, .y = 0, .width = (float)app->swapchain_extent.width, .height = (float)app->swapchain_extent.height, .minDepth = 0.0f, .maxDepth = 1.0f};
    VkRect2D scissor = {.offset = {0, 0}, .extent = app->swapchain_extent};
    VkPipelineViewportStateCreateInfo viewport_state = {.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, .viewportCount = 1, .pViewports = &viewport, .scissorCount = 1, .pScissors = &scissor};

    VkPipelineRasterizationStateCreateInfo rasterizer = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, .polygonMode = VK_POLYGON_MODE_FILL, .cullMode = VK_CULL_MODE_BACK_BIT, .frontFace = VK_FRONT_FACE_CLOCKWISE, .lineWidth = 1.0f};
    VkPipelineMultisampleStateCreateInfo multisampling = {.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT};

    VkPipelineColorBlendAttachmentState color_blend_attachment = {.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT};
    VkPipelineColorBlendStateCreateInfo color_blending = {.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, .attachmentCount = 1, .pAttachments = &color_blend_attachment};

    VkPipelineRenderingCreateInfo pipeline_rendering_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &app->surface_format.format,
    };

    VkGraphicsPipelineCreateInfo pipeline_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &pipeline_rendering_info,
        .stageCount = 2,
        .pStages = stages,
        .pVertexInputState = &vertex_input,
        .pInputAssemblyState = &input_assembly,
        .pViewportState = &viewport_state,
        .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisampling,
        .pColorBlendState = &color_blending,
        .layout = app->pipeline_layout,
    };

    ENGINE_xzlog("vkCreateGraphicsPipelines");
    check_vk(vkCreateGraphicsPipelines(app->device, VK_NULL_HANDLE, 1, &pipeline_info, NULL, &app->pipeline), "vkCreateGraphicsPipelines");

    /* --- Command Pool & Buffers --- */
    VkCommandPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = app->graphics_family,
    };
    ENGINE_xzlog("vkCreateCommandPool");
    check_vk(vkCreateCommandPool(app->device, &pool_info, NULL, &app->command_pool), "vkCreateCommandPool");

    /* --- Synchronization --- */
    VkSemaphoreCreateInfo sem_info = {.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkFenceCreateInfo fence_info = {.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags = VK_FENCE_CREATE_SIGNALED_BIT};

    ENGINE_xzlog("vkCreateSemaphore");
    check_vk(vkCreateSemaphore(app->device, &sem_info, NULL, &app->image_available_sem), "image_available");
    ENGINE_xzlog("vkCreateSemaphore");
    check_vk(vkCreateSemaphore(app->device, &sem_info, NULL, &app->render_finished_sem), "render_finished");
    ENGINE_xzlog("vkCreateFence");
    check_vk(vkCreateFence(app->device, &fence_info, NULL, &app->in_flight_fence), "in_flight");

    app->command_buffers.resize(app->image_count);
    VkCommandBufferAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = app->command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = app->image_count,
    };
    ENGINE_xzlog("vkAllocateCommandBuffers");
    check_vk(vkAllocateCommandBuffers(app->device, &alloc_info, app->command_buffers.data()), "vkAllocateCommandBuffers");

    /* --- Allocate imgui command buffers --- */
    app->imgui_command_buffers.resize(app->image_count);
    VkCommandBufferAllocateInfo imgui_alloc_info = alloc_info;
    imgui_alloc_info.commandBufferCount = app->image_count;
    check_vk(vkAllocateCommandBuffers(app->device, &imgui_alloc_info,
                                      app->imgui_command_buffers.data()),
             "imgui cmd bufs");

    create_vertex_buffer(app);
    create_index_buffer(app);

    /* --- Record Command Buffers --- */
    for (uint32_t i = 0; i < app->image_count; i++) {
        VkCommandBufferBeginInfo begin_info = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        ENGINE_xzlog("vkBeginCommandBuffer");
        check_vk(vkBeginCommandBuffer(app->command_buffers[i], &begin_info), "vkBeginCommandBuffer");

        VkImageMemoryBarrier barrier_to_render = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = app->images[i],
            .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
        };
        vkCmdPipelineBarrier(app->command_buffers[i],
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            0, 0, nullptr, 0, nullptr, 1, &barrier_to_render);

        VkRenderingAttachmentInfo color_attachment = {
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .imageView = app->image_views[i],
            .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .clearValue = {{0.0f, 0.0f, 0.0f, 1.0f}},
        };
        VkRenderingInfo rendering_info = {
            .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .renderArea = {{0, 0}, app->swapchain_extent},
            .layerCount = 1,
            .colorAttachmentCount = 1,
            .pColorAttachments = &color_attachment,
        };

        ENGINE_xzlog("vkCmdBeginRendering");
        vkCmdBeginRendering(app->command_buffers[i], &rendering_info);
        ENGINE_xzlog("vkCmdBindPipeline");
        vkCmdBindPipeline(app->command_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, app->pipeline);

        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(app->command_buffers[i], 0, 1, &app->vertex_buffer, offsets);
        vkCmdBindIndexBuffer(app->command_buffers[i], app->index_buffer, 0, VK_INDEX_TYPE_UINT32);

        ENGINE_xzlog("vkCmdDrawIndexed");
        vkCmdDrawIndexed(app->command_buffers[i], 3, 1, 0, 0, 0);

        ENGINE_xzlog("vkCmdEndRendering");
        vkCmdEndRendering(app->command_buffers[i]);

        // NOTE: PRESENT_SRC barrier is in the imgui cmd buf
        ENGINE_xzlog("vkEndCommandBuffer");
        check_vk(vkEndCommandBuffer(app->command_buffers[i]), "vkEndCommandBuffer");
    }

    free(vert_spv);
    free(frag_spv);
    vkDestroyShaderModule(app->device, vert_shader, NULL);
    vkDestroyShaderModule(app->device, frag_shader, NULL);

    /* ------------------------------------------------------------------ */
    /* ImGui init                                                           */
    /* ------------------------------------------------------------------ */
    VkDescriptorPoolSize pool_sizes[] = {
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, IMGUI_IMPL_VULKAN_MINIMUM_SAMPLED_IMAGE_POOL_SIZE },
        { VK_DESCRIPTOR_TYPE_SAMPLER,       IMGUI_IMPL_VULKAN_MINIMUM_SAMPLER_POOL_SIZE },
    };
    VkDescriptorPoolCreateInfo dp_info = {
        .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets       = IMGUI_IMPL_VULKAN_MINIMUM_SAMPLED_IMAGE_POOL_SIZE +
                         IMGUI_IMPL_VULKAN_MINIMUM_SAMPLER_POOL_SIZE,
        .poolSizeCount = 2,
        .pPoolSizes    = pool_sizes,
    };
    check_vk(vkCreateDescriptorPool(app->device, &dp_info, nullptr,
                                    &app->imgui_descriptor_pool),
             "imgui descriptor pool");

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
        .sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount    = 1,
        .pColorAttachmentFormats = &app->surface_format.format,
    };

    ImGui_ImplVulkan_Init(&imgui_vk_info);
}

void cleanup_vulkan(AppState* app) {
    ENGINE_xzlog("vkDeviceWaitIdle");
    vkDeviceWaitIdle(app->device);

    // --- ImGui cleanup ---
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    vkDestroyDescriptorPool(app->device, app->imgui_descriptor_pool, nullptr);

    // --- Vertex buffer cleanup ---
    vkDestroyBuffer(app->device, app->vertex_buffer, nullptr);
    vkFreeMemory(app->device, app->vertex_buffer_memory, nullptr);

    // --- Index buffer cleanup ---
    vkDestroyBuffer(app->device, app->index_buffer, nullptr);
    vkFreeMemory(app->device, app->index_buffer_memory, nullptr);

    ENGINE_xzlog("vkDestroySemaphore");
    vkDestroySemaphore(app->device, app->image_available_sem, NULL);
    ENGINE_xzlog("vkDestroySemaphore");
    vkDestroySemaphore(app->device, app->render_finished_sem, NULL);
    ENGINE_xzlog("vkDestroyFence");
    vkDestroyFence(app->device, app->in_flight_fence, NULL);
    ENGINE_xzlog("vkDestroyCommandPool");
    vkDestroyCommandPool(app->device, app->command_pool, NULL);
    ENGINE_xzlog("vkDestroyPipeline");
    vkDestroyPipeline(app->device, app->pipeline, NULL);
    ENGINE_xzlog("vkDestroyPipelineLayout");
    vkDestroyPipelineLayout(app->device, app->pipeline_layout, NULL);
    for (auto& image_view : app->image_views) {
        ENGINE_xzlog("vkDestroyImageView");
        vkDestroyImageView(app->device, image_view, NULL);
    }
    ENGINE_xzlog("vkDestroySwapchainKHR");
    vkDestroySwapchainKHR(app->device, app->swapchain, NULL);
    ENGINE_xzlog("vkDestroyDevice");
    vkDestroyDevice(app->device, NULL);
    ENGINE_xzlog("vkDestroySurfaceKHR");
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
        ImGui::Begin("Hello Triangle", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::Text("Press T key to toggle GUI");
        ImGui::Separator();
        ImGui::ColorEdit3("Clear color", &app->clear_r);
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
    check_vk(vkAcquireNextImageKHR(app->device, app->swapchain, UINT64_MAX, app->image_available_sem, VK_NULL_HANDLE, &image_index), "vkAcquireNextImageKHR");

    update_gui(app);
    record_imgui_command_buffers(app, image_index);

    VkCommandBuffer cmd_bufs[] = {
        app->command_buffers[image_index],
        app->imgui_command_buffers[image_index],
    };

    VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &app->image_available_sem,
        .pWaitDstStageMask = wait_stages,
        .commandBufferCount = 2,
        .pCommandBuffers = cmd_bufs,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &app->render_finished_sem,
    };
    ENGINE_xziter_log("vkQueueSubmit");
    check_vk(vkQueueSubmit(app->graphics_queue, 1, &submit_info, app->in_flight_fence), "vkQueueSubmit");

    VkPresentInfoKHR present_info = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &app->render_finished_sem,
        .swapchainCount = 1,
        .pSwapchains = &app->swapchain,
        .pImageIndices = &image_index,
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
    app->window = SDL_CreateWindow("Part 06 - Index Buffer", WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_VULKAN);
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

    if (event->type == SDL_EVENT_KEY_DOWN &&
        event->key.scancode == SDL_SCANCODE_T) {
        ((AppState*)appstate)->show_gui = !((AppState*)appstate)->show_gui;
    }

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