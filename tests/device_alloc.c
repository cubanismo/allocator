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

/* For getopt_long */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <allocator/allocator.h>

static void FAIL(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    
    exit(1);
}

static void usage(void)
{
    printf("\nUsage: device_alloc [-d|--device] DEVICE_FILE_NAME\n");
}

int main(int argc, char *argv[])
{
    static struct option long_options[] = {
        {"device", required_argument, NULL, 'd'},
        {NULL, 0, NULL, 0}
    };

    int opt;
    char *dev_file_name = NULL;
    int dev_fd;
    device_t *dev;

    while ((opt = getopt_long(argc, argv, "d:", long_options, NULL)) != -1) {
        switch (opt) {
        case 'd':
            dev_file_name = strdup(optarg);
            if (!dev_file_name) {
                FAIL("Failed to make a copy of the device string\n");
            }
            break;

        case '?':
            usage();
            exit(1);

        default:
            FAIL("Invalid option\n");
            break;
        }
    }

    if (!dev_file_name) {
        usage();
        exit(1);
    }

    dev_fd = open(dev_file_name, O_RDWR);

    if (dev_fd < 0) {
        FAIL("Couldn't open device file %s\n", dev_file_name);
    }

    dev = device_create(dev_fd);

    if (!dev) {
        FAIL("Couldn't create allocator device from device FD\n");
    }

    device_destroy(dev);

    close(dev_fd);

    printf("Success\n");

    return 0;
}
