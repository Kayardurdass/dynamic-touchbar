#include "Backend.hpp"
#include "Vulkan.hpp"

int main()
{
    Backend backend;
    Vulkan vulkan_bak(backend.width, backend.height);

    std::cout << "fb" << vulkan_bak.framebuffer << "\n";
    backend.add_dma_buff(vulkan_bak);
    auto start = std::chrono::high_resolution_clock::now();

    while (true) {
        auto begin = std::chrono::high_resolution_clock::now();
        backend.Render(vulkan_bak);
        auto end = std::chrono::high_resolution_clock::now();
        float dt = std::chrono::duration<float, std::chrono::seconds::period>(
            end - begin)
                       .count();
        std::cout << "FPS: " << 1.0 / dt << "\n";
    }
}
