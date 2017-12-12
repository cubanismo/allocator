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
#include <allocator/helpers.h>
#include <allocator/utils.h>

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

typedef struct {
    allocation_t base;

    struct nouveau_bo *bo;
    int fd;
} nouveau_allocation_t;

/* Nouveau capability definitions */
#define NOUVEAU_CAP_WORDS(c) ((sizeof(c) - sizeof(capability_header_t) + 3)/4)

typedef struct {
    capability_header_t header;
} nouveau_cap_vidmem_t;

#define NOUVEAU_CAP_VIDMEM_NAME  0xF000
#define NOUVEAU_CAP_VIDMEM_WORDS NOUVEAU_CAP_WORDS(nouveau_cap_vidmem_t)

typedef struct {
    capability_header_t header;
} nouveau_cap_contig_t;

#define NOUVEAU_CAP_CONTIG_NAME  0xF001
#define NOUVEAU_CAP_CONTIG_WORDS NOUVEAU_CAP_WORDS(nouveau_cap_contig_t)


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

static int nouveau_check_uses(const nouveau_device_t *dev,
                              uint32_t num_uses,
                              const usage_t *uses)
{
    uint32_t i;

    for (i = 0; i < num_uses; i++) {
        if ((uses[i].dev == &dev->base) || (uses[i].dev == DEVICE_NONE)) {
            return 1;
        }
    }

    return 0;
}

static int
nouveau_device_get_capabilities(device_t *dev,
                                const assertion_t *assertion,
                                uint32_t num_uses,
                                const usage_t *uses,
                                uint32_t *num_sets,
                                capability_set_t **capability_sets)
{
    const nouveau_device_t *nouveau_device = dev->device_private;
    uint32_t n_sets = 0;
    capability_set_t *cap_sets;

    constraint_t *tmp_constr;
    capability_header_t **tmp_caps;

    capability_pitch_linear_t *cap_pitch;
    nouveau_cap_vidmem_t *cap_vidmem;
    nouveau_cap_contig_t *cap_contig;
    int is_display;

    uint32_t n_pitch_constr = 3; /* pitch+address alignment, max pitch */
    uint32_t n_pitch_caps = 2; /* pitch, vidmem */

    if (!nouveau_check_uses(nouveau_device, num_uses, uses)) {
        /* The app didn't specify any use for this device */
        *num_sets = 0;
        *capability_sets = NULL;
        return 0;
    }

    is_display = !!util_find_use(num_uses, uses, dev, USAGE_BASE_DISPLAY);
    if (is_display) {
        n_pitch_caps += 1; /* contig */
    }

    n_sets = 1; /* Pitch-linear */

    cap_sets = calloc(n_sets, sizeof(*cap_sets));
    if (!cap_sets) {
        return -1;
    }

    /* Fill in pitch-linear constraints and capabilities */

    tmp_constr = calloc(n_pitch_constr, sizeof(*tmp_constr));
    tmp_caps = calloc(n_pitch_caps, sizeof(*tmp_caps));
    cap_pitch = calloc(1, sizeof(capability_pitch_linear_t));
    cap_vidmem = calloc(1, sizeof(nouveau_cap_vidmem_t));
    if (is_display) {
        cap_contig = calloc(1, sizeof(nouveau_cap_contig_t));
    }

    if (!tmp_constr ||
        !tmp_caps ||
        !cap_pitch ||
        !cap_vidmem ||
        (is_display && !cap_contig)) {
        free(tmp_constr);
        free(tmp_caps);
        free(cap_pitch);
        free(cap_vidmem);
        free(cap_contig);
        free(cap_sets);
        return -1;
    }

    tmp_constr[0].name = CONSTRAINT_ADDRESS_ALIGNMENT;
    tmp_constr[0].u.address_alignment.value =
        nouveau_device->properties.address_alignment;

    tmp_constr[1].name = CONSTRAINT_PITCH_ALIGNMENT;
    tmp_constr[1].u.pitch_alignment.value =
        nouveau_device->properties.pitch_alignment;

    tmp_constr[2].name = CONSTRAINT_MAX_PITCH;
    tmp_constr[2].u.max_pitch.value =
        nouveau_device->properties.max_pitch;

    cap_pitch->header.common.vendor = VENDOR_BASE;
    cap_pitch->header.common.name = CAP_BASE_PITCH_LINEAR;
    cap_pitch->header.common.length_in_words = 0;
    cap_pitch->header.required = 1;
    tmp_caps[0] = &cap_pitch->header;

    cap_vidmem->header.common.vendor = VENDOR_NVIDIA;
    cap_vidmem->header.common.name = NOUVEAU_CAP_VIDMEM_NAME;
    cap_vidmem->header.common.length_in_words = NOUVEAU_CAP_VIDMEM_WORDS;
    cap_vidmem->header.required = 0;
    tmp_caps[1] = &cap_vidmem->header;

    if (cap_contig) {
        cap_contig->header.common.vendor = VENDOR_NVIDIA;
        cap_contig->header.common.name = NOUVEAU_CAP_CONTIG_NAME;
        cap_contig->header.common.length_in_words = NOUVEAU_CAP_CONTIG_WORDS;
        cap_contig->header.required = 1;
        tmp_caps[2] = &cap_contig->header;
    }

    cap_sets[0].constraints = tmp_constr;
    cap_sets[0].num_constraints = n_pitch_constr;
    cap_sets[0].capabilities = (const capability_header_t *const *)tmp_caps;
    cap_sets[0].num_capabilities = n_pitch_caps;

    *capability_sets = cap_sets;
    *num_sets = n_sets;

    return 0;
}

