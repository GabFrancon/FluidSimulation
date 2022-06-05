#include "Vulkan/vk_engine.h"

int main() {
    VulkanEngine engine;

    try {
        engine.init();
        engine.run();
        engine.cleanup();
    }
    catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
