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
#include <inttypes.h>

#include "test_utils.h"

void FAIL(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);

    exit(1);
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

void print_constraint(const constraint_t *constraint)
{
    int i;

#define DO_PRINT_CONSTRAINT(NAME, FMT, UNION_MEMBER)                            \
    case CONSTRAINT_ ## NAME:                                                   \
        printf("         name:  CONSTRAINT_" #NAME " (0x%x)\n",                 \
               constraint->name);                                               \
        printf("         value: %" FMT "\n", constraint->u.UNION_MEMBER.value);  \
        break

    switch (constraint->name) {
    DO_PRINT_CONSTRAINT(ADDRESS_ALIGNMENT, PRIu64, address_alignment);
    DO_PRINT_CONSTRAINT(PITCH_ALIGNMENT, PRIu32, pitch_alignment);
    DO_PRINT_CONSTRAINT(MAX_PITCH, PRIu32, max_pitch);
    default:
        printf("         name:  UNKNOWN (0x%x)\n", constraint->name);
        printf("         value: ");
        for (i = 0; i < sizeof(constraint->u); i++) {
            if (i > 0) {
                printf(":");
            }
            printf("%02X", ((const char *)(&constraint->u))[i]);
        }
        printf("\n");
        break;
    }

#undef DO_PRINT_CONSTRAINT
}

void print_capability_header(const capability_header_t *capability)
{
#define PRINT_VENDOR(NAME)                                                      \
        printf("         vendor:          " #NAME " (0x%x)\n",                  \
        capability->common.vendor);                                             \
        break
#define DO_PRINT_VENDOR(NAME) case VENDOR_ ## NAME: PRINT_VENDOR(NAME)

#define PRINT_CAP(NAME)                                                         \
        printf("         name:            %s (0x%x)\n",                         \
               (capability->common.vendor == VENDOR_BASE) ? "CAP_BASE_" #NAME : \
                                                            "VENDOR_CAP",       \
               capability->common.name);                                        \
        break
#define DO_PRINT_CAP(NAME) case CAP_BASE_ ## NAME: PRINT_CAP(NAME)

    switch (capability->common.vendor) {
    DO_PRINT_VENDOR(BASE);
    DO_PRINT_VENDOR(NVIDIA);
    DO_PRINT_VENDOR(ARM);
    DO_PRINT_VENDOR(INTEL);
    default:
        PRINT_VENDOR(UNKNOWN);
    }

    switch (capability->common.name) {
    DO_PRINT_CAP(PITCH_LINEAR);
    default:
        PRINT_CAP(UNKNOWN);
    }

    printf("         length_in_words: %u\n", capability->common.length_in_words);
    printf("         required:        %s\n", capability->required ? "true" : "false");

    if (capability->common.length_in_words > 0) {
        int i;

        printf("         value:           ");
        for (i = 0; i < capability->common.length_in_words; i++) {
            if (i > 0) {
                printf(":");
            }
            printf("%04X", ((const uint32_t *)(&capability[1]))[i]);
        }
        printf("\n");
    }

#undef PRINT_VENDOR
#undef DO_PRINT_VENDOR
#undef PRINT_CAP
#undef DO_PRINT_CAP
}

void print_capability_set(const capability_set_t *set)
{
    int i;

    printf("   capability_set_t (%p):\n", set);
    printf("      num_constraints: %u\n", set->num_constraints);
    printf("      constraints:\n");
    for (i = 0; i < set->num_constraints; i++) {
        printf("       %d:\n", i);
        print_constraint(&set->constraints[i]);
        printf("\n");
    }

    printf("      num_capabilities: %u\n", set->num_capabilities);
    printf("      capabilities:\n");
    for (i = 0; i < set->num_capabilities; i++) {
        printf("       %d:\n", i);
        print_capability_header(set->capabilities[i]);
        printf("\n");
    }
}
