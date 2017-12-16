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

#ifndef __ALLOCATOR_HELPERS_H__
#define __ALLOCATOR_HELPERS_H__

#include <allocator/common.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#ifdef __cplusplus
extern "C" {
#endif

/*!
 * \file Inline helper functions to deal with capability sets.
 */

/*!
 * Free an array of capability sets created by the allocator library
 */
static inline void
free_capability_sets(uint32_t num_capability_sets,
                     capability_set_t *capability_sets)
{
    uint32_t i, j;

    if (capability_sets) {
        for (i = 0; i < num_capability_sets; i++) {
            if (capability_sets[i].capabilities) {
                for (j = 0; j < capability_sets[i].num_capabilities; j++) {
                    free((void *)capability_sets[i].capabilities[j]);
                }

                free((void *)capability_sets[i].capabilities);
            }

            free((void *)capability_sets[i].constraints);
        }

        free(capability_sets);
    }
}

/*!
 * Returns an exact copy of the given capability set
 */
static inline capability_set_t *
dup_capability_set(const capability_set_t *set)
{
    constraint_t *constraints;
    capability_header_t **capabilities;
    size_t constraints_size;
    size_t cap_size;
    uint32_t i;

    capability_set_t *dup_set = (capability_set_t *)calloc(1, sizeof(*set));
    if (!dup_set) {
        return NULL;
    }

    constraints_size = set->num_constraints * sizeof(*set->constraints);
    dup_set->constraints = constraints =
        (constraint_t *)malloc(constraints_size);
    if (!dup_set->constraints) {
        free_capability_sets(1, dup_set);
        return NULL;
    }

    dup_set->num_constraints = set->num_constraints;
    memcpy(constraints, set->constraints, constraints_size);

    capabilities = (capability_header_t **)
        calloc(set->num_capabilities, sizeof(*dup_set->capabilities));
    dup_set->capabilities = (const capability_header_t *const *)capabilities;
    if (!dup_set->capabilities) {
        free_capability_sets(1, dup_set);
        return NULL;
    }

    dup_set->num_capabilities = set->num_capabilities;
    for (i = 0; i < set->num_capabilities; i++) {
        cap_size = sizeof(*capabilities[i]) +
                   sizeof(uint32_t) * set->capabilities[i]->common.length_in_words;

        capabilities[i] = (capability_header_t *)malloc(cap_size);
        if (!capabilities[i]) {
            free_capability_sets(1, dup_set);
            return NULL;
        }

        memcpy(capabilities[i], set->capabilities[i], cap_size);
    }

    return dup_set;
}

/*!
 * Serialize a capability set to a stream of raw bytes.
 *
 * The caller is responsible for freeing the memory pointed to by
 * <data>:
 *
 *     free(data);
 */
static inline int
serialize_capability_set(const capability_set_t *set,
                         size_t *data_size,
                         void **data)
{
    size_t size = 0;
    unsigned char *d = NULL;
    uint32_t i;

    size += sizeof(set->num_constraints);
    size += sizeof(set->num_capabilities);

    /* constraints are fixed-size objects */
    size += set->num_constraints * sizeof(set->constraints[0]);

    for (i = 0; i < set->num_capabilities; i++) {
        /* length_in_words does not include the size of the capability header */
        size += sizeof(*set->capabilities[i]);

        /* Add in the size of the post-header capability content. */
        size += set->capabilities[i]->common.length_in_words * sizeof(uint32_t);
    }

    d = malloc(size);

    if (!d) {
        return -1;
    }

    *data = d;
    *data_size = size;

#define SERIALIZE(src, len) \
    assert(((d + (len)) - (unsigned char *)*data) <= size); \
    memcpy(d, (src), (len)); \
    d += (len)

    SERIALIZE(&set->num_constraints, sizeof(set->num_constraints));
    SERIALIZE(&set->num_capabilities, sizeof(set->num_capabilities));
    for (i = 0; i < set->num_constraints; i++) {
        SERIALIZE(&set->constraints[i], sizeof(set->constraints[i]));
    }

    for (i = 0; i < set->num_capabilities; i++) {
        size_t cap_size = sizeof(*set->capabilities[i]) +
            set->capabilities[i]->common.length_in_words * sizeof(uint32_t);

        SERIALIZE(set->capabilities[i], cap_size);
    }

#undef SERIALIZE

    return 0;
}

/*!
 * Allocate a capability set and populate it from a raw stream of bytes.
 *
 * The caller is responsible for freeing the memory pointed to by
 * <capability_set>:
 *
 *     free_capability_sets(1, *capability_set);
 */
static inline int
deserialize_capability_set(size_t data_size,
                           const void *data,
                           capability_set_t **capability_set)
{
    const unsigned char *d = data;
    constraint_t *constraints = NULL;
    capability_header_t **capabilities = NULL;
    uint32_t i;

    capability_set_t *set = calloc(1, sizeof(capability_set_t));

    if (!set) {
        goto fail;
    }

#define PEEK_DESERIALIZE(dst, size) \
    if (((d + size) - (const unsigned char *)data) > data_size) goto fail; \
    memcpy((dst), d, (size))

#define DESERIALIZE(dst, size) \
    PEEK_DESERIALIZE((dst), (size)); \
    d += (size)

    DESERIALIZE(&set->num_constraints, sizeof(set->num_constraints));
    DESERIALIZE(&set->num_capabilities, sizeof(set->num_capabilities));

    set->constraints = constraints =
        calloc(set->num_constraints, sizeof(*set->constraints));
    capabilities = calloc(set->num_capabilities, sizeof(*capabilities));
    set->capabilities = (const capability_header_t *const *)capabilities;

    if (!constraints || !capabilities) {
        goto fail;
    }

    for (i = 0; i < set->num_constraints; i++) {
        DESERIALIZE(&constraints[i], sizeof(set->constraints[i]));
    }

    for (i = 0; i < set->num_capabilities; i++) {
        capability_header_t header;

        PEEK_DESERIALIZE(&header, sizeof(header));

        capabilities[i] = calloc(1, sizeof(header) +
                                 header.common.length_in_words *
                                 sizeof(uint32_t));

        if (!capabilities[i]) {
            goto fail;
        }

        DESERIALIZE(capabilities[i], sizeof(*capabilities[i]) +
                    header.common.length_in_words * sizeof(uint32_t));
    }

#undef DESERIALIZE
#undef PEEK_DESERIALIZE

    set->constraints = constraints;
    set->capabilities = (const capability_header_t *const *)capabilities;
    *capability_set = set;

    return 0;

fail:
    free_capability_sets(1, set);

    return -1;
}

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* __ALLOCATOR_HELPERS_H__ */
