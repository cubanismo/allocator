/*
 * Copyright (c) 2016 NVIDIA CORPORATION.  All rights reserved.
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

#ifndef __ALLOCATOR_COMMON_H__
#define __ALLOCATOR_COMMON_H__

#include <stdint.h>

/*! TODO: Namespace everything when we settle on a project name (libgbm2?) */

/*!
 * \file Allocator definitions and declarations shared between the application
 *       and driver APIs
 */

/*!
 * Vendor IDs
 *
 * Vendor IDs are used to establish namespaces where device manufacturers and
 * driver authors may define vendor-specific extensions and allocation
 * properties.  The special vendor VENDOR_BASE is used to define a global
 * namespace that is expected to be understood by all driver vendors.  Vendors
 * may reference and parse properties and extensions from eachother's
 * namespaces as well, but applications should not rely on such interopation
 * in general.
 *
 * Vendors should register their vendor ID by adding it here.  The suggested
 * value is the same as the vendor's Vulkan vendor ID if it has one, which is
 * generally the vendor's PCI vendor ID or a value of the form 0x0001XXXX
 * registered with Khronos.  If the vendor does not have a PCI vendor ID or
 * a Vulkan vendor ID registered with Khronos, please use the first available
 * ID of the form 0xFFFFXXXX.
 *
 * For clarity, keep the vendor ID list in numerical order.
 */
#define VENDOR_BASE                             0x00000000
#define VENDOR_NVIDIA                           0x000010DE
#define VENDOR_ARM                              0x000013B5

/*!
 * Opaque pointer to a device handle
 */
typedef struct device *device_t;
#define DEVICE_NONE ((device_t)0)

/*!
 * \defgroup constraints
 * @{
 */

typedef struct constraint {
    uint32_t name;

    /*!
     * TODO: [JRJ] Is it portable to send unions of this form over the
     *       wire using a simple memcpy()/write()?
     */
    union {
        /*! CONSTRAINT_ADDRESS_ALIGNMENT */
        struct {
            uint64_t value;
        } address_alignment;

        /*! CONSTRAINT_PITCH_ALIGNMENT */
        struct {
            uint32_t value;
        } pitch_alignment;

        /*! CONSTRAINT_MAX_PITCH */
        struct {
            uint32_t value;
        } max_pitch;
    } u;
} constraint_t;
#define CONSTRAINT_ADDRESS_ALIGNMENT                                0x00000000
#define CONSTRAINT_PITCH_ALIGNMENT                                  0x00000001
#define CONSTRAINT_MAX_PITCH                                        0x00000002

/*!
 * @}
 * End of the constraint group
 */

/*!
 * Common header for usage and capabilities
 */
typedef struct header {
    uint32_t vendor;
    uint16_t name;
    uint16_t length_in_words;
} header_t;

/*!
 * \defgroup capabilities
 * @{
 */

typedef struct header capability_header_t;

/*!
 * The ability to represent 2D images using pitch x height pixel layout.
 *
 * This is a binary capability with no additional properties, so its mere
 * presence is sufficient to express it.  No additional fields beyond the
 * header are needed.
 */
typedef struct capability_pitch_linear {
    capability_header_t header; // { VENDOR_BASE, CAP_BASE_PITCH_LINEAR, 0 }
} capability_pitch_linear_t;
#define CAP_BASE_PITCH_LINEAR 0x0000

/*!
 * Capability sets are made up of zero or more constraints and one or more
 * capability descriptors
 *
 * Device capabilities and constraints can not be mixed arbitrarily.  For
 * example, a device may support pitch linear tiling, proprietary tiling,
 * and image compression, but not all independently.  Compression may only
 * be available when using certain proprietary tiling capabilities.
 * Therefore, capabilities must be reported and compared as immutable sets.
 *
 * Constraints need to be included in capability sets because they may be
 * specific to a set of capabilities.  For example, a device may have one
 * address alignment requirement for pitch linear, but another requirement
 * for proprietary tiling.
 */
typedef struct capability_set {
    uint32_t num_constraints;
    uint32_t num_capabilities;
    const constraint_t *constraints;
    const capability_header_t *capabilities;
} capability_set_t;

/*!
 * @}
 * End of the capabilities group
 */

/*!
 * \defgroup usage
 * @{
 */

typedef struct header usage_header_t;

/*!
 * Request to support sampling from a 2D image using a GPU's texture units.
 */
typedef struct usage_texture {
    usage_header_t header; // { VENDOR_BASE, USAGE_BASE_TEXTURE, 0 }
} usage_texture_t;
#define USAGE_BASE_TEXTURE 0x0000

/*!
 * Request to support displaying a 2D image at the specified rotation.
 */
typedef struct usage_display {
    usage_header_t header; // { VENDOR_BASE, USAGE_BASE_DISPLAY, 1 }
    uint32_t rotation_types;
} usage_display_t;
#define USAGE_BASE_DISPLAY 0x0001
/* 2 bits to describe rotation */
#define USAGE_BASE_DISPLAY_ROTATION_0           0x00000000
#define USAGE_BASE_DISPLAY_ROTATION_90          0x00000001
#define USAGE_BASE_DISPLAY_ROTATION_180         0x00000002
#define USAGE_BASE_DISPLAY_ROTATION_270         0x00000003
/* mirror flag */
#define USAGE_BASE_DISPLAY_MIRROR               0x00000004

/*!
 * Structure to specify a single usage atom.
 *
 * Usage is always specified relative to a device.  If the
 * application wishes to specify a usage on all devices,
 * it can specify DEVICE_NONE as the device.
 */
typedef struct usage {
    device_t dev;
    const usage_header_t *usage;
} usage_t;

/*!
 * @}
 * End of the usage group
 */

/*!
 * \defgroup assertions
 * @{
 */

/*!
 * An assertion is the parameters the application supplies when requesting
 * a surface allocation, or when requesting capabilities.  The parameters
 * here are different from requested usage in that they are requirements.
 * In other words, it is not expected that the application will retry with
 * different values for these parameters if the returned capability set is
 * zero, whereas usage is something that is intended to be negotiated via
 * several capability requests.  As such, these should be kept to a minimum.
 */
typedef struct assertion {
    /*! Required surface width */
    uint32_t width;

    /*! Required surface height */
    uint32_t height;

    /*! Required surface pixel format.
     *
     * TODO: Non-concensus!  Decide if this is Khronos data format or fourcc
     */
    const uint32_t *format;

    /*!
     * To handle extended assertions, define a new structure whose first
     * member is a value describing its type, and point to it here.
     */ 
    void *ext;
} assertion_t;

/*!
 * @}
 * End of the assertions group
 */

#endif /* __ALLOCATOR_H__ */
