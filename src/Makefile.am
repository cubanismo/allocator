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

lib_LTLIBRARIES = liballocator.la

liballocator_la_CFLAGS = -I$(top_srcdir)/include

liballocator_la_LIBADD = $(MATH_LIBS) $(DL_LIBS)

liballocator_la_SOURCES = allocator.c
liballocator_la_SOURCES += constraint_funcs.c
liballocator_la_SOURCES += constraint_funcs.h
liballocator_la_SOURCES += driver_manager.c
liballocator_la_SOURCES += driver_manager.h
liballocator_la_SOURCES += cJSON/cJSON.c
liballocator_la_SOURCES += cJSON/cJSON.h
liballocator_la_SOURCES += constraints/lcm.c
liballocator_la_SOURCES += constraints/lcm.h
liballocator_la_SOURCES += constraints/address_alignment.c
liballocator_la_SOURCES += constraints/pitch_alignment.c
liballocator_la_SOURCES += constraints/max_pitch.c
