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

#ifndef __ALLOCATOR_TEST_UTILS_H__
#define __ALLOCATOR_TEST_UTILS_H__

#include <allocator/allocator.h>

extern void FAIL(const char *fmt, ...);
extern void free_assertion_hints(uint32_t num_hints, assertion_hint_t *hints);
extern void free_capability_sets(uint32_t num_sets, capability_set_t *sets);
extern int compare_capability_sets(capability_set_t *set0,
                                   capability_set_t *set1);
extern const constraint_t *find_constraint(const capability_set_t *set,
                                           uint32_t name);
extern void print_constraint(const constraint_t *constraint);
extern void print_capability_header(const capability_header_t *capability);
extern void print_capability_set(const capability_set_t *set);

#endif /* __ALLOCATOR_TEST_UTILS_H__ */
