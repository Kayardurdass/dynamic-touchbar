#include "Backend.hpp"

#include "Vulkan.hpp"
#include "sys/mman.h"

uint32_t find_prop_id(
    int fd, uint32_t handle, uint32_t obj_type, const std::string &name)
{
    std::vector<uint32_t> ids;
    std::vector<uint64_t> vals;

    drmModeObjectPropertiesPtr props
        = drmModeObjectGetProperties(fd, handle, obj_type);

    if (props == nullptr)
        throw std::runtime_error("failed to get object properties");
    for (int i = 0; i < props->count_props; i++) {
        drmModePropertyPtr prop = drmModeGetProperty(fd, props->props[i]);
        if (prop != nullptr) {
            if (prop->name == name) {
                uint32_t id = prop->prop_id;
                drmModeFreeProperty(prop);
                drmModeFreeObjectProperties(props);
                return id;
            }
            drmModeFreeProperty(prop);
        }
    }
    drmModeFreeObjectProperties(props);
    throw std::runtime_error("property not found");
}

void Backend::try_open_card(int fd)
{
    if (drmSetClientCap(fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, true) == -1)
        throw std::runtime_error(
            "failed to set client capacity universal planes");
    if (drmSetClientCap(fd, DRM_CLIENT_CAP_ATOMIC, true) == -1)
        throw std::runtime_error("failed to set client capacity atomic");
    if (drmSetMaster(fd) == -1)
        throw std::runtime_error("Failed to set master card");
    drmModeRes *res = drmModeGetResources(fd);
    drmModeConnector *conn = NULL;
    drmModeCrtc *crtc = NULL;

    for (int i = 0; i < res->count_connectors; i++) {
        conn = drmModeGetConnector(fd, res->connectors[i]);

        if (conn == nullptr)
            throw std::runtime_error("No connector found");
        if (conn->connection != DRM_MODE_CONNECTED)
            throw std::runtime_error("No connected connectors found");
        if (conn->count_modes == 0)
            throw std::runtime_error("No modes found");
        if (conn->modes->vdisplay / conn->modes->hdisplay < 30)
            throw std::runtime_error("This is not a touchbar");
        break;
    }
    for (int i = 0; i < res->count_crtcs; i++) {
        crtc = drmModeGetCrtc(fd, res->crtcs[i]);

        if (crtc == nullptr)
            throw std::runtime_error("No crcts found");
        break;
    }
    this->mode = conn->modes[0];
    this->conn = conn;
    this->crtc = crtc;
    width = mode.vdisplay;
    height = mode.hdisplay;
}

void Backend::add_dma_buff(Vulkan &vulkan)
{
    drm_prime_handle prime = {};
    prime.fd = vulkan.dma_buf_fd;
    prime.flags = DRM_PRIME_CAP_IMPORT;

    if (drmIoctl(card, DRM_IOCTL_PRIME_FD_TO_HANDLE, &prime) < 0)
        throw std::runtime_error("PRIME import failed");

    uint32_t handle = prime.handle;

    uint32_t handles[4] = { handle };
    uint32_t pitches[4] = { vulkan.pitch };
    uint32_t offsets[4] = { 0 };

    uint32_t fb;

    if (drmModeAddFB2(card, mode.hdisplay, mode.vdisplay, DRM_FORMAT_XRGB8888,
            handles, pitches, offsets, &fb, 0)) {
        throw std::runtime_error("Failed to create framebuffer");
    }

    drmModePlaneResPtr plane_res = drmModeGetPlaneResources(card);
    for (uint32_t i = 0; i < plane_res->count_planes; i++) {
        drmModePlanePtr plane = drmModeGetPlane(card, plane_res->planes[i]);
        if (!plane)
            continue;
        if (plane->possible_crtcs & (1 << 0)) {
            plane_id = plane->plane_id;
            drmModeFreePlane(plane);
            break;
        }
        drmModeFreePlane(plane);
    }
    if (!plane_id)
        throw std::runtime_error("No compatible plane found");

    drmModeAtomicReqPtr atomic_req = drmModeAtomicAlloc();
    if (!atomic_req)
        throw std::runtime_error("failed to create atomic request");
    uint32_t mode_blob;
    drmModeCreatePropertyBlob(card, &mode, sizeof(mode), &mode_blob);
    std::vector<uint32_t> atomic_reqs;
    uint32_t conn_crtc_prop = find_prop_id(
        card, conn->connector_id, DRM_MODE_OBJECT_CONNECTOR, "CRTC_ID");
    uint32_t crtc_mode_prop
        = find_prop_id(card, crtc->crtc_id, DRM_MODE_OBJECT_CRTC, "MODE_ID");
    uint32_t crtc_active_prop
        = find_prop_id(card, crtc->crtc_id, DRM_MODE_OBJECT_CRTC, "ACTIVE");
    uint32_t plane_fb_prop
        = find_prop_id(card, plane_id, DRM_MODE_OBJECT_PLANE, "FB_ID");
    uint32_t plane_crtc_prop
        = find_prop_id(card, plane_id, DRM_MODE_OBJECT_PLANE, "CRTC_ID");
    uint32_t src_x
        = find_prop_id(card, plane_id, DRM_MODE_OBJECT_PLANE, "SRC_X");
    uint32_t src_y
        = find_prop_id(card, plane_id, DRM_MODE_OBJECT_PLANE, "SRC_Y");
    uint32_t src_w
        = find_prop_id(card, plane_id, DRM_MODE_OBJECT_PLANE, "SRC_W");
    uint32_t src_h
        = find_prop_id(card, plane_id, DRM_MODE_OBJECT_PLANE, "SRC_H");
    uint32_t crtc_x
        = find_prop_id(card, plane_id, DRM_MODE_OBJECT_PLANE, "CRTC_X");
    uint32_t crtc_y
        = find_prop_id(card, plane_id, DRM_MODE_OBJECT_PLANE, "CRTC_Y");
    uint32_t crtc_w
        = find_prop_id(card, plane_id, DRM_MODE_OBJECT_PLANE, "CRTC_W");
    uint32_t crtc_h
        = find_prop_id(card, plane_id, DRM_MODE_OBJECT_PLANE, "CRTC_H");

    drmModeAtomicAddProperty(
        atomic_req, conn->connector_id, conn_crtc_prop, crtc->crtc_id);
    drmModeAtomicAddProperty(
        atomic_req, crtc->crtc_id, crtc_mode_prop, mode_blob);
    drmModeAtomicAddProperty(atomic_req, crtc->crtc_id, crtc_active_prop, 1);
    drmModeAtomicAddProperty(atomic_req, plane_id, plane_fb_prop, fb);
    drmModeAtomicAddProperty(
        atomic_req, plane_id, plane_crtc_prop, crtc->crtc_id);
    drmModeAtomicAddProperty(atomic_req, plane_id, src_x, 0);
    drmModeAtomicAddProperty(atomic_req, plane_id, src_y, 0);
    drmModeAtomicAddProperty(
        atomic_req, plane_id, src_w, ((uint64_t)mode.hdisplay) << 16);
    drmModeAtomicAddProperty(
        atomic_req, plane_id, src_h, ((uint64_t)mode.vdisplay) << 16);
    drmModeAtomicAddProperty(atomic_req, plane_id, crtc_x, 0);
    drmModeAtomicAddProperty(atomic_req, plane_id, crtc_y, 0);
    drmModeAtomicAddProperty(atomic_req, plane_id, crtc_w, mode.hdisplay);
    drmModeAtomicAddProperty(atomic_req, plane_id, crtc_h, mode.vdisplay);
    if (drmModeAtomicCommit(
            card, atomic_req, DRM_MODE_ATOMIC_ALLOW_MODESET, nullptr)
        != 0) {
        throw std::runtime_error("Atomic commit failed");
    }

    drmModeAtomicFree(atomic_req);

    drmModeFreePlaneResources(plane_res);
    this->frame_buffer = fb;
    drmEventContext event_handler {};
    event_handler.version = DRM_EVENT_CONTEXT_VERSION;
    event_handler.page_flip_handler
        = [](int fd, unsigned int frame, unsigned int sec, unsigned int usec,
              void *data) {
              bool *flip_done = static_cast<bool *>(data);
              *flip_done = true;
              std::cout << "caca\n";
          };
}

