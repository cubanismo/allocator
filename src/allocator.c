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

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <allocator/allocator.h>
#include <allocator/driver.h>
#include <allocator/helpers.h>
#include "driver_manager.h"
#include "constraint_funcs.h"

device_t *device_create(int dev_fd)
{
    driver_t *driver = find_driver_for_fd(dev_fd);

    if (!driver) {
        return NULL;
    }

    return driver->device_create_from_fd(driver, dev_fd);
}

void device_destroy(device_t *dev)
{
    if (dev) {
        dev->destroy(dev);
    }
}

int device_get_capabilities(device_t *dev,
                            const assertion_t *assert,
                            uint32_t num_uses,
                            const usage_t *uses,
                            uint32_t *num_capability_sets,
                            capability_set_t **capability_sets)
{
    return dev->get_capabilities(dev,
                                 assert,
                                 num_uses,
                                 uses,
                                 num_capability_sets,
                                 capability_sets);
}

#define ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))

/*!
 * Merge two lists of constraints.
 *
 * Entries in only one of the two original lists are included verbatim in the
 * merged list.
 *
 * If the two lists each contain a given constraint name, the two values for
 * that constraint itself are merged and the result is included in the merged
 * list.
 *
 * This will allocate and return memory in *new_constraints.  The caller is
 * responsible for freeing this memory using free().
 */
static int merge_constraints(uint32_t num_constraints0,
                             const constraint_t *constraints0,
                             uint32_t num_constraints1,
                             const constraint_t *constraints1,
                             uint32_t *num_new_constraints,
                             constraint_t **new_constraints)
{
    int res;
    uint32_t i, j, k, common_constraints;

    *new_constraints = calloc(num_constraints0, sizeof((*new_constraints)[0]));

    if (!*new_constraints) {
        return -1;
    }

    common_constraints = 0;
    k = 0;

    for (i = 0; i < num_constraints0; i++) {
        for (j = 0; j < num_constraints1; j++) {
            if (constraints0[i].name == constraints1[j].name) {
                if (constraints0[i].name >=
                    ARRAY_LEN(constraint_merge_func_table)) {
                    return -1;
                }

                res = constraint_merge_func_table[constraints0[i].name](
                    &constraints0[i],
                    &constraints1[j],
                    &(*new_constraints)[k++]);

                if (res) {
                    free(*new_constraints);
                    return res;
                }

                common_constraints++;

                break;
            }
        }

        if (j == num_constraints1) {
            memcpy(&(*new_constraints)[k++], &constraints0[i],
                   sizeof(constraints0[i]));
        }
    }

    assert(k == num_constraints0);
    assert(common_constraints <= num_constraints0);

    *num_new_constraints = k + num_constraints1 - common_constraints;

    if (*num_new_constraints != num_constraints0) {
        constraint_t *tmp_constraints =
            realloc(*new_constraints,
                    sizeof((*new_constraints)[0]) * *num_new_constraints);

        if (!tmp_constraints) {
            free(*new_constraints);
            return -1;
        }

        *new_constraints = tmp_constraints;

        for (j = 0; j < num_constraints1; j++) {
            for (i = 0; i <  num_constraints0; i++) {
                if (constraints1[j].name == constraints0[i].name) {
                    break;
                }
            }

            if (i == num_constraints0) {
                memcpy(&(*new_constraints)[k++], &constraints1[j],
                       sizeof(constraints1[j]));
            }
        }
    }

    assert(*num_new_constraints == k);

    return 0;
}

/*!
 * Compare two individual capabilities for equivalence.
 */
static int compare_capabilities(const capability_header_t *cap0,
                                const capability_header_t *cap1)
{
    int diff = (int)cap0->common.length_in_words -
        (int)cap1->common.length_in_words;

    if (diff != 0) {
        return diff;
    }

    /*
     * Compare the header manually, so the "required" field can be ignored.
     *
     * TODO: Is ignoring the required field necessary and desirable?
     */
    diff = memcmp(&cap0->common,
                  &cap1->common,
                  sizeof(cap0->common));

    if (diff != 0) {
        return diff;
    }

    /*
     * This works as long as no uninitialized padding data exists in the
     * structs.  To ensure that, all capability headers * and their tail
     * data need to be allocated using calloc.
     */
    return memcmp(&cap0[1],
                  &cap1[1],
                  cap0->common.length_in_words * sizeof(uint32_t));
}

