#include "Vulkan.hpp"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <istream>

uint32_t Vulkan::find_memory_type(
    uint32_t type_filter, VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties mem_properties;

    vkGetPhysicalDeviceMemoryProperties(
        device.physical_device, &mem_properties);

    for (uint32_t i = 0; i < mem_properties.memoryTypeCount; i++) {
        bool supported = (type_filter & (1 << i));

        bool has_properties
            = (mem_properties.memoryTypes[i].propertyFlags & properties)
            == properties;

        if (supported && has_properties)
            return i;
    }

    throw std::runtime_error("Failed to find suitable memory type");
}

void Vulkan::create_buffer(VkDeviceSize size, VkBufferUsageFlags usage,
    VkMemoryPropertyFlags properties, VkBuffer &buffer,
    VkDeviceMemory &bufferMemory)
{
    VkBufferCreateInfo bufferInfo {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to create buffer!");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex
        = find_memory_type(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory)
        != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate buffer memory!");
    }

    vkBindBufferMemory(device, buffer, bufferMemory, 0);
}

bool Vulkan::init_vulkan()
{
    vkb::InstanceBuilder builder;
    auto inst_ret = builder.set_app_name("Example Vulkan Application")
                        .request_validation_layers()
                        .enable_extension(
                            VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME)
                        .use_default_debug_messenger()
                        .build();
    if (!inst_ret) {
        std::cerr << "Failed to create Vulkan instance. Error: "
                  << inst_ret.error().message() << "\n";
        return false;
    }
    vkb::Instance vkb_inst = inst_ret.value();
    instance = vkb_inst;

    vkb::PhysicalDeviceSelector selector { vkb_inst };
    auto phys_ret
        = selector.defer_surface_initialization()
              .add_required_extension(VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME)
              .add_required_extension(VK_KHR_BIND_MEMORY_2_EXTENSION_NAME)
              .add_required_extension(
                  VK_KHR_SAMPLER_YCBCR_CONVERSION_EXTENSION_NAME)
              .add_required_extension(VK_KHR_MAINTENANCE1_EXTENSION_NAME)
              .add_required_extension(
                  VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME)
              .add_required_extension(VK_KHR_IMAGE_FORMAT_LIST_EXTENSION_NAME)
              .add_required_extension(VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME)
              .add_required_extension(
                  VK_EXT_EXTERNAL_MEMORY_DMA_BUF_EXTENSION_NAME)
              .add_required_extension(
                  VK_EXT_IMAGE_DRM_FORMAT_MODIFIER_EXTENSION_NAME)
              .select();
    if (!phys_ret) {
        std::cerr << "failed to create physical device\n";
        return 1;
    }

    vkb::DeviceBuilder device_builder { phys_ret.value() };
    // automatically propagate needed data from instance & physical device
    auto dev_ret = device_builder.build();
    if (!dev_ret) {
        std::cerr << "Failed to create Vulkan device. Error: "
                  << dev_ret.error().message() << "\n";
        return false;
    }
    vkb::Device vkb_device = dev_ret.value();

    // Get the VkDevice handle used in the rest of a vulkan application
    device = vkb_device;

    VkExternalMemoryImageCreateInfo info {};
    info.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
    info.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;

    VkImageCreateInfo image_info {};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.pNext = &info;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.format = VK_FORMAT_B8G8R8A8_UNORM;
    image_info.extent.width = 64;
    image_info.extent.height = 2048;
    image_info.extent.depth = 1;
    image_info.mipLevels = 1;
    image_info.arrayLayers = 1;
    image_info.tiling = VK_IMAGE_TILING_LINEAR;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
        | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    if (vkCreateImage(device, &image_info, nullptr, &frame) != VK_SUCCESS) {
        std::cerr << "failed to create render image\n";
        return 1;
    }

    VkMemoryRequirements mem_req;
    vkGetImageMemoryRequirements(device, frame, &mem_req);
    VkExportMemoryAllocateInfo export_info {};
    export_info.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO;
    export_info.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;
    VkMemoryAllocateInfo alloc_info {};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.pNext = &export_info;

    alloc_info.allocationSize = mem_req.size;
    alloc_info.memoryTypeIndex = find_memory_type(
        mem_req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    vkAllocateMemory(device, &alloc_info, nullptr, &memory);
    vkBindImageMemory(device, frame, memory, 0);
    VkMemoryGetFdInfoKHR fd_info {};
    fd_info.sType = VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR;
    fd_info.memory = memory;
    fd_info.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;

    PFN_vkGetMemoryFdKHR vkGetMemoryFdKHR_func;
    vkGetMemoryFdKHR_func
        = (PFN_vkGetMemoryFdKHR)vkGetDeviceProcAddr(device, "vkGetMemoryFdKHR");

    if (!vkGetMemoryFdKHR_func)
        throw std::runtime_error("Failed to load vkGetMemoryFdKHR");
    vkGetMemoryFdKHR_func(device, &fd_info, &dma_buf_fd);
    VkImageSubresource subresource {};
    subresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

    VkSubresourceLayout layout;

    vkGetImageSubresourceLayout(device, frame, &subresource, &layout);
    pitch = layout.rowPitch;

    VkImageViewCreateInfo view_info {};
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image = frame;
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_info.format = VK_FORMAT_B8G8R8A8_UNORM;

    view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = 1;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = 1;

    vkCreateImageView(device, &view_info, nullptr, &frame_view);
    disp = device.make_table();

    get_queues();
    create_render_pass();
    create_descriptor_set_layout();
    create_graphics_pipeline();

    VkFramebufferCreateInfo fb_info {};
    fb_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fb_info.renderPass = render_pass;
    fb_info.pAttachments = &frame_view;
    fb_info.width = 64;
    fb_info.attachmentCount = 1;
    fb_info.height = 2048;
    fb_info.layers = 1;

    vkCreateFramebuffer(device, &fb_info, nullptr, &framebuffer);

    create_command_pool();
    create_uniform_buffers();
    create_descripto_pools();
    create_descripto_sets();
    create_command_buffers();
    create_sync_objects();
    return true;
}

bool Vulkan::create_sync_objects()
{
    VkSemaphoreCreateInfo semaphore_info = {};
    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fence_info = {};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    if (disp.createSemaphore(&semaphore_info, nullptr, &render_semaphore)
        != VK_SUCCESS) {
        std::cout << "failed to create sync objects\n";
        return -1; // failed to create synchronization objects for a frame
    }

    if (disp.createFence(&fence_info, nullptr, &render_fence) != VK_SUCCESS) {
        std::cout << "failed to create sync objects\n";
        return -1; // failed to create synchronization objects for a frame
    }
    return 0;
}

bool Vulkan::create_descriptor_set_layout()
{
    VkDescriptorSetLayoutBinding uboLayoutBinding {};
    uboLayoutBinding.binding = 0;
    uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding.descriptorCount = 1;
    uboLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    VkDescriptorSetLayoutCreateInfo layoutInfo {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &uboLayoutBinding;

    if (vkCreateDescriptorSetLayout(
            device, &layoutInfo, nullptr, &descriptor_set_layout)
        != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor set layout!");
        return (false);
    }
    return true;
}

bool Vulkan::create_descripto_sets()
{
    VkDescriptorSetAllocateInfo allocInfo {};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
    allocInfo.pSetLayouts = &descriptor_set_layout;
    if (vkAllocateDescriptorSets(device, &allocInfo, &descriptor_sets)
        != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate descriptor sets!");
    }
    VkDescriptorBufferInfo bufferInfo {};
    bufferInfo.buffer = uniformBuffers;
    bufferInfo.offset = 0;
    bufferInfo.range = sizeof(UniformBufferObject);

    VkWriteDescriptorSet descriptorWrite {};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = descriptor_sets;
    descriptorWrite.dstBinding = 0;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pBufferInfo = &bufferInfo;

    vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);
    return true;
}

bool Vulkan::create_descripto_pools()
{
    VkDescriptorPoolSize poolSize {};
    poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSize.descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
    VkDescriptorPoolCreateInfo poolInfo {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool)
        != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor pool!");
    }
    return true;
}

