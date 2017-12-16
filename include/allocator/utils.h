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

#ifndef __ALLOCATOR_UTILS_H__
#define __ALLOCATOR_UTILS_H__

#include <allocator/common.h>
#include <assert.h>

#ifdef __cplusplus
extern "C" {
#endif

/*!
 * \file Inline miscellaneous utility macros and functions.
 */

#define UTIL_MAX2(X, Y)   ((X) > (Y) ? (X) : (Y))

static inline const usage_t *
util_find_use(uint32_t num_uses,
              const usage_t *uses,
              device_t *dev,
              uint16_t name)
{
    uint32_t i;

    for (i = 0; i < num_uses; i++) {
        if (uses[i].usage->name == name &&
            (dev == DEVICE_NONE || uses[i].dev == dev)) {
            return &uses[i];
        }
    }

    return NULL;
}

static inline const constraint_t *
util_find_constraint(const capability_set_t *set, uint16_t name)
{
    uint32_t i;

    for (i = 0; i < set->num_constraints; i++) {
        if (set->constraints[i].name == name) {
            return &set->constraints[i];
        }
    }

    return NULL;
}

static inline const capability_header_t *
util_find_cap(const capability_set_t *set, uint16_t name)
{
    uint32_t i;

    for (i = 0; i < set->num_capabilities; i++) {
        if (set->capabilities[i]->common.name == name) {
            return set->capabilities[i];
        }
    }

    return NULL;
}

static inline uint64_t
util_align(uint64_t value, uint64_t alignment)
{
    value += (alignment - 1);
    value &= ~(alignment - 1);
    return value;
}

static inline uint64_t
util_next_power_of_two(uint64_t x)
{
    uint64_t val = x;

    if (x <= 1) {
        return 1;
    }

    /* Already power of two? */
    if ((x & (x - 1)) == 0) {
        return x;
    }

    val--;
    val = (val >> 1) | val;
    val = (val >> 2) | val;
    val = (val >> 4) | val;
    val = (val >> 8) | val;
    val = (val >> 16) | val;
    val++;

    return val;
}

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* __ALLOCATOR_UTILS_H__ */
