# Copyright (c) 2017 NVIDIA CORPORATION.  All rights reserved.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

SUBDIRS = src include
ACLOCAL_AMFLAGS = -I m4
EXTRA_DIST = autogen.sh README.md

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = liballocator.pc

HEADERS_TO_TEST = \
	allocator/common.h \
	allocator/allocator.h

LOCAL_TARGETS = test_headers.timestamp
LOCAL_CLEAN = $(top_builddir)/test_headers.timestamp

test_headers.timestamp: $(add_prefix $(top_srcdir),$(HEADERS_TO_TEST))
	for HEADER in $(HEADERS_TO_TEST); do \
		echo -n '#include "' >> .test.c; \
		echo -n $$HEADER >> .test.c; \
		echo '"' >> .test.c; \
	done
	$(CC) -I$(top_srcdir)/include -Wall -Werror -c .test.c -o .test.o
	rm .test.o .test.c
	echo Timestamp > test_headers.timestamp

if HAVE_DOXYGEN
LOCAL_TARGETS += doxygen/doxygen.timestamp
LOCAL_CLEAN += $(top_builddir)/doxygen

doxygen/doxygen.timestamp:
	$(DOXYGEN) Doxyfile
	echo Timestamp > doxygen/doxygen.timestamp
endif

all-local: $(LOCAL_TARGETS)

clean-local:
	rm -rf $(LOCAL_CLEAN)
