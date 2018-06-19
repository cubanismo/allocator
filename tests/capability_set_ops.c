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
#include <stdbool.h>
#include <allocator/allocator.h>
#include <allocator/helpers.h>

#include "test_utils.h"

static void usage(void)
{
    printf("\nUsage: capability_set_ops [-d|--device] DEVICE0_FILE_NAME "
           "[[-d|--device] DEVICE1_FILE_NAME ...] [-v|--verbose]\n");
}

int main(int argc, char *argv[])
{
    static struct option long_options[] = {
        {"device",  required_argument, NULL, 'd'},
        {"verbose", no_argument,       NULL, 'v'},
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
            VENDOR_BASE,                            /* usage vendor */
            USAGE_BASE_TEXTURE,                     /* usage name */
            USAGE_LENGTH_IN_WORDS(usage_texture_t)  /* length_in_word */
        }
    };
    static usage_display_t display_usage = {
        { /* header */
            VENDOR_BASE,                            /* usage vendor */
            USAGE_BASE_DISPLAY,                     /* usage name */
            USAGE_LENGTH_IN_WORDS(usage_display_t)  /* length_in_word */
        },

        USAGE_BASE_DISPLAY_ROTATION_0
    };

    static usage_t uses[] = {
        {
            NULL,                   /* dev, overidden below */
            &texture_usage.header   /* usage */
        },
        {
            NULL,                   /* dev, overidden below */
            &display_usage.header   /* usage */
        }
    };
    static const uint32_t num_uses = sizeof(uses) / sizeof(usage_t);

    uint32_t num_assertion_hints = 0;
    assertion_hint_t *assertion_hints;

    uint32_t *num_capability_sets;
    capability_set_t **capability_sets;

    uint32_t tmp_num_sets[2];
    capability_set_t *tmp_sets[2];

    int opt;
    size_t num_devices = 0;
    int i, j;
    char **dev_file_names = NULL;
    char **temp;
    int *dev_fds;
    device_t **devs;
    bool verbose = false;

    while ((opt = getopt_long(argc, argv, "d:v", long_options, NULL)) != -1) {
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

        case 'v':
            verbose = true;
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

        for (j = 0; j < num_uses; j++) {
            uses[j].dev = devs[i];
        }

        /* Query assertion hints and use maximum surface size reported */
        if (device_get_assertion_hints(devs[i], num_uses, uses,
                                       &num_assertion_hints,
                                       &assertion_hints) ||
            (num_assertion_hints == 0)) {
            FAIL("Couldn't get assertion hints for given usage from "
                 "device %i\n", i);
        }

        assertion.width = assertion_hints[0].max_width;
        assertion.height = assertion_hints[0].max_height;

        free_assertion_hints(num_assertion_hints, assertion_hints);

        /* Query capabilities for a common usage case from the device */
        if (device_get_capabilities(devs[i], &assertion, num_uses, uses,
                                    &num_capability_sets[i],
                                    &capability_sets[i])) {
            FAIL("Couldn't get capabilities for given usage from device %i\n",
                 i);
        }

        /* Print initial capability sets */
        if (verbose) {
            uint32_t n;

            for (n = 0; n < num_capability_sets[i]; n++) {
                printf("Device %i - Set %d:\n", i, n);
                print_capability_set(&capability_sets[i][n]);
            }
        }
    }

    for (i = 0; i < num_devices; i++) {
        if (num_capability_sets[i]) {
            uint32_t n;

            /*
             * Ensure serializing and deserializing capability sets is an
             * identity operation.
             */
            for (n = 0; n < num_capability_sets[i]; n++) {
                void *data;
                size_t data_size;
                capability_set_t *tmp_set;

                if (serialize_capability_set(&capability_sets[i][n],
                                             &data_size,
                                             &data)) {
                    FAIL("Could not serialize a capability set\n");
                }

                if (deserialize_capability_set(data_size, data, &tmp_set)) {
                    FAIL("Could not deserialize a capability set\n");
                }

                if (compare_capability_sets(&capability_sets[i][n],
                                            tmp_set)) {
                    /* Print tmp_set to compare */
                    if (verbose) {
                        printf("Deserialized (Device %i - Set %d):\n", i, n);
                        print_capability_set(tmp_set);
                    }

                    FAIL("Serializing then deserializing a capability set "
                         "modified the set contents\n");
                }

                free_capability_sets(1, tmp_set);
                free(data);
            }

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
                /* Print tmp_sets to compare */
                if (verbose) {
                    for (n = 0; n < tmp_num_sets[0]; n++) {
                        printf("Derived (Device %i - Set %d):\n", i, n);
                        print_capability_set(&tmp_sets[0][n]);
                    }
                }

                FAIL("Deriving capabilities from two identical lists removed "
                     "or added sets\n");
            }

            for (n = 0; n < num_capability_sets[i]; n++) {
                if (compare_capability_sets(&capability_sets[i][n],
                                            &tmp_sets[0][n])) {
                    /* Print tmp_set to compare */
                    if (verbose) {
                        printf("Derived (Device %i - Set %d):\n", i, n);
                        print_capability_set(&tmp_sets[0][n]);
                    }

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

        if (verbose) {
            uint32_t n;
            for (n = 0; n < tmp_num_sets[next]; n++) {
                printf("Derived against device %i (set %d):\n", i, n);
                print_capability_set(&tmp_sets[next][n]);
            }
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