static int
nouveau_device_get_assertion_hints(device_t *dev,
                                   uint32_t num_uses,
                                   const usage_t *uses,
                                   uint32_t *num_hints,
                                   assertion_hint_t **hints)
{
    nouveau_device_t *nouveau_device = (nouveau_device_t *)dev->device_private;
    uint32_t n_hints;
    assertion_hint_t *hint_array;
    uint32_t i;

    if (!nouveau_check_uses(nouveau_device, num_uses, uses)) {
        /* The app didn't specify any use for this device */
        *num_hints = 0;
        *hints    = NULL;
        return 0;
    }

    /*
     * TODO: 1. Fetch internal pixel formats for this device
     *       2. For each use
     *            For each pixel format
     *               If use and pixel format are not compatible, discard pixel
     *               format
     *          If different uses have different requirements (e.g. different
     *          size limits for scanout/render surfaces)
     *            Find the minimum set that satisfies all uses
     *       3. If no pixel formats survived OR no compatible uses
     *            Return 0 hints
     *       4. Return N hints
     */
    n_hints = 1;

    hint_array = (assertion_hint_t *)calloc(n_hints, sizeof(*hint_array));
    if (!hint_array) {
        return -1;
    }

    for (i = 0; i < n_hints; i++) {
        assertion_hint_t _HINT_INIT_ = {
            /* XXX max_width/max_height should vary based on usage */
            .max_width   = nouveau_device->properties.max_dimensions,
            .max_height  = nouveau_device->properties.max_dimensions,

            /* XXX No format enumeration yet. Assume RGBA8888 everywhere */
            .num_formats = 0,
            .formats     = NULL
        };

        /* Need this because of assertion_hint_t constness */
        memcpy(&hint_array[i], &_HINT_INIT_, sizeof(_HINT_INIT_));
    }

    *num_hints = n_hints;
    *hints = hint_array;

    return 0;
}

static void
nouveau_device_destroy_allocation(device_t *dev,
                                  allocation_t *allocation)
{
    nouveau_allocation_t *nouveau_allocation =
        (nouveau_allocation_t *)allocation->allocation_private;

    if (nouveau_allocation) {
        if (nouveau_allocation->fd >= 0) {
            close(nouveau_allocation->fd);
        }

        nouveau_bo_ref(NULL, &nouveau_allocation->bo);

        free_capability_sets(1, (capability_set_t *)allocation->capability_set);

        free(nouveau_allocation);
    }
}

