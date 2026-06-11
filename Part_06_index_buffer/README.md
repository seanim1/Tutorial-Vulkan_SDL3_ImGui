
## Updated:
main.cpp


## Recap:

the only differences are VK_BUFFER_USAGE_INDEX_BUFFER_BIT instead of vertex, and at draw time you call vkCmdBindIndexBuffer + vkCmdDrawIndexed instead of just vkCmdDraw