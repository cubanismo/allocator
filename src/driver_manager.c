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

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <dlfcn.h>
#include <dirent.h>
#include <fnmatch.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <assert.h>
#include <allocator/allocator.h>
#include <allocator/driver.h>
#include "driver_manager.h"
#include "cJSON/cJSON.h"

/*!
 * A linked list of all the available driver instances.
 *
 * TODO Make access to this thread_safe
 * TODO Use an existing linked-list implementation instead of open-coding
 */
driver_t *driver_list = NULL;

/*! Non-zero if driver enumeration & initialization has been run */
int drivers_initialized = 0;

/*!
 * Run a driver's destructor and unload its library.
 *
 * \param[in] driver A pointer to a driver instance.
 */
static void remove_one_driver(driver_t *driver)
{
    if (driver) {
        for (driver_t **d = &driver_list; *d; d = &(*d)->next) {
            if (*d == driver) {
                *d = driver->next;
                break;
            }
        }

        if (driver->destroy) {
            driver->destroy(driver);
        }

        if (driver->lib_handle) {
            dlclose(driver->lib_handle);
        }

        free(driver);
    }
}

/*!
 * Load and initialize one driver library.
 *
 * \param[in] driver_file The driver library file name.
 *
 * \return 0 on success, -1 on failure.
 *
 * TODO Need to define error propagation policy.  Should this and other
 *      functions return a more detailed error code and/or set errno?
 */
static int add_one_driver(const char *driver_file)
{
    driver_init_func_t driver_init_func;
    driver_t *driver = calloc(1, sizeof(driver_t));
    driver_t **d;
    int ret = 0;

    if (!driver) {
        ret = -1;
        goto done;
    }

    driver->driver_interface_version = DRIVER_INTERFACE_VERSION;

    driver->lib_handle = dlopen(driver_file, RTLD_LAZY);

    if (!driver->lib_handle) {
        ret = -1;
        goto done;
    }

    driver_init_func = (driver_init_func_t)dlsym(driver->lib_handle,
                                                 DRIVER_INIT_FUNC);

    if (!driver_init_func) {
        ret = -1;
        goto done;
    }

    if (driver_init_func(driver) < 0) {
        ret = -1;
        goto done;
    }

    /*
     * The intention is that the allocator library be both backwards and
     * forwards compatible, so this logic should change once a stable ABI
     * is declared.  For now, there's no way to be compatible with a prior
     * version because this code is the first version of the driver
     * interface.
     */
    if (driver->driver_interface_version < DRIVER_INTERFACE_VERSION) {
        ret = -1;
        goto done;
    }

    if (!driver->is_fd_supported ||
        !driver->device_create_from_fd) {
        ret = -1;
        goto done;
    }

    /*
     * Because the ordering of drivers affects system behavior, the driver needs
     * to be appended rather than prepended to the list to preserve the
     * configuration directory sort order.
     */
    for (d = &driver_list; *d; d = &(*d)->next);
    *d = driver;

done:
    if (ret < 0) {
        remove_one_driver(driver);
    }

    return ret;
}

static int check_json_format_version(const char *version_string)
{
    int major;
    int minor;
    int micro;
    int length;

    major = minor = micro = -1;
    length = sscanf(version_string, "%d.%d.%d", &major, &minor, &micro);

    /* A major version must be specified */
    if (length < 1) {
        return -1;
    }

    if (length < 2) {
        minor = 0;
    }

    if (length < 3) {
        micro = 0;
    }

    if (major != JSON_FILE_VERSION_MAJOR) {
        return -1;
    }

    if (minor > JSON_FILE_VERSION_MINOR) {
        return -1;
    }

    return 0;
}

/*!
 * Load and initialize the driver defined by a single JSON driver config file.
 *
 * \param[in] driver_json_file A driver JSON config file.
 *
 * \return 0 on success, -1 on failure.
 */
static int add_one_driver_from_config(const char *driver_json_file)
{
    int ret = 0;
    char *file_buf = NULL;
    cJSON *json_root = NULL;
    cJSON *format_node;
    cJSON *driver_node;
    cJSON *driver_path_node;
    struct stat stats;
    FILE *fp = fopen(driver_json_file, "r");

    if (!fp) {
        ret = -1;
        goto done;
    }

    if (fstat(fileno(fp), &stats)) {
        ret = -1;
        goto done;
    }

    file_buf = malloc(stats.st_size + 1);

    if (!file_buf) {
        ret = -1;
        goto done;
    }

    if (fread(file_buf, stats.st_size, 1, fp) != 1) {
        ret = -1;
        goto done;
    }

    json_root = cJSON_Parse(file_buf);

    if (!json_root) {
        ret = -1;
        goto done;
    }

    format_node = cJSON_GetObjectItem(json_root, "file_format_version");
    if (!format_node ||
        (format_node->type != cJSON_String) ||
        check_json_format_version(format_node->valuestring)) {
        ret = -1;
        goto done;
    }

    driver_node = cJSON_GetObjectItem(json_root, "allocator_driver");
    if (!driver_node ||
        (driver_node->type != cJSON_Object)) {
        ret = -1;
        goto done;
    }

    driver_path_node = cJSON_GetObjectItem(driver_node, "library_path");
    if (!driver_path_node ||
        (driver_path_node->type != cJSON_String)) {
        ret = -1;
        goto done;
    }

    ret = add_one_driver(driver_path_node->valuestring);

done:
    cJSON_Delete(json_root);

    free(file_buf);

    if (fp) {
        fclose(fp);
    }

    return ret;
}

