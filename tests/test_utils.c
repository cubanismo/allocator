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

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "test_utils.h"

void FAIL(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);

    exit(1);
}

void free_capability_sets(uint32_t num_sets, capability_set_t *sets)
{
    uint32_t s;

    for (s = 0; s < num_sets; s++) {
        uint32_t i;

        free((void *)sets[s].constraints);

        for (i = 0; i < sets[s].num_capabilities; i++) {
            free((void *)sets[s].capabilities[i]);
        }

        free((void *)sets[s].capabilities);
    }

    free(sets);
}

int compare_capability_sets(capability_set_t *set0, capability_set_t *set1)
{
    uint32_t i;

    if (!set0 || !set1) {
        return 1;
    }

    if (set0->num_constraints != set1->num_constraints) {
        return 1;
    }

    if (set0->num_capabilities != set1->num_capabilities) {
        return 1;
    }

    if (memcmp(set0->constraints, set1->constraints,
               sizeof(set0->constraints[0]) * set0->num_constraints)) {
        return 1;
    }

    for (i = 0; i < set0->num_capabilities; i++) {
        if (memcmp(set0->capabilities[i], set1->capabilities[i],
                   sizeof(*set0->capabilities[i]) +
                   (set0->capabilities[i]->common.length_in_words *
                    sizeof(uint32_t)))) {
            return 1;
        }
    }

    return 0;
}
