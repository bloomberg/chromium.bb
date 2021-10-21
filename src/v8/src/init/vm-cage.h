// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INIT_VM_CAGE_H_
#define V8_INIT_VM_CAGE_H_

#include "include/v8-internal.h"
#include "src/base/bounded-page-allocator.h"
#include "src/common/globals.h"

namespace v8 {

class PageAllocator;

namespace internal {

#ifdef V8_VIRTUAL_MEMORY_CAGE

/**
 * V8 Virtual Memory Cage.
 *
 * When the virtual memory cage is enabled, V8 will reserve a large region of
 * virtual address space - the cage - and place most of its objects inside of
 * it. This allows these objects to reference each other through offsets rather
 * than raw pointers, which in turn makes it harder for an attacker to abuse
 * them in an exploit.
 *
 * The pointer compression region, which contains most V8 objects, and inside
 * of which compressed (32-bit) pointers are used, is located at the start of
 * the virtual memory cage. The remainder of the cage is mostly used for memory
 * buffers, in particular ArrayBuffer backing stores and WASM memory cages.
 *
 * It should be assumed that an attacker is able to corrupt data arbitrarily
 * and concurrently inside the virtual memory cage. The heap sandbox, of which
 * the virtual memory cage is one building block, attempts to then stop an
 * attacker from corrupting data outside of the cage.
 *
 * As the embedder is responsible for providing ArrayBuffer allocators, v8
 * exposes a page allocator for the virtual memory cage to the embedder.
 *
 * TODO(chromium:1218005) come up with a coherent naming scheme for this class
 * and the other "cages" in v8.
 */
class V8_EXPORT_PRIVATE V8VirtualMemoryCage {
 public:
  // +-  ~~~  -+----------------------------------------  ~~~  -+-  ~~~  -+
  // |  32 GB  |                 (Ideally) 1 TB                 |  32 GB  |
  // |         |                                                |         |
  // | Guard   |      4 GB      :  ArrayBuffer backing stores,  | Guard   |
  // | Region  |    V8 Heap     :  WASM memory buffers, and     | Region  |
  // | (front) |     Region     :  any other caged objects.     | (back)  |
  // +-  ~~~  -+----------------+-----------------------  ~~~  -+-  ~~~  -+
  //           ^                                                ^
  //           base                                             base + size

  V8VirtualMemoryCage() = default;

  V8VirtualMemoryCage(const V8VirtualMemoryCage&) = delete;
  V8VirtualMemoryCage& operator=(V8VirtualMemoryCage&) = delete;

  bool Initialize(v8::PageAllocator* page_allocator);
  void Disable() {
    CHECK(!initialized_);
    disabled_ = true;
  }

  void TearDown();

  bool is_initialized() const { return initialized_; }
  bool is_disabled() const { return disabled_; }
  bool is_enabled() const { return !disabled_; }

  Address base() const { return base_; }
  size_t size() const { return size_; }

  base::BoundedPageAllocator* page_allocator() const {
    return cage_page_allocator_.get();
  }

  bool Contains(Address addr) const {
    return addr >= base_ && addr < base_ + size_;
  }

  bool Contains(void* ptr) const {
    return Contains(reinterpret_cast<Address>(ptr));
  }

 private:
  // The SequentialUnmapperTest calls the private Initialize method to create a
  // cage without guard regions, which would otherwise consume too much memory.
  friend class SequentialUnmapperTest;

  // We allow tests to disable the guard regions around the cage. This is useful
  // for example for tests like the SequentialUnmapperTest which track page
  // allocations and so would incur a large overhead from the guard regions.
  bool Initialize(v8::PageAllocator* page_allocator, size_t total_size,
                  bool use_guard_regions);

  Address base_ = kNullAddress;
  size_t size_ = 0;
  bool has_guard_regions_ = false;
  bool initialized_ = false;
  bool disabled_ = false;
  // The PageAllocator through which the virtual memory of the cage was
  // allocated.
  v8::PageAllocator* page_allocator_ = nullptr;
  // The BoundedPageAllocator to allocate pages inside the cage.
  std::unique_ptr<base::BoundedPageAllocator> cage_page_allocator_;
};

V8_EXPORT_PRIVATE V8VirtualMemoryCage* GetProcessWideVirtualMemoryCage();

#endif  // V8_VIRTUAL_MEMORY_CAGE

V8_INLINE bool IsValidBackingStorePointer(void* ptr) {
#ifdef V8_VIRTUAL_MEMORY_CAGE
  Address addr = reinterpret_cast<Address>(ptr);
  return kAllowBackingStoresOutsideCage || addr == kNullAddress ||
         GetProcessWideVirtualMemoryCage()->Contains(addr);
#else
  return true;
#endif
}

}  // namespace internal
}  // namespace v8

#endif  // V8_INIT_VM_CAGE_H_