void Backend::open_card()
{
    for (auto path : std::filesystem::directory_iterator("/dev/dri/")) {
        if (path.path().string().find("card") == path.path().string().npos)
            continue;
        try {
            int card = open(path.path().c_str(), O_RDWR);
            if (card == -1) {
                throw std::runtime_error(
                    std::string("Failed to open card: ") + path.path().c_str());
            }
            try_open_card(card);
            std::cout << "Suceeded to open card " << path.path().string()
                      << "\n";
            this->card = card;
            break;
        } catch (const std::runtime_error &e) {
            std::cerr << "Couldnt open card: " << path.path().string()
                      << ", because " << e.what() << "\n";
        }
    }
}

Backend::Backend()
{
    try {
        open_card();
    } catch (const std::runtime_error &e) {
        std::cerr << "Failed to open card " << e.what() << "\n";
    }
}

Backend::~Backend() { close(card); }

void Backend::Render(Vulkan &vulkan)
{
    static uint32_t plane_fb_prop
        = find_prop_id(card, plane_id, DRM_MODE_OBJECT_PLANE, "FB_ID");
    bool flip_pending = false;

    vulkan.render_frame();
    // drmModeClip clips[1];
    // clips[0].x1 = 0;
    // clips[0].y1 = 0;
    // clips[0].x2 = mode.hdisplay;
    // clips[0].y2 = mode.vdisplay;
    // drmModeDirtyFB(card, frame_buffer, clips, 1);

    if (!flip_pending) {
        int ret = drmModePageFlip(card, crtc->crtc_id, frame_buffer,
            DRM_MODE_PAGE_FLIP_EVENT, &flip_pending);
        if (ret != 0)
            flip_pending = true;
    }

    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(card, &fds);

    timeval wait;
    wait.tv_usec = 5000;
    wait.tv_sec = 0;
    select(card + 1, &fds, NULL, NULL, &wait);

    if (FD_ISSET(card, &fds)) {
        drmHandleEvent(card, &event_handler);
    }
    // drmModeAtomicReqPtr atomic_req = drmModeAtomicAlloc();
    // if (atomic_req == nullptr)
    //     throw std::runtime_error("failed to create atomic request");
    // std::vector<uint32_t> atomic_reqs;
    // drmModeAtomicAddProperty(atomic_req, plane_id, plane_fb_prop,
    // frame_buffer); if (drmModeAtomicCommit(
    //         card, atomic_req, DRM_MODE_ATOMIC_ALLOW_MODESET, nullptr)
    //     != 0) {
    //     throw std::runtime_error("Atomic commit failed");
    // }
    //
    // drmModeAtomicFree(atomic_req);
}
