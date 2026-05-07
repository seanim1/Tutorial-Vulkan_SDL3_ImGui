#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_gpu.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600

uint8_t* load_shader_file(const char* filename, size_t* out_size) {
    FILE* f = fopen(filename, "rb");
    if (!f) {
        fprintf(stderr, "Failed to open shader: %s\n", filename);
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t* data = (uint8_t*)malloc(size);
    fread(data, 1, size, f);
    fclose(f);
    *out_size = size;
    return data;
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    printf("=== SDL3 GPU Triangle ===\n\n");

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "SDL_Init failed\n");
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "SDL3 GPU Triangle",
        WINDOW_WIDTH, WINDOW_HEIGHT,
        SDL_WINDOW_RESIZABLE
    );
    if (!window) {
        fprintf(stderr, "Window creation failed: %s\n", SDL_GetError());
        return 1;
    }
    printf("✓ Window created\n");

    /* --- GPU Device --- */
    SDL_GPUDevice* device = SDL_CreateGPUDevice(
        SDL_GPU_SHADERFORMAT_SPIRV,
        true,
        NULL
    );
    if (!device) {
        fprintf(stderr, "GPU device creation failed: %s\n", SDL_GetError());
        return 1;
    }
    printf("✓ GPU device created\n");

    /* --- Claim Window --- */
    if (!SDL_ClaimWindowForGPUDevice(device, window)) {
        fprintf(stderr, "Failed to claim window: %s\n", SDL_GetError());
        return 1;
    }
    printf("✓ Window claimed for GPU\n");

    /* --- Load Shaders --- */
    size_t vert_size, frag_size;
    uint8_t* vert_code = load_shader_file(SHADER_OUTPUT_DIR "/vert.spv", &vert_size);
    uint8_t* frag_code = load_shader_file(SHADER_OUTPUT_DIR "/frag.spv", &frag_size);

    if (!vert_code || !frag_code) {
        fprintf(stderr, "Failed to load shaders\n");
        return 1;
    }
    printf("✓ Shaders loaded from disk\n");

    /* --- Create Shader Modules --- */
    SDL_GPUShaderCreateInfo vert_info = {
        .code_size = vert_size,
        .code = vert_code,
        .entrypoint = "main",
        .format = SDL_GPU_SHADERFORMAT_SPIRV,
        .stage = SDL_GPU_SHADERSTAGE_VERTEX,
    };

    SDL_GPUShaderCreateInfo frag_info = {
        .code_size = frag_size,
        .code = frag_code,
        .entrypoint = "main",
        .format = SDL_GPU_SHADERFORMAT_SPIRV,
        .stage = SDL_GPU_SHADERSTAGE_FRAGMENT,
    };

    SDL_GPUShader* vertex_shader = SDL_CreateGPUShader(device, &vert_info);
    SDL_GPUShader* fragment_shader = SDL_CreateGPUShader(device, &frag_info);

    if (!vertex_shader || !fragment_shader) {
        fprintf(stderr, "Failed to create shaders: %s\n", SDL_GetError());
        return 1;
    }
    printf("✓ Shader modules created\n");

    /* --- Graphics Pipeline --- */
    SDL_GPUColorTargetDescription color_target = {
        .format = SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM,
        .blend_state = {
            .color_write_mask = SDL_GPU_COLORCOMPONENT_R | SDL_GPU_COLORCOMPONENT_G | SDL_GPU_COLORCOMPONENT_B | SDL_GPU_COLORCOMPONENT_A,
        },
    };

    SDL_GPUGraphicsPipelineCreateInfo pipeline_info = {
        .vertex_shader = vertex_shader,
        .fragment_shader = fragment_shader,
        .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
        .target_info = {
            .color_target_descriptions = &color_target,
            .num_color_targets = 1,
        },
    };

    SDL_GPUGraphicsPipeline* pipeline = SDL_CreateGPUGraphicsPipeline(device, &pipeline_info);
    if (!pipeline) {
        fprintf(stderr, "Failed to create pipeline: %s\n", SDL_GetError());
        return 1;
    }
    printf("✓ Graphics pipeline created\n");

    /* --- Main Loop --- */
    printf("\n=== Running (close window to exit) ===\n");
    bool running = true;
    SDL_Event event;

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
        }

        /* Acquire command buffer */
        SDL_GPUCommandBuffer* cmd_buf = SDL_AcquireGPUCommandBuffer(device);
        if (!cmd_buf) {
            fprintf(stderr, "Failed to acquire command buffer\n");
            break;
        }

        /* Get swapchain texture */
        SDL_GPUTexture* swapchain_tex = NULL;
        Uint32 width = 0, height = 0;
        if (!SDL_AcquireGPUSwapchainTexture(cmd_buf, window, &swapchain_tex, &width, &height)) {
            fprintf(stderr, "Failed to acquire swapchain texture\n");
            SDL_SubmitGPUCommandBuffer(cmd_buf);
            break;
        }

        /* Begin render pass */
        SDL_GPUColorTargetInfo color_info = {
            .texture = swapchain_tex,
            .clear_color = {0.0f, 0.0f, 0.0f, 1.0f},
            .load_op = SDL_GPU_LOADOP_CLEAR,
            .store_op = SDL_GPU_STOREOP_STORE,
        };

        SDL_GPURenderPass* render_pass = SDL_BeginGPURenderPass(cmd_buf, &color_info, 1, NULL);
        if (!render_pass) {
            fprintf(stderr, "Failed to begin render pass\n");
            SDL_SubmitGPUCommandBuffer(cmd_buf);
            break;
        }

        /* Bind pipeline and draw triangle */
        SDL_BindGPUGraphicsPipeline(render_pass, pipeline);
        SDL_DrawGPUPrimitives(render_pass, 3, 1, 0, 0);

        SDL_EndGPURenderPass(render_pass);

        /* Submit and present */
        if (!SDL_SubmitGPUCommandBuffer(cmd_buf)) {
            fprintf(stderr, "Failed to submit command buffer\n");
            break;
        }

        SDL_Delay(16);
    }

    /* --- Cleanup --- */
    printf("\n✓ Shutting down...\n");

    SDL_ReleaseGPUShader(device, vertex_shader);
    SDL_ReleaseGPUShader(device, fragment_shader);
    SDL_ReleaseGPUGraphicsPipeline(device, pipeline);
    SDL_ReleaseWindowFromGPUDevice(device, window);
    SDL_DestroyGPUDevice(device);
    SDL_DestroyWindow(window);
    SDL_Quit();

    free(vert_code);
    free(frag_code);

    printf("✓ Done!\n");
    return 0;
}