#pragma once

#include <drm/drm_fourcc.h>
#include <fcntl.h>
#include <libdrm/drm.h>
#include <libdrm/drm_fourcc.h>
#include <libdrm/drm_mode.h>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include <filesystem>
#include <iostream>
#include <vector>

#include "Vulkan.hpp"

struct DumbBuffer {
    uint32_t width;
    uint32_t height;
    uint64_t length;
    uint32_t format;
    uint32_t pitch;
    uint32_t handle;
};

class Backend {
public:
    Backend();
    void Render(Vulkan &vulkan);
    ~Backend();
    void add_dma_buff(uint32_t dma_buff_fd, uint32_t pitch);

private:
    void open_card();
    void try_open_card(int fd);
    int card;
    drmModeModeInfo mode;
    DumbBuffer db;
    uint32_t frame_buffer;
    void *fb_ptr;
    drmModeConnectorPtr conn;
    drmModeCrtcPtr crtc;
    uint32_t plane_id;
};