static int nouveau_device_create_allocation(device_t *dev,
                                  const assertion_t *assertion,
                                  const capability_set_t *capability_set,
                                  allocation_t **allocation)
{
    nouveau_driver_t *nouveau_driver =
        (nouveau_driver_t *)dev->driver->driver_private;
    nouveau_device_t *nouveau_device =
        (nouveau_device_t *)dev->device_private;
    nouveau_allocation_t *nouveau_allocation = NULL;

    int is_vidmem = !!util_find_cap(capability_set, NOUVEAU_CAP_VIDMEM_NAME);
    int is_contig = !!util_find_cap(capability_set, NOUVEAU_CAP_CONTIG_NAME);
    const constraint_t *address_alignment =
        util_find_constraint(capability_set, CONSTRAINT_ADDRESS_ALIGNMENT);
    const constraint_t *pitch_alignment =
        util_find_constraint(capability_set, CONSTRAINT_PITCH_ALIGNMENT);

    uint32_t bpp = 32; /* XXX Should be based on assertion->format */
    uint32_t pitch = 0;
    uint32_t height = 0;
    uint32_t flags = 0;
    union nouveau_bo_config bo_config = { 0 };

    /* Allocate the allocation object */
    nouveau_allocation =
        (nouveau_allocation_t *)calloc(1, sizeof(nouveau_allocation_t));
    if (!nouveau_allocation) {
        goto fail;
    }

    nouveau_allocation->base.allocation_private = nouveau_allocation;
    nouveau_allocation->fd = -1;

    /* Save the capability set for future use */
    nouveau_allocation->base.capability_set = dup_capability_set(capability_set);
    if (!nouveau_allocation->base.capability_set) {
        goto fail;
    }

    /* Set the approriate buffer object flags */
    flags |= (is_vidmem ? NOUVEAU_BO_VRAM   : 0);
    flags |= (is_contig ? NOUVEAU_BO_CONTIG : 0);
    flags |= NOUVEAU_BO_NOSNOOP;

    /* Allocation size */
    pitch = bpp * assertion->width / 8;
    pitch = util_align(pitch, (pitch_alignment ?
                               pitch_alignment->u.pitch_alignment.value : 1));

    /* Account for very generous prefetch (allocate size as if tiled). */
    height = UTIL_MAX2(assertion->height, 8);
    height = util_next_power_of_two(height);

    nouveau_allocation->base.size = pitch * height;

    /* Memory type and tile mode according to format.
     *
     * Memory type values below come from the nouveau Gallium driver
     * implementation in Mesa.
     */
    /* TODO: Handle all formats and tile modes */
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
        switch (bpp) {
        case 32:
            bo_config.nvc0.memtype = 0xfe; /* uncompressed */
            break;
        default:
            bo_config.nvc0.memtype = 0;
            break;
        }
        bo_config.nvc0.tile_mode = 0; /* No tiling */
        break;
    default:
        switch (bpp) {
        case 32:
            bo_config.nv50.memtype = 0x70; /* uncompressed */
            break;
        default:
            bo_config.nvc0.memtype = 0;
            break;
        }
        bo_config.nv50.tile_mode = 0; /* No tiling */
        break;
    }

    if (nouveau_bo_new(nouveau_device->dev,
                       flags,
                       (address_alignment ?
                        address_alignment->u.address_alignment.value : 1),
                       nouveau_allocation->base.size,
                       &bo_config,
                       &nouveau_allocation->bo))
    {
        goto fail;
    }

    if (nouveau_bo_set_prime(nouveau_allocation->bo, &nouveau_allocation->fd)) {
        goto fail;
    }

    *allocation = &nouveau_allocation->base;

    return 0;

fail:
    if (nouveau_allocation) {
        dev->destroy_allocation(dev, &nouveau_allocation->base);
    }
    return -1;
}

static int nouveau_device_get_allocation_fd(device_t *dev,
                                 const allocation_t *allocation,
                                 int *fd)
{
    const nouveau_allocation_t *nouveau_allocation =
        (const nouveau_allocation_t *)allocation->allocation_private;

    *fd = dup(nouveau_allocation->fd);

    return (*fd < 0) ? -1 : 0;
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