static int scandir_filter(const struct dirent *ent)
{
    /* Ignore the entry if we know that it's not a regular file or symlink */
    if ((ent->d_type != DT_REG) &&
        (ent->d_type != DT_LNK) &&
        (ent->d_type != DT_UNKNOWN)) {
        return 0;
    }

    /* Otherwise, select any JSON files */
    if (fnmatch("*.json", ent->d_name, 0) == 0) {
        return 1;
    } else {
        return 0;
    }
}

/*!
 * Helper function to compare two filenames without taking locale into account
 *
 * This is used in place of alphasort() to avoid loading drivers in a different
 * order depending on the locale of the machine, which would be a pain to
 * debug if it ever caused problems.
 */
static int compare_filenames(const struct dirent **ent1,
                             const struct dirent **ent2)
{
    return strcmp((*ent1)->d_name, (*ent2)->d_name);
}

/*!
 * Enumerate driver JSON config files in the specified directory, and load the
 * drivers they refer to.
 *
 * \param[in] dir_name A directory path.
 *
 * \return 0 on success, -1 on failure.  Note success does not mean any drivers
 *         were found.
 */
static int add_drivers_in_dir(const char *dir_name)
{
    struct dirent **entries = NULL;
    int ret = 0;
    const char *path_separator;
    static const char *PATH_FMT = "%s%s%s";
    int i;
    int count = scandir(dir_name, &entries, scandir_filter, compare_filenames);
    size_t dir_name_len;

    if (count < 0 ) {
        /* Skip non-existent directories */
        ret = (errno == ENOENT) ? 0 : -1;
        goto done;
    } else if (count == 0) {
        ret = 0;
        goto done;
    }

    dir_name_len = strlen(dir_name);

    if ((dir_name_len > 0) && (dir_name[dir_name_len - 1] != '/')) {
        path_separator = "/";
    } else {
        path_separator = "";
    }

    for (i = 0; i < count; i++) {
        char *path;
        int load_result;
        int path_len = snprintf(NULL, 0, PATH_FMT,
                                dir_name, path_separator, entries[i]->d_name);

        if (path_len < 0 ) {
            ret = -1;
            goto done;
        }

        /* Since only files ending in json were found, path_len should be > 0 */
        assert(path_len > 0);

        path = malloc(path_len + 1);

        if (!path) {
            ret = -1;
            goto done;
        }

        snprintf(path, path_len + 1, PATH_FMT,
                 dir_name, path_separator, entries[i]->d_name);

        load_result = add_one_driver_from_config(path);

        free(path);

        if (load_result < 0) {
            ret = load_result;
            goto done;
        }
    }

done:
    free(entries);

    return ret;
}

/*!
 * Enumerate, load, and initialize all available driver libraries on the system
 *
 * \return 0 on success, -1 on failure.  Not success does not indicate any
 *         drivers were found.
 */

#define ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))

static const char *DEFAULT_SYS_CONF_DIRS[] = {
    "/usr/share/liballocator",
    "/usr/local/share/liballocator"
};

static const char *DEFAULT_USR_CONF_DIRS[] = {
    ".liballocator"
};

static int init_drivers(void)
{
    if (!drivers_initialized) {
        int i;

        drivers_initialized = 1;

        /* Load drivers under default system configuration directories */
        for (i = 0; i < ARRAY_LEN(DEFAULT_SYS_CONF_DIRS); i++) {
            if (add_drivers_in_dir(DEFAULT_SYS_CONF_DIRS[i]) < 0) {
                return -1;
            }
        }

        /* If not running as setuid, also try user configuration directories */
        if (getuid() == geteuid()) {
            const char *home = getenv("HOME");
            const char *usr_extra = getenv("__LIBALLOCATOR_EXTRA_CONF_DIRS");

            /* Default user configuration directories */
            for (i = 0; i < ARRAY_LEN(DEFAULT_USR_CONF_DIRS); i++) {
                char tmp[PATH_MAX];
                strcpy(tmp, home);
                strcat(tmp, "/");
                strcat(tmp, DEFAULT_USR_CONF_DIRS[i]);
                if (add_drivers_in_dir(tmp) < 0) {
                    return -1;
                }
            }

            /* User-defined extra directories */
            if (usr_extra) {
                char *tmp = strdup(usr_extra);
                char *dir = strtok(tmp, ":");
                while (dir) {
                    if (add_drivers_in_dir(dir) < 0) {
                        free(tmp);
                        return -1;
                    }
                    dir = strtok(NULL, ":");
                }
                free(tmp);
            }
        }
    }

    return 0;
}

/*!
 * Given a file descriptor, attempt to find a driver that supports it.
 *
 * Initializes any available drivers and then scans through them in the
 * order they were enumerated.
 *
 * \param[in] fd The file descriptor for which a driver is requested.
 *
 * \return An initialized driver instance on success, NULL on failure.
 */
driver_t *find_driver_for_fd(int fd)
{
    if (init_drivers() < 0) {
        return NULL;
    }

    for (driver_t *driver = driver_list; driver; driver = driver->next) {
        if (driver->is_fd_supported(driver, fd)) {
            return driver;
        }
    }

    return NULL;
}
