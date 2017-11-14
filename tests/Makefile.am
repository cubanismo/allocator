# Copyright (c) 2017 NVIDIA CORPORATION.  All rights reserved.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

bin_PROGRAMS = capability_set_ops device_alloc create_allocation

capability_set_ops_CFLAGS = -I$(top_srcdir)/include
capability_set_ops_SOURCES = capability_set_ops.c test_utils.c
capability_set_ops_LDADD = $(top_builddir)/src/liballocator.la

device_alloc_CFLAGS = -I$(top_srcdir)/include
device_alloc_SOURCES = device_alloc.c test_utils.c
device_alloc_LDADD = $(top_builddir)/src/liballocator.la

create_allocation_CFLAGS = -I$(top_srcdir)/include
create_allocation_SOURCES = create_allocation.c test_utils.c
create_allocation_LDADD = $(top_builddir)/src/liballocator.la

if HAVE_LIBDRM
bin_PROGRAMS += drm_import_allocation
drm_import_allocation_CFLAGS = -I$(top_srcdir)/include $(LIBDRM_CFLAGS)
drm_import_allocation_SOURCES = drm_import_allocation.c test_utils.c
drm_import_allocation_LDADD = $(top_builddir)/src/liballocator.la
drm_import_allocation_LDFLAGS = $(LIBDRM_LIBS)
endif

noinst_HEADERS = test_utils.h
