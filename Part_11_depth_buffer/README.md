
## User Guide:
Try moving sliders on the GUI

changing VK_CULL_MODE_BACK_BIT to VK_CULL_MODE_BACK_BIT will no longer cause faces bleeding

## Updated:
main.cpp


## New:
DEPTH_FORMAT constant added at the top
3 new AppState members: depth_image, depth_image_memory, depth_image_view
New create_depth_buffer function — creates VK_FORMAT_D32_SFLOAT image with DEVICE_LOCAL memory and an image view with VK_IMAGE_ASPECT_DEPTH_BIT
create_depth_buffer called in init_vulkan after create_texture
VkPipelineDepthStencilStateCreateInfo added to pipeline with depthTestEnable, depthWriteEnable, VK_COMPARE_OP_LESS
depthAttachmentFormat added to VkPipelineRenderingCreateInfo
pDepthStencilState added to VkGraphicsPipelineCreateInfo
depth_attachment added to command buffer recording with clearValue = {1.0f, 0} (clear to far)
pDepthAttachment added to VkRenderingInfo