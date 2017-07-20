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

#ifndef __ALLOCATOR_DRIVER_H__
#define __ALLOCATOR_DRIVER_H__

#include <allocator/common.h>

/*!
 * Top-level function table exported by drivers.
 */
typedef struct driver {
    /*!
     * Maximum version of the allocator<->driver interface supported by both
     * this driver and the allocation library.
     *
     * Populated by the allocator library before initializing the driver.  The
     * driver in turn should set this value to its maximum supported version if
     * it is lower than the version set by the allocator library.
     */
    unsigned int driver_interface_version;

    /*!
     * Private data used by the driver implementation.
     *
     * Populated by the driver, if needed.  This value must never be used by
     * the allocation library itself.
     */
    void *driver_private;

    /*!
     * Handle to the library containing the driver implementation referred to by
     * this structure.
     *
     * Populated by the allocator library.  The allocator library will use this
     * to unload the library after calling the destructor function.  This field
     * should generally be ignored by the driver code.
     */
    void *lib_handle;

    /*!
     * Linked list of initialized drivers.
     *
     * Populated by the allocator library and used to maintain a list of all
     * available drivers.  This field should generally be ignored by the driver
     * code.
     */
    struct driver *next;

    /*!
     * Driver destructor function, called by the allocator library before
     * unloading a driver.
     *
     * Populated by the driver.
     */
    void (*destroy)(struct driver *driver);

    /*!
     * Check whether this driver can initialize a device on the specified
     * file descriptor.
     *
     * Populated by the driver.
     *
     * \param[in] driver A pointer to this structure.
     *
     * \param[in] dev_fd A file descriptor referring to a device node.
     *
     * \return 0 if the file descriptor represents a device supported by
     *         this driver.  -1 if it does not.
     */
    int (*is_fd_supported)(struct driver *driver, int dev_fd);

    /*!
     * Initialize a device context on the device specified by a file descriptor
     *
     * Populated by the driver.
     *
     * \param[in] driver A pointer to this structure.
     *
     * \param[in] dev_fd A device file descriptor previously verified to be
     *                   compatible with this driver by \ref is_fd_supported.
     *
     * \return An initialized device context on success.  NULL on failure.
     */
    device_t *(*device_create_from_fd)(struct driver *driver, int dev_fd);
} driver_t;

/*!
 * Structure representing an initialized device context within the allocator
 */
struct device {
    /*! Pointer back to the driver instance that created this device. */
    driver_t *driver;

    /*!
     * Private data used by the device's driver implementation.
     *
     * Populated by the driver.  This value must never be used by the
     * allocation library itself.
     */
    void *device_private;

    /*!
     * Device destructor function.  Called when the application destroys a
     * device object.
     *
     * Populated by the driver.
     */
    void (*destroy)(device_t *dev);

    /*!
     * Query the capabilities and constraints of a device for a specified usage.
     *
     * Populated by the driver.
     *
     * Given an assertion and a list of uses, return the capabilities, if any,
     * this device supports for the requested usage.
     */
    int (*get_capabilities)(device_t *dev,
                            const assertion_t *assert,
                            uint32_t num_uses,
                            const usage_t *uses,
                            uint32_t *num_capability_sets,
                            capability_set_t **capability_sets);
};

/*!
 * Top-level driver entry point.
 *
 * \param[in,out] drv Top-level function table to be initialized by the driver.
 *                    drv->driver_interface_version will be set by the
 *                    allocator library prior to calling this function, and
 *                    the driver should set it to the version it supports if
 *                    that version is less than the version set by the
 *                    allocator library.
 *
 * \return 0 on success.
 */
typedef int (*driver_init_func_t)(driver_t *drv);

/*!
 * Name of the top-level driver entry point.
 */
#define DRIVER_INIT_FUNC "allocator_driver_init"

/*!
 * Current driver interface version
 */
#define DRIVER_INTERFACE_VERSION 1

/*!
 * Current driver json file major version
 */
#define JSON_FILE_VERSION_MAJOR 1

/*!
 * Current driver json file minor version
 */
#define JSON_FILE_VERSION_MINOR 0

/*!
 * Current driver json file micro version
 */
#define JSON_FILE_VERSION_MICRO 0

#endif /* __ALLOCATOR_DRIVER_H__ */
