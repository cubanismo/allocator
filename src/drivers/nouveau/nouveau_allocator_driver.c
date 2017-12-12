/*
 * Copyright (c) 2017 NVIDIA CORPORATION.  All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <allocator/driver.h>

#include <sys/stat.h>
#include <unistd.h>
#include <xf86drm.h>
#include <nouveau.h>
#include <nouveau_drm.h>
#include <nvif/class.h>
#include <nvif/cl0080.h>

#include <stdlib.h>
#include <string.h>
#include <limits.h>

/* Nouveau allocator driver and device definitions */
typedef struct {
    driver_t *pbase;
} nouveau_driver_t;

typedef struct {
    device_t base;

    struct nouveau_drm *drm;
    struct nouveau_device *dev;

    struct {
        uint64_t address_alignment;
        uint32_t pitch_alignment;
        uint32_t max_pitch;
        uint32_t max_dimensions;
    } properties;
} nouveau_device_t;

static int
nouveau_is_fd_supported(driver_t *driver, int fd)
{
    /* XXX: This only checks whether the given fd is a DRM device. Note that
     *      this doesn't necessarily means it's a nouveau device */
    switch (drmGetNodeTypeFromFd(fd)) {
    case DRM_NODE_PRIMARY:
    case DRM_NODE_CONTROL:
    case DRM_NODE_RENDER:
        return 1;
    default:
        return 0;
    }
}

static int
nouveau_device_get_capabilities(device_t *dev,
                                const assertion_t *assertion,
                                uint32_t num_uses,
                                const usage_t *uses,
                                uint32_t *num_sets,
                                capability_set_t **capability_sets)
{
    /* TODO */
    return -1;
}

static int
nouveau_device_get_assertion_hints(device_t *dev,
                                   uint32_t num_uses,
                                   const usage_t *uses,
                                   uint32_t *num_hints,
                                   assertion_hint_t **hints)
{
    /* TODO */
    return -1;
}

static void
nouveau_device_destroy_allocation(device_t *dev,
                                  allocation_t *allocation)
{
    /* TODO */
}

static int nouveau_device_create_allocation(device_t *dev,
                                  const assertion_t *assertion,
                                  const capability_set_t *capability_set,
                                  allocation_t **allocation)
{
    /* TODO */
    return -1;
}

static int nouveau_device_get_allocation_fd(device_t *dev,
                                 const allocation_t *allocation,
                                 int *fd)
{
    /* TODO */
    return -1;
}

static void nouveau_device_destroy(device_t *device)
{
    nouveau_device_t *nouveau_device = device->device_private;

    if (nouveau_device) {
        nouveau_device_del(&nouveau_device->dev);
        nouveau_drm_del(&nouveau_device->drm);
        free(device);
    }
}

static device_t *nouveau_device_create_from_fd(driver_t *driver, int fd)
{
    nouveau_device_t *nouveau_device;
    int ret;

    if (!driver->is_fd_supported(driver, fd)) {
        return NULL;
    }

    nouveau_device = (nouveau_device_t *)calloc(1, sizeof(nouveau_device_t));
    if (!nouveau_device) {
        return NULL;
    }

    ret = nouveau_drm_new(fd, &nouveau_device->drm);
    if (ret) {
        goto fail;
    }

    ret = nouveau_device_new(&nouveau_device->drm->client, NV_DEVICE,
                             &(struct nv_device_v0) {
                                 .device = ~0LL,
                             }, sizeof(struct nv_device_v0),
                             &nouveau_device->dev);
    if (ret) {
        goto fail;
    }

    nouveau_device->base.device_private = nouveau_device;
    nouveau_device->base.driver = driver;
    nouveau_device->base.destroy = &nouveau_device_destroy;
    nouveau_device->base.get_capabilities = &nouveau_device_get_capabilities;
    nouveau_device->base.get_assertion_hints = &nouveau_device_get_assertion_hints;
    nouveau_device->base.create_allocation = &nouveau_device_create_allocation;
    nouveau_device->base.destroy_allocation = &nouveau_device_destroy_allocation;
    nouveau_device->base.get_allocation_fd = &nouveau_device_get_allocation_fd;

    switch (nouveau_device->dev->chipset & ~0xf) {
    /* Fermi */
    case 0xc0:
    case 0xd0:
    /* Kepler */
    case 0xe0:
    case 0xf0:
    case 0x100:
    /* Maxwell */
    case 0x110:
    case 0x120:
    /* Pascal */
    case 0x130:
        nouveau_device->properties.address_alignment = 4096;
        nouveau_device->properties.pitch_alignment = 128;
        nouveau_device->properties.max_pitch = INT_MAX;
        nouveau_device->properties.max_dimensions = 16384;
        break;
    default:
        nouveau_device->properties.address_alignment = 4096;
        nouveau_device->properties.pitch_alignment = 64;
        nouveau_device->properties.max_pitch = INT_MAX;
        nouveau_device->properties.max_dimensions = 16384;
        break;
    }

    return &nouveau_device->base;

fail:
    nouveau_device_del(&nouveau_device->dev);
    nouveau_drm_del(&nouveau_device->drm);
    free(nouveau_device);
    return NULL;
}

static void nouveau_driver_destroy(driver_t *driver)
{
    nouveau_driver_t *nouveau_driver =
        (nouveau_driver_t *)driver->driver_private;

    if (nouveau_driver) {
        free(nouveau_driver);
        driver->driver_private = NULL;
    }
}

int allocator_driver_init(driver_t *driver)
{
    nouveau_driver_t *nouveau_driver;

    // Workaround for compiler warning that no unsigned value is < 0
    const unsigned int our_header_version = DRIVER_INTERFACE_VERSION;

    if (driver->driver_interface_version < our_header_version) {
        return -1;
    }

    nouveau_driver = (nouveau_driver_t *)calloc(1, sizeof(nouveau_driver_t));
    if (!nouveau_driver) {
        return -1;
    }

    nouveau_driver->pbase = driver;

    driver->driver_private = (void *)nouveau_driver;

    driver->driver_interface_version = DRIVER_INTERFACE_VERSION;

    driver->destroy = &nouveau_driver_destroy;
    driver->is_fd_supported = &nouveau_is_fd_supported;
    driver->device_create_from_fd = &nouveau_device_create_from_fd;

    return 0;
}
