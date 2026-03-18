
#pragma once

#include <VkBootstrap.h>
#include <iostream>
#include <vulkan/vulkan_core.h>
#define MAX_FRAMES_IN_FLIGHT 1

struct UniformBufferObject {
  float time;
};

class Vulkan {
public:
  Vulkan();
  ~Vulkan();
  int dma_buf_fd;
  uint32_t pitch;
  void render_frame();

private:
  VkQueue graphics_queue;
  VkQueue present_queue;
  std::vector<VkImage> swapchain_images;
  std::vector<VkImageView> swapchain_image_views;
  std::vector<VkFramebuffer> framebuffers;
  VkRenderPass render_pass;
  VkPipelineLayout pipeline_layout;
  VkDescriptorSetLayout descriptor_set_layout;
  VkPipeline graphics_pipeline;
  VkCommandPool command_pool;
  std::vector<VkCommandBuffer> command_buffers;
  size_t current_frame = 0;
  vkb::Instance instance;
  vkb::InstanceDispatchTable inst_disp;
  vkb::Device device;
  vkb::DispatchTable disp;
  VkImage frame;
  VkImageView frame_view;
  VkDeviceMemory memory;
  VkFramebuffer framebuffer;
  VkFence render_fence;
  VkSemaphore render_semaphore;

  VkBuffer indexBuffer;
  VkDeviceMemory indexBufferMemory;

  VkBuffer uniformBuffers;
  VkDeviceMemory uniformBuffersMemory;
  void *uniformBuffersMapped;
  VkDescriptorPool descriptorPool;
  VkDescriptorSet descriptor_sets;

  bool create_graphics_pipeline();
  bool create_render_pass();
  bool get_queues();
  bool create_swapchain();
  VkShaderModule createShaderModule(const std::vector<char> &code);
  bool create_sync_objects();
  bool create_command_buffers();
  bool create_command_pool();
  bool create_framebuffers();
  bool init_vulkan();
  bool create_descriptor_set_layout();
  bool create_uniform_buffers();
  void create_buffer(VkDeviceSize size, VkBufferUsageFlags usage,
                     VkMemoryPropertyFlags properties, VkBuffer &buffer,
                     VkDeviceMemory &bufferMemory);
  uint32_t find_memory_type(uint32_t type_filter,
                            VkMemoryPropertyFlags properties);
  bool create_descripto_pools();
  bool create_descripto_sets();
};
