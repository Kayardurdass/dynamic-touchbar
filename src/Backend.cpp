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
}

void Backend::add_dma_buff(uint32_t dma_buff_fd, uint32_t pitch)
{
    drm_prime_handle prime = {};
    prime.fd = dma_buff_fd;
    prime.flags = 0;

    if (drmIoctl(card, DRM_IOCTL_PRIME_FD_TO_HANDLE, &prime) < 0)
        throw std::runtime_error("PRIME import failed");

    uint32_t handle = prime.handle;

    uint32_t handles[4] = { handle };
    uint32_t pitches[4] = { pitch };
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

    vulkan.render_frame();
    drmModeAtomicReqPtr atomic_req = drmModeAtomicAlloc();
    if (atomic_req == nullptr)
        throw std::runtime_error("failed to create atomic request");
    std::vector<uint32_t> atomic_reqs;
    uint32_t plane_fb_prop
        = find_prop_id(card, plane_id, DRM_MODE_OBJECT_PLANE, "FB_ID");
    drmModeAtomicAddProperty(atomic_req, plane_id, plane_fb_prop, frame_buffer);
    if (drmModeAtomicCommit(
            card, atomic_req, DRM_MODE_ATOMIC_ALLOW_MODESET, nullptr)
        != 0) {
        throw std::runtime_error("Atomic commit failed");
    }

    drmModeAtomicFree(atomic_req);
}
