Example Usage
=============

Just wanted to try to capture some thoughts about how the usage of this API
might roughly look.

Note that we want to take into account scenarios where some processes may not
be permitted to open all devices.  For example a client of the display server
running in a sandbox, etc.  And we don't want window-system specific IPC
knowledge in "liballoc".

So each `alloc_dev_t` should be exposed to the user, and logic for
constructing the union of constraints and intersection of capabilities
should be decoupled from `alloc_dev_t`.  And of course, constraints/
capabilities should be easily serializable.

To keep things simple, a hypothetical example with a camera + display, in
single process but with some comments about where things would be serialized
in a multi-process scenario.

All error checking omitted for brevity.

Open Devices
------------

Generally an `alloc_dev_t` represents a single device file or API.  In cases
where the underlying device file is not exposed (such as OpenMAX or OpenCL,
or OpenGL when not running on top of gbm) an extension would be required to
retrieve the `alloc_dev_t` from that particular API.

```
alloc_dev_t *display = alloc_dev_create(open("/dev/dri/card0", O_RDWR));
alloc_dev_t *camera  = alloc_dev_create(open("/dev/video0", O_RDWR));
#ifdef HAVE_ION
/* for systems with ION, it could be used as an optional allocator device: */
alloc_dev_t *ion = alloc_dev_create(open("/dev/ion", O_RDWR));
#endif
```

Setup Usage
-----------

The `assertion_t` represents global generic properties of the buffer (width,
height, and "external" color format, ie. ignoring tiled/compressed/etc format
modifiers), plus per-device usage structs.

```
assertion_t assertion = {
    .width = 1920,
    .height = 1080,
    .format = NV12,
};

usage_t *display_usage = NULL, *camera_usage = NULL;

usage_add(&display_usage, &(usage_display_t) {
     .header = { VENDOR_BASE, USAGE_BASE_DISPLAY, 1 },
     .rotation_types = USAGE_BASE_DISPLAY_ROTATION_0 | USAGE_BASE_DISPLAY_ROTATION_180,
});
usage_add(&display_usage, &(usage_texture_t) {
     .header = { VENDOR_BASE, USAGE_BASE_TEXTURE, 0 },
});

usage_add(&camera_usage, &(user_write_t) {
     .header = { VENDOR_BASE, USAGE_BASE_WRITE, 0 },  /* generic usage for device writing to buffer */
});
```

Get Device Caps
---------------

Get per-device constraints and capabilities for a given usage.

```
capabilities_set_t *display_capabilities, *camera_capabilities;

alloc_dev_get_caps(display, &assertion, display_usage, &display_capabilites);
alloc_dev_get_caps(camera, &assertion, camera_usage, &camera_capabilites);
```

At this point, if (for example) camera was remote, the `alloc_dev_get_caps()`
call would happen in the remote process and caps sent to the local process.

Find Common Caps
----------------

If it is possible to share a buffer across various devices, we need to use
the union of all device's constraints, and intersection of all device's
capabilities.

Note that the helpers to find result caps are common (not provided by device)
and do not require a `alloc_dev_t`, since that may only be available in the
remote process.

```
capabilities_t *result_capabilities = display_capabilities;

combine_capabilities(&result_capabilities, camera_capabilities);
```

Allocate Buffer
---------------

Ask the allocator to create the buffer, it will go around and ask each
of the devices it knows about to do the allocation until one succeeds.

This means the allocation may be done on a device not involved in the
buffer sharing itself, like ION.

```
alloc_bo_t *buf;

buf = alloc_bo_create(&assertion, &result_capabilities);

assert(buf);
```

