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

While the APIs will be similar, if not identical, they server different
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

