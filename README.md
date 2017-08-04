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

Constraint Merging
------------------

Constraint lists are joined by a merge operation.  Given two lists of
constraints, merging them results in a new list with at least as many
entries as the longer of the two input lists.  If a given constraint,
identified by its name, is in only one of the two original lists, it is
included verbatim in the resulting list.  If a constraint appears in
both lists, the two constraint values are merged and only the resulting
constraint is included in the merged list.

Capability Filtering
--------------------

As additive constructs, capability lists are naively merged via
intersection, with one caveat:  Any entry in a list can be marked as
"required", meaning if the intersection operation removes it from the
resulting list, the resulting list is invalid and the intersection
operation has failed.

Capability comparison is also naive.  While capabilities themselves are
opaque to the library beyond their header content, the library can
compare the non-header content using memcmp().  Note, however, that the
above-mentioned "required" flag is ignored when comparing capabilities and
set if true in either of the matching capabilities in the resulting list,
since its value should not alter an allocation operation.  Because
compilers may introduce padding in structures to account for architectural
alignment requirements or preferences, care must be taken to ensure memcpy-
based comparisons are reliable despite this invisible data.  Consequently,
all capability struct allocation should be done using calloc() or an
equivalent function.

Because capabilities in the list can be arbitrarily culled, all hard
dependencies between capabilities in a given list must be expressible using
the simple "required" flag.

An alternate design considered was to allow general interdependence between
capabilities in a given list, and allow any two sets of "compatible"
capabilities to be merged into a single list, where compatibility could
be certified by the vendor of either of the two capabilities in
question.  This design initially seemed sound and is more powerful and
flexible than the final design.

Consider the case of two capability sets, one corresponding to a
particular device or engine which enables use of a particular local
cache, and another which corresponds to a separate engine or device
without access to that and hence doesn't include the corresponding
capability.  Assuming the device or engine corresponding to the set with
access to the cache is capable of flushing the cache, the lack of
support for that cache in the other set need not disqualify
compatibiliy.  However, note the device or engine exposing the cache
capability must take care to include a cache flush when when building a
transition to a usage that involves other engines or other devices not
capable of accessing the cache.

This ability to match non-identical combined with the ability for
drivers to report vendor-specific capabilities that are opaque to other
vendors further complicates the capability intersection logic.  Consider
again the above cache capability.  Assume it arises while intersecting
capability sets from two vendors, A and B, where vendor A is the one
reporting the cache capability.  Vendor A can trivially succeed
intersecting the cache capability with any other capability, but Vendor
B has no way to know if the opaque capability is compatible with any of
its capabilities.  Because of this, each capability comparison must be
broadcast to both vendors.  The final result is the logical OR of each
vendor's result.

Despite it's apparent power, this alternative design proves unworkable
though when considering how an allocation would be realized based on the
resulting capability set.  Since the set would potentially contain more
capabilities than any one vendor was familiar with, potentially even
vendor-specific capabilities from multiple vendors, it's not clear any
one vendor would know how to properly create an allocation encompassing
all the requested capabilities.

A variation of this alternative design is to require exact set matches,
expose properties such as cache capabilities in both sets, and rely on
the transition flushing logic to handle the transition.  However, this
is infeasible when the caches are vendor specific, as they likely will
be for all but the special CPU cache special case.  It is unlikely
vendor A would know when to include the capability to use vendor B's on-
chip or on-engine cache in its capability sets.

Another alternative implementation is to simply dispatch intersection to
application-chosen vendors directly, providing them with the sets in
full.  This allows tests based on combinations of capabilities rather
than the simple atomic comparisons enabled by the above design.
However, it moves a great deal of complexity form the common code to the
drivers.  Additionally, it could ideally be asserted that if such a
combination comparison is necessary to determine compatibility, the
capabilities in question are not in fact distinct atoms and instead
should be combined into one, larger capability object.  Whether this
assertion holds up in practice remains to be seen.

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
