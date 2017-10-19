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

#ifndef __ALLOCATOR_ALLOCATOR_H__
#define __ALLOCATOR_ALLOCATOR_H__

#include <allocator/common.h>

#ifdef __cplusplus
extern "C" {
#endif

/*!
 * \file Allocator constructs specific to allocator clients/applications.
 */

/*!
 * Initialize a device context on the specified device file descriptor.
 */
extern device_t *device_create(int dev_fd);

/*!
 * Tear down a device context returned by dev_create().
 */
extern void device_destroy(device_t *dev);

/*!
 * Query device capabilities for a given assertion.
 */
extern int device_get_capabilities(device_t *dev,
                                   const assertion_t *assertion,
                                   uint32_t num_uses,
                                   const usage_t *uses,
                                   uint32_t *num_capability_sets,
                                   capability_set_t** capability_sets);

/*!
 * Compute a list of common capabilities by determining the compatible combination
 * of two existing capability set lists.
 */
extern int derive_capabilities(uint32_t num_caps0,
                               const capability_set_t *caps0,
                               uint32_t num_caps1,
                               const capability_set_t *caps1,
                               uint32_t *num_capability_sets,
                               capability_set_t** capability_sets);

/*!
 * Query device assertion hints for a given usage
 *
 * Returns both the number of hints <num_hints> and a malloc'ed array of hints
 * <hints>.
 *
 * The caller is responsible of freeing the memory pointed by <hints>:
 *
 *     free((void *)((*hints)->formats));
 *     free((void *)(*hints));
 */
extern int device_get_assertion_hints(device_t *dev,
                                      uint32_t num_uses,
                                      const usage_t *uses,
                                      uint32_t *num_hints,
                                      assertion_hint_t **hints);

/*!
 * Create an allocation conforming to an assertion and capability set on the
 * specified device.
 */
extern int device_create_allocation(device_t *dev,
                                    const assertion_t *assertion,
                                    const capability_set_t *capability_set,
                                    allocation_t **allocation);

/*!
 * Destroy an allocation previously created on the specified device.
 */
extern void device_destroy_allocation(device_t *dev,
                                      allocation_t *allocation);

/*!
 * Serialize a capability set to a stream of raw bytes.
 *
 * The caller is responsible for freeing the memory pointed to by
 * <data>:
 *
 *     free(data);
 */
extern int serialize_capability_set(const capability_set_t *capability_set,
                                    size_t *data_size,
                                    void **data);

/*!
 * Allocate a capability set and populate it from a raw stream of bytes.
 *
 * The caller is responsible for freeing the memory pointed to by
 * <capability_set>:
 *
 *     free((void *)((*capability_set)->constraints));
 *     for i = [0..(*capability_set)->num_capabilities)
 *         free((void *)((*capability_set)->capabilities[i]));
 *     free((void *)((*capability_set)->capabilities));
 */
extern int deserialize_capability_set(size_t data_size,
                                      const void *data,
                                      capability_set_t **capability_set);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* __ALLOCATOR_ALLOCATOR_H__ */