bool Vulkan::create_uniform_buffers()
{
    VkDeviceSize bufferSize = sizeof(UniformBufferObject);

    create_buffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
            | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        uniformBuffers, uniformBuffersMemory);

    vkMapMemory(
        device, uniformBuffersMemory, 0, bufferSize, 0, &uniformBuffersMapped);
    return true;
}
std::vector<char> readFile(const std::string &filename)
{
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error("failed to open file!");
    }

    size_t file_size = (size_t)file.tellg();
    std::vector<char> buffer(file_size);

    file.seekg(0);
    file.read(buffer.data(), static_cast<std::streamsize>(file_size));

    file.close();

    return buffer;
}

VkShaderModule Vulkan::createShaderModule(const std::vector<char> &code)
{
    VkShaderModuleCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = code.size();
    create_info.pCode = reinterpret_cast<const uint32_t *>(code.data());

    VkShaderModule shaderModule;
    if (disp.createShaderModule(&create_info, nullptr, &shaderModule)
        != VK_SUCCESS) {
        return VK_NULL_HANDLE;
    }

    return shaderModule;
}

bool Vulkan::create_graphics_pipeline()
{
    auto vert_code = readFile("./shaders/triangle.vert.spv");
    auto frag_code = readFile("./shaders/triangle.frag.spv");

    VkShaderModule vert_module = createShaderModule(vert_code);
    VkShaderModule frag_module = createShaderModule(frag_code);
    if (vert_module == VK_NULL_HANDLE || frag_module == VK_NULL_HANDLE) {
        std::cout << "failed to create shader module\n";
        return -1;
    }

    VkPipelineShaderStageCreateInfo vert_stage_info = {};
    vert_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vert_stage_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vert_stage_info.module = vert_module;
    vert_stage_info.pName = "main";

    VkPipelineShaderStageCreateInfo frag_stage_info = {};
    frag_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    frag_stage_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    frag_stage_info.module = frag_module;
    frag_stage_info.pName = "main";

    VkPipelineShaderStageCreateInfo shader_stages[]
        = { vert_stage_info, frag_stage_info };

    VkPipelineVertexInputStateCreateInfo vertex_input_info = {};
    vertex_input_info.sType
        = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input_info.vertexBindingDescriptionCount = 0;
    vertex_input_info.vertexAttributeDescriptionCount = 0;

    VkPipelineInputAssemblyStateCreateInfo input_assembly = {};
    input_assembly.sType
        = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport = {};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)64;
    viewport.height = (float)2048;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor = {};
    scissor.offset = { 0, 0 };
    scissor.extent = (VkExtent2D) { 64, 2048 };

    VkPipelineViewportStateCreateInfo viewport_state = {};
    viewport_state.sType
        = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1;
    viewport_state.pViewports = &viewport;
    viewport_state.scissorCount = 1;
    viewport_state.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer = {};
    rasterizer.sType
        = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling = {};
    multisampling.sType
        = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT
        | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT
        | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo color_blending = {};
    color_blending.sType
        = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blending.logicOpEnable = VK_FALSE;
    color_blending.logicOp = VK_LOGIC_OP_COPY;
    color_blending.attachmentCount = 1;
    color_blending.pAttachments = &colorBlendAttachment;
    color_blending.blendConstants[0] = 0.0f;
    color_blending.blendConstants[1] = 0.0f;
    color_blending.blendConstants[2] = 0.0f;
    color_blending.blendConstants[3] = 0.0f;

    VkPipelineLayoutCreateInfo pipeline_layout_info = {};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.setLayoutCount = 1;
    pipeline_layout_info.pSetLayouts = &descriptor_set_layout;
    pipeline_layout_info.pushConstantRangeCount = 0;

    if (disp.createPipelineLayout(
            &pipeline_layout_info, nullptr, &pipeline_layout)
        != VK_SUCCESS) {
        std::cout << "failed to create pipeline layout\n";
        return -1; // failed to create pipeline layout
    }

    std::vector<VkDynamicState> dynamic_states
        = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

    VkPipelineDynamicStateCreateInfo dynamic_info = {};
    dynamic_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_info.dynamicStateCount
        = static_cast<uint32_t>(dynamic_states.size());
    dynamic_info.pDynamicStates = dynamic_states.data();

    VkGraphicsPipelineCreateInfo pipeline_info = {};
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.stageCount = 2;
    pipeline_info.pStages = shader_stages;
    pipeline_info.pVertexInputState = &vertex_input_info;
    pipeline_info.pInputAssemblyState = &input_assembly;
    pipeline_info.pViewportState = &viewport_state;
    pipeline_info.pRasterizationState = &rasterizer;
    pipeline_info.pMultisampleState = &multisampling;
    pipeline_info.pColorBlendState = &color_blending;
    pipeline_info.pDynamicState = &dynamic_info;
    pipeline_info.layout = pipeline_layout;
    pipeline_info.renderPass = render_pass;
    pipeline_info.subpass = 0;
    pipeline_info.basePipelineHandle = VK_NULL_HANDLE;

    if (disp.createGraphicsPipelines(
            VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &graphics_pipeline)
        != VK_SUCCESS) {
        std::cout << "failed to create pipline\n";
        return -1; // failed to create graphics pipeline
    }

    disp.destroyShaderModule(frag_module, nullptr);
    disp.destroyShaderModule(vert_module, nullptr);
    return 0;
}

