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
#include <assert.h>
#include <allocator/allocator.h>
#include <allocator/driver.h>
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

    *new_constraints = malloc(sizeof((*new_constraints)[0]) * num_constraints0);

    if (!*new_constraints) {
        return -1;
    }

    common_constraints = 0;
    k = 0;

    for (i = 0; i < num_constraints0; i++) {
        int merged = 0;

        for (j = 0; j < num_constraints1; i++) {
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

                merged = 1;
                break;
            }
        }

        if (merged == 0) {
            (*new_constraints)[k++] = constraints0[i];
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
            int merged = 0;
            for (i = 0; i <  num_constraints0; i++) {
                if (constraints1[j].name == constraints0[i].name) {
                    merged = 1;
                    break;
                }
            }

            if (merged == 0) {
                (*new_constraints)[k++] = constraints1[j];
            }
        }
    }

    assert(*num_new_constraints == k);

    return 0;
}

static int are_capabilities_compatible(const capability_header_t *cap0,
                                       const capability_header_t *cap1)
{
    int compatible = 0;
    const capability_header_t *tmp;
    int iterations = 0;

    for (int iterations = 0; iterations < 2; iterations++) {
        if (cap0->vendor == 0) {
            compatible |=
                capabilities_compatible_func_table[cap0.name](cap0, cap1);
        } else {
            driver_t *driver = find_driver_for_fencor(cap0->vendor);

            assert(driver);

            compatible |= driver->capabilities_compatible(cap0, cap1);
        }

        tmp = cap0;
        cap0 = cap1;
        cap1 = tmp;
    }

    return compatible;
}

static int intersect_capabilities(uint32_t num_caps0,
                                  const capability_header_t *caps0,
                                  uint32_t num_caps1,
                                  const capability_header_t *caps1,
                                  uint32_t *num_new_capabilities,
                                  capability_header_t **new_capabilities)
{
    for (uint32_t i0 = 0; i0 < num_caps0 && !ret; i0++) {
        uint32_t i1;

        for (i1 = 0; i1 < num_caps1; i1++) {
            if (!are_capabilities_compatible(&caps0[i0], caps1[i1])) {
                return -1;
            }
        }
    }
    
    // TODO: Great, everything is compatible, but what is the resulting list
    // of capabilities?  The lists aren't necessarily equal or a superset of
    // the other.  The intersection of them isn't necessarily meaningful, and
    // the union of them isn't likely to be understood by any one vendor/
    // allocator, so that can't be the result either.
    
    return 0;
}

int derive_capabilities(uint32_t num_caps0,
                        capability_set_t *caps0,
                        uint32_t num_caps1,
                        capability_set_t *caps1,
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
            capability_header_t *new_capabilities;
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
                                       caps1[i1].capabilities)) {
                continue;
            }

            temp_caps = realloc(new_capability_sets,
                                sizeof(*new_capability_sets) *
                                num_new_capability_sets + 1);

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
                new_capabilities;
            num_new_capability_sets++;
        }
    }

    *num_capability_sets = num_new_capability_sets;
    *capability_sets = new_capability_sets;

    return 0;
}