/*!
 * Helper function to deep-free an array of capabilities.
 */
static void free_capabilities(uint32_t num_caps, capability_header_t **caps)
{
    uint32_t i;

    for (i = 0; i < num_caps; i++) {
        free(caps[i]);
    }

    free(caps);
}

/*!
 * Generate the intersection of two lists of capabilities.
 *
 * Each capability can be included at most once in a given capability list.
 *
 * If a capability exist in only one of the two original lists, it will not be
 * included in the final list.
 *
 * If a capability name exists in both lists but the two capabilities are not
 * equivalent, they will not be included in the final list.  There is no merging
 * or intersecting of capability values.
 *
 * Capability lists are unordered.  Culling a capability marked as required
 * invalidates the list, causing the intersection operation to fail.
 *
 * This will allocate and return memory in *new_capabilities.  The caller is
 * responsible for freeing this memory using free().
 */
static int intersect_capabilities(uint32_t num_caps0,
                                  const capability_header_t *const *caps0,
                                  uint32_t num_caps1,
                                  const capability_header_t *const *caps1,
                                  uint32_t *num_new_capabilities,
                                  capability_header_t ***new_capabilities)
{
    const uint32_t max_new_caps = num_caps0 > num_caps1 ? num_caps1 : num_caps0;
    int *matched_caps1 = NULL;
    capability_header_t **new_caps = NULL;
    uint32_t num_new_caps = 0;
    uint32_t i0, i1;


    if ((num_caps0 < 1) || (num_caps1 < 1)) {
        return -1;
    }

    new_caps = (capability_header_t **)calloc(max_new_caps,
                                              sizeof(capability_header_t *));
    matched_caps1 = calloc(num_caps1, sizeof(*matched_caps1));

    if (!new_caps || !matched_caps1) {
        free(new_caps);
        free(matched_caps1);
        return -1;
    }

    for (uint32_t i0 = 0; i0 < num_caps0; i0++) {
        int required = 0;

        for (i1 = 0; i1 < num_caps1; i1++) {
            if (compare_capabilities(caps0[i0], caps1[i1])) {
                continue;
            } else {
                /* Capabilities should never be duplicated in either list */
                assert(matched_caps1[i1] == 0);
                matched_caps1[i1] = 1;
                required = caps1[i1]->required;
                break;
            }
        }

        if (i1 < num_caps1) {
            /*
             * An equivalent capability was found.  Include this capability in
             * the new list.
             */
            size_t cap_size = sizeof(*caps0[i0]) +
                caps0[i0]->common.length_in_words * sizeof(uint32_t);

            new_caps[num_new_caps] = (capability_header_t *)calloc(1, cap_size);

            if (!new_caps[num_new_caps]) {
                free_capabilities(num_new_caps, new_caps);
                free(matched_caps1);
                return -1;
            }

            memcpy(new_caps[num_new_caps], caps0[i0], cap_size);
            new_caps[num_new_caps++]->required |= required;
        } else if (caps0[i0]->required) {
            free_capabilities(num_new_caps, new_caps);
            free(matched_caps1);

            return -1;
        }
    }

    for (i1 = 0; i1 < num_caps1; i1++) {
        if ((matched_caps1[i1] == 0) && caps1[i1]->required) {
            /*
             * A required capability from caps1 was not included in the
             * intersected list.  The resulting list is therefore invalid from
             * the point of view of the generator of the list caps1, so the
             * intersection fails.
             */
            free_capabilities(num_new_caps, new_caps);
            free(matched_caps1);
            return -1;
        }
    }

    assert(num_new_caps >= 1);
    assert(num_new_caps <= max_new_caps);

    *new_capabilities =
        (capability_header_t **)realloc(new_caps,
                                        num_new_caps *
                                        sizeof(capability_header_t *));

    if (!*new_capabilities) {
        /*
         * Shrinking an allocation shouldn't fail, but if it somehow does just
         * return the existing, slightly larger than necessary allocation.
         */
        *new_capabilities = new_caps;
    }

    *num_new_capabilities = num_new_caps;

    free(matched_caps1);

    return 0;
}