bool Vulkan::create_command_pool()
{
    VkCommandPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.queueFamilyIndex
        = device.get_queue_index(vkb::QueueType::graphics).value();

    if (disp.createCommandPool(&pool_info, nullptr, &command_pool)
        != VK_SUCCESS) {
        std::cout << "failed to create command pool\n";
        return -1; // failed to create command pool
    }
    return 0;
}

bool Vulkan::create_command_buffers()
{
    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = command_pool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    if (disp.allocateCommandBuffers(&allocInfo, &command_buffer)
        != VK_SUCCESS) {
        return -1; // failed to allocate command buffers;
    }

    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (disp.beginCommandBuffer(command_buffer, &begin_info) != VK_SUCCESS) {
        return -1; // failed to begin recording command buffer
    }

    VkRenderPassBeginInfo render_pass_info = {};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_info.renderPass = render_pass;
    render_pass_info.framebuffer = framebuffer;
    render_pass_info.renderArea.offset = { 0, 0 };
    render_pass_info.renderArea.extent = (VkExtent2D) { 64, 2048 };
    VkClearValue clearColor { { { 0.0f, 0.0f, 0.0f, 1.0f } } };
    render_pass_info.clearValueCount = 1;
    render_pass_info.pClearValues = &clearColor;

    VkViewport viewport = {};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)64;
    viewport.height = (float)2048;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor = {};
    scissor.offset = { 0, 0 };
    scissor.extent = (VkExtent2D) { 64, 2048 };

    disp.cmdSetViewport(command_buffer, 0, 1, &viewport);
    disp.cmdSetScissor(command_buffer, 0, 1, &scissor);

    disp.cmdBeginRenderPass(
        command_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);

    disp.cmdBindPipeline(
        command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline);
    vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipeline_layout, 0, 1, &descriptor_sets, 0, nullptr);
    disp.cmdDraw(command_buffer, 6, 1, 0, 0);

    disp.cmdEndRenderPass(command_buffer);

    if (disp.endCommandBuffer(command_buffer) != VK_SUCCESS) {
        std::cout << "failed to record command buffer\n";
        return -1; // failed to record command buffer!
    }
    return 0;
}

