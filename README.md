Unix Device Memory Allocator
============================

This document describes the design of a Unix Device Memory Allocation
library.  This repository contains an implementation of an allocator
based on the design outlined in this document.

**This is a living document.**  Please update it as the design of the
allocator code evolves.

High-Level Design
-----------------

The allocator library consists of two separate APIs and corresponding
ABIs:

1. An application-facing API and ABI

2. A driver/backend/plugin API and ABI

While the APIs will be similar, if not identical, they serve different
purposes.  The application API will be used by clients of the library
to (TODO: [JRJ] This list is incomplete and may contain non-concensus
items):

1. Enumerate target devices in the system

2. Open target devices.

3. Enumerate and filter capabilities and constraints of [open?] target
   devices based on asserted and requested usage.

4. Sort capabilities based on requested usage.

5. Allocate memory (images?) based on usage and a chosen set of
   capabilities.

6. Export handles for allocated memory that can be shared across
   process and API boundaries.

7. Import previously exported handles.

The driver API will handle roughly the same functions, but will be used
only within the allocation library to service requests from the
application API.

Terms
-----

The set of terms below is used throughout this design document and the
allocator implementation:

* **Device** - A single logical unit within a computer system.
  Generally, this will correspond to a single unix device node.  A
  device may contain mulitple engines or perform multiple functions,
  such as graphics processing and display scan out, or it may be
  dedicated to a single task such as display scan out or video
  encoding.  (TODO: [JRJ] Is there a better way to describe what is
  intended to be represented by a device?  Are there cases not covered
  by this description?)

* **Usage** - Usage is the description of which operations the
  application intends to perform on an allocation.  These include
  "asserted" or "global" usage, such as the width and height of an
  image, as well as "requested" or "local" usage such as graphics
  rendering, display scan out, or video encoding.  Asserted usage
  is state that must be supported by all devices an allocation will be
  used on.  Requested usage may be needed only on particular devices
  and the application may specify those devices along with the
  requested usage.  Generally, words used to name usages will be
  verbs, such as "render", "display", and "encode".

* **Constraint** - Constraints describe limitations of particular
  devices for a given set of usage.  Examples of common constraints are
  image pitch alignment, allocation address alignment, or memory bank
  spanning requirements.  Generally, constraints are defined using
  negative adjectives.  These are words that describe what a device can
  not do, or the minimum requirements that must be met to use the
  device.

* **Capability** - Capabilities describe properties of an allocation a
  device can support for a given set of usage.  Examples of capabilities
  include image tiling formats such as pitch/linear or device-specific
  swizzling and memory compression techniques.  In general, capabilities
  are defined using positive adjectives.  These are words that describe
  what a device can do.

* **Allocation** - A range of memory, its requested usage, and its
  chosen capabilities.

Requirements
------------

* The Usage, Constraint, and Capability data must be serializable such
  that it can be shared across process boundaries, building up image
  properties across multiple instances of the allocator library.

Capabilities Set Math
---------------------

A description of what it means to construct the union of constraints and
intersection of capabilities.

A capabilities set is an array of capabilities descriptors (each with one
or more `capabilities_header_t` plus associated payload if any) with a
corresponding `constraint_t` block.  For example:

device A:
```
  {FOO_TILED(32,64) | FOO_COMPRESSED}, <constraintsA1>
  {FOO_TILED(32,64)}, <constraintsA2>
  {BAR_TILED(16,16)}, <constraintsA3>
  {BASE_LINEAR}, <constraintsA4>
```
which means that device A supports TILED+COMPRESSED, or TILED, or LINEAR.

device B:
```
  {BAR_TILED(16,16)}, <constraintsB1>
  {BASE_LINEAR}, <constraintsB2>
```

intersection:
```
  {BAR_TILED(16,16)}, union(<constraintsA3>, <constraintsB1>)
  {BASE_LINEAR}, union(<constraintsA4>, <constraintsB2>)
```

TODO I guess if we used dataformat, maybe the capabilities block simply
becomes a dataformat block?  So a *Capabilities Set* is just a set of
pairs of dataformat block plus corresponding `constraint_t` block?

Acknowledgments
----------------

This project includes code from the following projects:

* **cJSON** - https://github.com/DaveGamble/cJSON

  Copyright (c) 2009-2017 Dave Gamble and cJSON contributors

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.

* **libglvnd** - https://github.com/NVIDIA/libglvnd

  Copyright (c) 2013, NVIDIA CORPORATION.

  Permission is hereby granted, free of charge, to any person obtaining a
  copy of this software and/or associated documentation files (the
  "Materials"), to deal in the Materials without restriction, including
  without limitation the rights to use, copy, modify, merge, publish,
  distribute, sublicense, and/or sell copies of the Materials, and to
  permit persons to whom the Materials are furnished to do so, subject to
  the following conditions:

  The above copyright notice and this permission notice shall be included
  unaltered in all copies or substantial portions of the Materials.
  Any additions, deletions, or changes to the original source files
  must be clearly indicated in accompanying documentation.

  If only executable code is distributed, then the accompanying
  documentation must state that "this software is based in part on the
  work of the Khronos Group."

  THE MATERIALS ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
  CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
  TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
  MATERIALS OR THE USE OR OTHER DEALINGS IN THE MATERIALS.
