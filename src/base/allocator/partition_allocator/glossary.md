# Glossary

This page describes some core terminology used in PartitionAlloc.
A weak attempt is made to present terms "in conceptual order" s.t.
each term depends mainly upon previously defined ones.

## Top-Level Terms

* **Partition**: A heap that is separated and protected both from other
  partitions and from non-PartitionAlloc memory. Each partition holds
  multiple buckets.
* **Bucket**: A collection of regions in a partition that contains
  similar-sized objects. For example, one bucket may hold objects of
  size (224,&nbsp;256], another (256,&nbsp;320], etc. Bucket size
  brackets are geometrically spaced,
  [going up to `kMaxBucketed`][max-bucket-comment].
* **Normal Bucket**: Any bucket whose size ceiling does not exceed
  `kMaxBucketed`. This is the common case in PartitionAlloc, and
  the "normal" modifier is often dropped in casual reference.
* **Direct Map (Bucket)**: Any allocation whose size exceeds `kMaxBucketed`.

Buckets consist of slot spans, organized as linked lists (see below).

## Pages

* **System Page**: A memory page defined by the CPU/OS. Commonly
  referred to as a "virtual page" in other contexts. This is typically
  4KiB, but it can be larger. PartitionAlloc supports up to 64KiB,
  though this constant isn't always known at compile time (depending
  on the OS).
* **Partition Page**: The most common granularity used by
  PartitionAlloc. Consists of exactly 4 system pages.
* **Super Page**: A 2MiB region, aligned on a 2MiB boundary. Not to
  be confused with OS-level terms like "large page" or "huge page",
  which are also commonly 2MiB. These have to be fully committed /
  uncommitted in memory, whereas super pages can be partially committed
  with system page granularity.

## Slots and Spans

* **Slot**: An indivisible allocation unit. Slot sizes are tied to
  buckets. For example, each allocation that falls into the bucket
  (224,&nbsp;256] would be satisfied with a slot of size 256. This
  applies only to normal buckets, not to direct map.
* **Slot Span**: A run of same-sized slots that are contiguous in
  memory. Slot span size is a multiple of partition page size, but it
  isn't always a multiple of slot size, although we try hard for this
  to be the case.
  * **Small Bucket**: Allocations up to 4 partition pages. In these
    cases, slot spans are always between 1 and 4 partition pages in
    size. For each slot span size, the slot span is chosen to minimize
    number of pages used while keeping the rounding waste under a
    reasonable limit.
    * For example, for a slot size 96, 64B waste is deemed acceptable
      when using a single partition page, but for slot size
      384, the potential waste of 256B wouldn't be, so 3 partition pages
      are used to achieve 0B waste.
    * PartitionAlloc may avoid waste by lowering the number of committed
      system pages compared to the number of reserved pages. For
      example, for the slot size of 896B we'd use a slot span of 2
      partition pages of 16KiB, i.e. 8 system pages of 4KiB, but commit
      only up to 7, thus resulting in perfect packing.
  * **Single-Slot Span**: Allocations above 4 partition pages (but
    &le;`kMaxBucketed`). This is because each slot span is guaranteed to
    hold exactly one slot.
    * Fun fact: there are sizes &le;4 partition pages that result in a
      slot span having exactly 1 slot, but nonetheless they're still
      classified as small buckets. The reason is that single-slot spans
      are often handled by a different code path, and that distinction
      is made purely based on slot size, for simplicity and efficiency.

## Other Terms

* **Object**: A chunk of memory returned to the allocating invoker
  of the size requested. It doesn't have to span the entire slot,
  nor does it have to begin at the slot start. This term is commonly
  used as a parameter name in PartitionAlloc code, as opposed to
  `slot_start`.
* **Thread Cache**: A [thread-local structure][pa-thread-cache] that
  holds some not-too-large memory chunks, ready to be allocated. This
  speeds up in-thread allocation by reducing a lock hold to a
  thread-local storage lookup, improving cache locality.
* **GigaCage**: A memory region several gigabytes wide, reserved by
  PartitionAlloc upon initialization, from which all allocations are
  taken. The motivation for GigaCage is for code to be able to examine
  a pointer and to immediately determine whether or not the memory was
  allocated by PartitionAlloc. This provides support for a number of
  features, including
  [StarScan][starscan-readme] and
  [BackupRefPtr][brp-doc].
  * Note that GigaCage only exists in builds with 64-bit pointers.
  * In builds with 32-bit pointers, PartitionAlloc tracks pointers
    it dispenses with a bitmap. This is often referred to as "fake
    GigaCage" (or simply "GigaCage") for lack of a better term.
* **Payload**: The usable area of a super page in which slot spans
  reside. While generally this means "everything between the first
  and last guard partition pages in a super page," the presence of
  other metadata (e.g. StarScan bitmaps) can bump the starting offset
  forward. While this term is entrenched in the code, the team
  considers it suboptimal and is actively looking for a replacement.

## PartitionAlloc-Everywhere

Originally, PartitionAlloc was used only in Blink (Chromium's rendering engine).
It was invoked explicitly, by calling PartitionAlloc APIs directly.

PartitionAlloc-Everywhere is the name of the project that brought PartitionAlloc
to the entire-ish codebase (exclusions apply). This was done by intercepting
`malloc()`, `free()`, `realloc()`, aforementioned `posix_memalign()`, etc. and
routing them into PartitionAlloc. The shim located in
`base/allocator/allocator_shim_default_dispatch_to_partition_alloc.h` is
responsible for intercepting. For more details, see
[base/allocator/README.md](../../../base/allocator/README.md).

A special, catch-it-all *Malloc* partition has been created for the intercepted
`malloc()` et al. This is to isolate from already existing Blink partitions.
The only exception from that is Blink's *FastMalloc* partition, which was also
catch-it-all in nature, so it's perfectly fine to merge these together, to
minimize fragmentation.

As of 2022, PartitionAlloc-Everywhere is supported on

* Windows 32- and 64-bit
* Linux
* Android 32- and 64-bit
* macOS
* Fuchsia

[max-bucket-comment]: https://source.chromium.org/chromium/chromium/src/+/main:base/allocator/partition_allocator/partition_alloc_constants.h;l=345;drc=667e6b001f438521e1c1a1bc3eabeead7aaa1f37
[pa-thread-cache]: https://source.chromium.org/chromium/chromium/src/+/main:base/allocator/partition_allocator/thread_cache.h
[starscan-readme]: https://chromium.googlesource.com/chromium/src/+/main/base/allocator/partition_allocator/starscan/README.md
[brp-doc]: https://docs.google.com/document/d/1m0c63vXXLyGtIGBi9v6YFANum7-IRC3-dmiYBCWqkMk/preview