bool Vulkan::create_render_pass()
{
    VkAttachmentDescription color_attachment = {};
    color_attachment.format = VK_FORMAT_B8G8R8A8_UNORM;
    color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attachment.finalLayout = VK_IMAGE_LAYOUT_GENERAL;

    VkAttachmentReference color_attachment_ref = {};
    color_attachment_ref.attachment = 0;
    color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment_ref;

    VkSubpassDependency dependency = {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
        | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo render_pass_info = {};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_info.attachmentCount = 1;
    render_pass_info.pAttachments = &color_attachment;
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass;
    render_pass_info.dependencyCount = 1;
    render_pass_info.pDependencies = &dependency;

    if (disp.createRenderPass(&render_pass_info, nullptr, &render_pass)
        != VK_SUCCESS) {
        std::cout << "failed to create render pass\n";
        return -1; // failed to create render pass!
    }
    return 0;
}

bool Vulkan::get_queues()
{
    auto gq = device.get_queue(vkb::QueueType::graphics);
    if (!gq.has_value()) {
        std::cout << "failed to get graphics queue: " << gq.error().message()
                  << "\n";
        return false;
    }
    graphics_queue = gq.value();

    return true;
}

void Vulkan::render_frame()
{
    static int frame = 0;
    static auto startTime = std::chrono::high_resolution_clock::now();
    disp.waitForFences(1, &render_fence, VK_TRUE, UINT64_MAX);
    disp.resetFences(1, &render_fence);

    auto currentTime = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float, std::chrono::seconds::period>(
        currentTime - startTime)
                     .count();
    UniformBufferObject ubo {};
    ubo.time = time;
    memcpy(uniformBuffersMapped, &ubo, sizeof(ubo));

    VkSubmitInfo submit {};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &command_buffer;

    if (disp.queueSubmit(graphics_queue, 1, &submit, render_fence)
        != VK_SUCCESS)
        throw std::runtime_error("queueSubmit failed");
    frame++;
    frame = std::max(frame, 0);
}

Vulkan::Vulkan(int width, int height)
    : width(width)
    , height(height)
{
    if (!init_vulkan())
        throw std::runtime_error("failed to init vulkan");
    std::cout << "Successfully initialized vulkan\n";
}

Vulkan::~Vulkan()
{
    disp.destroySemaphore(render_semaphore, nullptr);
    disp.destroyFence(render_fence, nullptr);

    disp.destroyCommandPool(command_pool, nullptr);

    disp.destroyFramebuffer(framebuffer, nullptr);
    vkDestroyBuffer(device, uniformBuffers, nullptr);
    vkFreeMemory(device, uniformBuffersMemory, nullptr);
    vkDestroyDescriptorPool(device, descriptorPool, nullptr);

    vkDestroyDescriptorSetLayout(device, descriptor_set_layout, nullptr);
    disp.destroyDescriptorSetLayout(descriptor_set_layout, nullptr);
    disp.destroyPipeline(graphics_pipeline, nullptr);
    disp.destroyPipelineLayout(pipeline_layout, nullptr);
    disp.destroyRenderPass(render_pass, nullptr);

    vkb::destroy_device(device);
    vkb::destroy_instance(instance);
}
