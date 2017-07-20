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

#ifndef __SRC_CONSTRAINT_FUNCS_H__
#define __SRC_CONSTRAINT_FUNCS_H__

#include <allocator/common.h>

typedef int (*constraint_merge_func_t)(const constraint_t *a,
                                       const constraint_t *b,
                                       constraint_t *merged);

extern constraint_merge_func_t constraint_merge_func_table[CONSTRAINT_END];

/* XXX The below should be auto-generated */

extern int merge_address_alignment(const constraint_t *a,
                                   const constraint_t *b,
                                   constraint_t *merged);

extern int merge_pitch_alignment(const constraint_t *a,
                                 const constraint_t *b,
                                 constraint_t *merged);

extern int merge_max_pitch(const constraint_t *a,
                           const constraint_t *b,
                           constraint_t *merged);

#endif /* __SRC_CONSTRAINT_FUNCS_H__ */
