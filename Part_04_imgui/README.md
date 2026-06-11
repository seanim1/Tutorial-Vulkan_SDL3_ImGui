# IMGUI integration

## Updated:
main.cpp

## Blog?
I was running into segfault in 
ImGui_ImplVulkan_Init -> ImGui_ImplVulkan_CreateDeviceObjects -> ImGui_ImplVulkan_CreateSamplerDS
On the second call of vkUpdateDescriptorSets, I was Segfaulting on the second call of this because I only had 1 descriptorCount for a pool combined image sampler
```bash
Segmentation fault         (core dumped) ./build/Part_04_imgui/04_imgui
```
It turns out that I wasn't allocating big enough descriptor pool
I did not know you could do the following to find the required descriptor pool size for imgui
```bash
grep -n "IMGUI_IMPL_VULKAN_MINIMUM" ~/repo/Tutorial-Vulkan_SDL3_ImGui/third_party/imgui/backends/imgui_impl_vulkan.h
81:#define IMGUI_IMPL_VULKAN_MINIMUM_SAMPLED_IMAGE_POOL_SIZE   (8)     // Minimum per atlas
```
//              - When creating your own descriptor pool (instead of letting backend creates its own):
//                - Before: need at least IMGUI_IMPL_VULKAN_MINIMUM_IMAGE_SAMPLER_POOL_SIZE descriptors of type VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER.
//                - After:  need at least IMGUI_IMPL_VULKAN_MINIMUM_SAMPLED_IMAGE_POOL_SIZE descriptors of type VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE.
//                                      + IMGUI_IMPL_VULKAN_MINIMUM_SAMPLER_POOL_SIZE       descriptors of type VK_DESCRIPTOR_TYPE_SAMPLER.
