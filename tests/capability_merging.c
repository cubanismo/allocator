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
#include <stdio.h>
#include <allocator/allocator.h>

#include "test_utils.h"

static void usage(void)
{
    printf("\nUsage: capability_merging [-d|--device] DEVICE0_FILE_NAME "
           "[[-d|--device] DEVICE1_FILE_NAME ...]\n");
}

int main(int argc, char *argv[])
{
    static struct option long_options[] = {
        {"device", required_argument, NULL, 'd'},
        {NULL, 0, NULL, 0}
    };

    static assertion_t assertion = {
        256,            /* width */
        256,            /* height */
        NULL,           /* format */
        NULL            /* ext */
    };

    static usage_texture_t texture_usage = {
        { /* header */
            VENDOR_BASE,        /* usage vendor */
            USAGE_BASE_TEXTURE, /* usage name */
            0,                  /* length_in_word */
        }
    };

    static usage_t uses = {
        NULL,                   /* dev, overidden below */
        &texture_usage.header   /* usage */
    };
    static const uint32_t num_uses = 1;

    uint32_t num_assertion_hints = 0;
    assertion_hint_t *assertion_hints;

    uint32_t *num_capability_sets;
    capability_set_t **capability_sets;

    uint32_t tmp_num_sets[2];
    capability_set_t *tmp_sets[2];

    int opt;
    size_t num_devices = 0;
    size_t i;
    char **dev_file_names = NULL;
    char **temp;
    int *dev_fds;
    device_t **devs;

    while ((opt = getopt_long(argc, argv, "d:", long_options, NULL)) != -1) {
        switch (opt) {
        case 'd':
            dev_file_names = realloc(dev_file_names,
                                     sizeof(dev_file_names[0]) *
                                     (num_devices + 1));

            if (!dev_file_names) {
                FAIL("Not enough memory for device file name list\n");
            }

            dev_file_names[num_devices] = strdup(optarg);
            if (!dev_file_names[num_devices]) {
                FAIL("Failed to make a copy of the device string\n");
            }

            num_devices++;
            break;

        case '?':
            usage();
            exit(1);

        default:
            FAIL("Invalid option\n");
            break;
        }
    }

    if (!num_devices) {
        usage();
        exit(1);
    }

    dev_fds = malloc(sizeof(dev_fds[0]) * num_devices);
    devs = malloc(sizeof(devs[0]) * num_devices);
    num_capability_sets = malloc(sizeof(num_capability_sets[0]) * num_devices);
    capability_sets = malloc(sizeof(capability_sets[0]) * num_devices);

    if (!dev_fds || !devs || !num_capability_sets || !capability_sets) {
        FAIL("Couldn't allocate memory for device lists\n");
    }

    for (i = 0; i < num_devices; i++) {
        dev_fds[i] = open(dev_file_names[i], O_RDWR);

        if (dev_fds[i] < 0) {
            FAIL("Couldn't open device file %s\n", dev_file_names[i]);
        }

        devs[i] = device_create(dev_fds[i]);

        if (!devs[i]) {
            FAIL("Couldn't create allocator device from device FD\n");
        }

        uses.dev = devs[i];

        /* Query assertion hints and use maximum surface size reported */
        if (device_get_assertion_hints(devs[i], num_uses, &uses,
                                       &num_assertion_hints,
                                       &assertion_hints) ||
            (num_assertion_hints == 0)) {
            FAIL("Couldn't get assertion hints for given usage from "
                 "device %zd\n", i);
        }

        assertion.width = assertion_hints[0].max_width;
        assertion.height = assertion_hints[0].max_height;

        free_assertion_hints(num_assertion_hints, assertion_hints);

        /* Query capabilities for a common usage case from the device */
        if (device_get_capabilities(devs[i], &assertion, num_uses, &uses,
                                    &num_capability_sets[i],
                                    &capability_sets[i])) {
            FAIL("Couldn't get capabilities for given usage from device %zd\n",
                 i);
        }

        if (num_capability_sets[i]) {
            uint32_t n;

            /*
             * Ensure deriving capabilities from two identical lists of sets is
             * an identity operation.
             */
            if (derive_capabilities(num_capability_sets[i],
                                    capability_sets[i],
                                    num_capability_sets[i],
                                    capability_sets[i],
                                    &tmp_num_sets[0],
                                    &tmp_sets[0])) {
                FAIL("Couldn't derive capabilities from identical set lists\n");
            }

            if (tmp_num_sets[0] != num_capability_sets[i]) {
                FAIL("Deriving capabilities from two identical lists removed "
                     "or added sets\n");
            }

            for (n = 0; n < num_capability_sets[i]; n++) {
                if (compare_capability_sets(&capability_sets[i][n],
                                            &tmp_sets[0][n])) {
                    FAIL("Deriving capabilities from two identical lists "
                         "was not an identity operation\n");
                }
            }

            free_capability_sets(tmp_num_sets[0], tmp_sets[0]);
            tmp_sets[0] = NULL;
            tmp_num_sets[0] = 0;
        }
    }

    tmp_sets[0] = capability_sets[0];
    tmp_num_sets[0] = num_capability_sets[0];
    for (i = 1; i < num_devices; i++) {
        size_t last = !(i % 2);
        size_t next = i % 2;
        if (derive_capabilities(tmp_num_sets[last],
                                tmp_sets[last],
                                num_capability_sets[i],
                                capability_sets[i],
                                &tmp_num_sets[next],
                                &tmp_sets[next])) {
            FAIL("Couldn't derive capabilities\n");
        }

        free_capability_sets(tmp_num_sets[last], tmp_sets[last]);
        tmp_sets[last] = NULL;
        tmp_num_sets[last] = 0;
    }

    for (i = 0; i < num_devices; i++) {
        device_destroy(devs[i]);

        close(dev_fds[i]);
    }

    printf("Success\n");

    return 0;
}
