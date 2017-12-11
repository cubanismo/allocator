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

#include <stdlib.h>

static int
nouveau_is_fd_supported(driver_t *driver, int fd)
{
    /* TODO */
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
    if (device) {
        /* TODO: Private device destruction */
        free(device);
    }
}

static device_t *nouveau_device_create_from_fd(driver_t *driver, int fd)
{
    device_t *device;

    /* TODO: Sanity checks on fd */

    device = (device_t *)calloc(1, sizeof(device_t));
    if (!device) {
        return NULL;
    }

    /* TODO: Private device creation */
    device->device_private = NULL;

    device->driver = driver;

    device->destroy = &nouveau_device_destroy;
    device->get_capabilities = &nouveau_device_get_capabilities;
    device->get_assertion_hints = &nouveau_device_get_assertion_hints;
    device->create_allocation = &nouveau_device_create_allocation;
    device->destroy_allocation = &nouveau_device_destroy_allocation;
    device->get_allocation_fd = &nouveau_device_get_allocation_fd;

    return device;
}

static void nouveau_driver_destroy(driver_t *driver)
{
    /* TODO */
}

int allocator_driver_init(driver_t *driver)
{
    // Workaround for compiler warning that no unsigned value is < 0
    const unsigned int our_header_version = DRIVER_INTERFACE_VERSION;

    if (driver->driver_interface_version < our_header_version) {
        return -1;
    }

     /* TODO: Private driver creation */
    driver->driver_private = NULL;

    driver->driver_interface_version = DRIVER_INTERFACE_VERSION;

    driver->destroy = &nouveau_driver_destroy;
    driver->is_fd_supported = &nouveau_is_fd_supported;
    driver->device_create_from_fd = &nouveau_device_create_from_fd;

    return 0;
}
