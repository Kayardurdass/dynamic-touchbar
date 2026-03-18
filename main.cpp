#include "Backend.hpp"
#include "Vulkan.hpp"

int main() {
  Backend backend;
  Vulkan vulkan_bak;

  backend.add_dma_buff(vulkan_bak.dma_buf_fd, vulkan_bak.pitch);
  auto start = std::chrono::high_resolution_clock::now();

  while (true) {
    auto begin = std::chrono::high_resolution_clock::now();
    backend.Render(vulkan_bak);
    auto end = std::chrono::high_resolution_clock::now();
    float dt =
        std::chrono::duration<float, std::chrono::seconds::period>(end - begin)
            .count();
    std::cout << "FPS: " << 1.0 / dt << "\n";
  }
}