/*!
 * Given two lists of capability sets, find the subset of compatible sets.
 *
 * Capability sets are only partially mutable.  This function attempts to
 * merge each capability set in caps0[] against each capability set in
 * caps1[] by merging the two sets' constraints and intersecting their
 * capabilities.  Constraint list and capability list merging is defined
 * by the helper functions \ref merge_constraints() and
 * \ref intersect_capabilities().
 *
 * This function will allocate new memory to store a list of capability
 * sets and return it in *capability_sets, and the capability sets
 * themselves will be allocated as well.  It is the callers responsibility
 * to free this memory using free().
 */
int derive_capabilities(uint32_t num_caps0,
                        const capability_set_t *caps0,
                        uint32_t num_caps1,
                        const capability_set_t *caps1,
                        uint32_t *num_capability_sets,
                        capability_set_t **capability_sets)
{
    uint32_t num_new_capability_sets = 0;
    capability_set_t *new_capability_sets = NULL;

    // 1) Filter based on compatible constraints.
    // 2) Remove capability sets with any incompatible capabilities
    // 3) Return the resulting capability set.

    for (uint32_t i0 = 0; i0 < num_caps0; i0++) {
        for (uint32_t i1 = 0; i1 < num_caps1; i1++) {
            uint32_t num_new_constraints;
            constraint_t *new_constraints;
            uint32_t num_new_capabilities;
            capability_header_t **new_capabilities;
            capability_set_t *temp_caps;

            if (merge_constraints(caps0[i0].num_constraints,
                                  caps0[i0].constraints,
                                  caps1[i1].num_constraints,
                                  caps1[i1].constraints,
                                  &num_new_constraints,
                                  &new_constraints)) {
                continue;
            }

            if (intersect_capabilities(caps0[i0].num_capabilities,
                                       caps0[i0].capabilities,
                                       caps1[i1].num_capabilities,
                                       caps1[i1].capabilities,
                                       &num_new_capabilities,
                                       &new_capabilities)) {
                continue;
            }

            temp_caps = realloc(new_capability_sets,
                                sizeof(*new_capability_sets) *
                                (num_new_capability_sets + 1));

            if (!temp_caps) {
                free(new_capability_sets);
                free(new_constraints);
                free(new_capabilities);
                return -1;
            }

            new_capability_sets = temp_caps;
            new_capability_sets[num_new_capability_sets].num_constraints =
                num_new_constraints;
            new_capability_sets[num_new_capability_sets].constraints =
                new_constraints;
            new_capability_sets[num_new_capability_sets].num_capabilities =
                num_new_capabilities;
            new_capability_sets[num_new_capability_sets].capabilities =
                (const capability_header_t *const *)new_capabilities;
            num_new_capability_sets++;
        }
    }

    *num_capability_sets = num_new_capability_sets;
    *capability_sets = new_capability_sets;

    return 0;
}

/*!
 * Given a list of uses, returns a list of assertion hints.
 *
 * An application can use the returned hints to tweak its requirements to fall
 * within valid and supported ranges.
 *
 * This function will allocate new memory to store a list of assertion hints and
 * return it in *hints.  It is the callers responsibility to free this memory
 * using free().
 */
int device_get_assertion_hints(device_t *dev,
                               uint32_t num_uses,
                               const usage_t *uses,
                               uint32_t *num_hints,
                               assertion_hint_t **hints)
{
    return dev->get_assertion_hints(dev,
                                    num_uses,
                                    uses,
                                    num_hints,
                                    hints);
}

void free_assertion_hints(uint32_t num_hints, assertion_hint_t *hints)
{
    uint32_t h;

    for (h = 0; h < num_hints; h++) {
        free((void *)hints[h].formats);
    }

    free(hints);
}

int device_create_allocation(device_t *dev,
                             const assertion_t *assertion,
                             const capability_set_t *capability_set,
                             allocation_t **allocation)
{
    return dev->create_allocation(dev, assertion, capability_set, allocation);
}

void device_destroy_allocation(device_t *dev, allocation_t *allocation)
{
    dev->destroy_allocation(dev, allocation);
}

int device_export_allocation(device_t *dev,
                             const allocation_t *allocation,
                             uint64_t *allocation_size,
                             size_t *metadata_size,
                             void **metadata,
                             int *fd)
{
    int status = serialize_capability_set(allocation->capability_set,
                                          metadata_size,
                                          metadata);

    if (status) {
        return status;
    }

    *allocation_size = allocation->size;

    return dev->get_allocation_fd(dev, allocation, fd);
}
