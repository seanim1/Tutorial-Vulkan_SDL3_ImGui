# IMGUI integration

## Updated:
main.cpp
shaders/shader.frag
shaders/shader.vert


## Recap:

Vertex struct with position data on the CPU

vkCreateBuffer + vkAllocateMemory + vkBindBufferMemory — the three steps to get a GPU buffer

find_memory_type — querying the GPU for the right memory heap

HOST_VISIBLE | HOST_COHERENT — memory the CPU can write directly into without needing an explicit flush

vkMapMemory / memcpy / vkUnmapMemory — uploading the data

VkVertexInputBindingDescription + VkVertexInputAttributeDescription — telling the pipeline how to read the buffer

vkCmdBindVertexBuffers — binding it at draw time

