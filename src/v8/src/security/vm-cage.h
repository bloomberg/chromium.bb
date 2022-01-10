// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SECURITY_VM_CAGE_H_
#define V8_SECURITY_VM_CAGE_H_

#include "include/v8-internal.h"
#include "src/base/bounded-page-allocator.h"
#include "src/common/globals.h"

namespace v8 {

namespace internal {

#ifdef V8_VIRTUAL_MEMORY_CAGE_IS_AVAILABLE

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

  bool Initialize(v8::VirtualAddressSpace* vas);
  void Disable() {
    CHECK(!initialized_);
    disabled_ = true;
  }

  void TearDown();

  bool is_initialized() const { return initialized_; }
  bool is_disabled() const { return disabled_; }
  bool is_enabled() const { return !disabled_; }
  bool is_fake_cage() const { return is_fake_cage_; }

  Address base() const { return base_; }
  Address end() const { return end_; }
  size_t size() const { return size_; }

  Address base_address() const { return reinterpret_cast<Address>(&base_); }
  Address end_address() const { return reinterpret_cast<Address>(&end_); }
  Address size_address() const { return reinterpret_cast<Address>(&size_); }

  v8::PageAllocator* page_allocator() const {
    return cage_page_allocator_.get();
  }

  v8::VirtualAddressSpace* virtual_address_space() const {
    return virtual_address_space_.get();
  }

  bool Contains(Address addr) const {
    return addr >= base_ && addr < base_ + size_;
  }

  bool Contains(void* ptr) const {
    return Contains(reinterpret_cast<Address>(ptr));
  }

#ifdef V8_CAGED_POINTERS
  class CagedPointerConstants final {
   public:
    Address empty_backing_store_buffer() const {
      return empty_backing_store_buffer_;
    }
    Address empty_backing_store_buffer_address() const {
      return reinterpret_cast<Address>(&empty_backing_store_buffer_);
    }
    void set_empty_backing_store_buffer(Address value) {
      empty_backing_store_buffer_ = value;
    }

    void Reset() { empty_backing_store_buffer_ = 0; }

   private:
    Address empty_backing_store_buffer_ = 0;
  };
  const CagedPointerConstants& constants() const { return constants_; }
#endif

 private:
  // The SequentialUnmapperTest calls the private Initialize method to create a
  // cage without guard regions, which would otherwise consume too much memory.
  friend class SequentialUnmapperTest;

  // These tests call the private Initialize methods below.
  FRIEND_TEST(VirtualMemoryCageTest, InitializationWithSize);
  FRIEND_TEST(VirtualMemoryCageTest, InitializationAsFakeCage);
  FRIEND_TEST(VirtualMemoryCageTest, FakeCagePageAllocation);

  // We allow tests to disable the guard regions around the cage. This is useful
  // for example for tests like the SequentialUnmapperTest which track page
  // allocations and so would incur a large overhead from the guard regions.
  // The provided virtual address space must be able to allocate subspaces.
  // The size must be a multiple of the allocation granularity of the virtual
  // memory space.
  bool Initialize(v8::VirtualAddressSpace* vas, size_t size,
                  bool use_guard_regions);

  // Used on OSes where reserving virtual memory is too expensive. A fake cage
  // does not reserve all of the virtual memory and so doesn't have the desired
  // security properties.
  // The size and size_to_reserve parameters must be multiples of the
  // allocation granularity of the virtual address space.
  bool InitializeAsFakeCage(v8::VirtualAddressSpace* vas, size_t size,
                            size_t size_to_reserve);

  // Initialize the caged pointer constants for this cage. Called by the
  // Initialize methods above.
  void InitializeConstants();

  Address base_ = kNullAddress;
  Address end_ = kNullAddress;
  size_t size_ = 0;

  // Base and size of the virtual memory reservation backing this cage. These
  // can be different from the cage base and size due to guard regions or when a
  // fake cage is used.
  Address reservation_base_ = kNullAddress;
  size_t reservation_size_ = 0;

  bool initialized_ = false;
  bool disabled_ = false;
  bool is_fake_cage_ = false;

  // The virtual address subspace backing the cage.
  std::unique_ptr<v8::VirtualAddressSpace> virtual_address_space_;

  // The page allocator instance for this cage.
  std::unique_ptr<v8::PageAllocator> cage_page_allocator_;

#ifdef V8_CAGED_POINTERS
  // CagedPointer constants inside this cage.
  CagedPointerConstants constants_;
#endif
};

#endif  // V8_VIRTUAL_MEMORY_CAGE_IS_AVAILABLE

#ifdef V8_VIRTUAL_MEMORY_CAGE
// This function is only available when the cage is actually used.
V8_EXPORT_PRIVATE V8VirtualMemoryCage* GetProcessWideVirtualMemoryCage();
#endif

V8_INLINE bool IsValidBackingStorePointer(void* ptr) {
#ifdef V8_VIRTUAL_MEMORY_CAGE
  Address addr = reinterpret_cast<Address>(ptr);
  return kAllowBackingStoresOutsideCage || addr == kNullAddress ||
         GetProcessWideVirtualMemoryCage()->Contains(addr);
#else
  return true;
#endif
}

V8_INLINE void* EmptyBackingStoreBuffer() {
#ifdef V8_CAGED_POINTERS
  return reinterpret_cast<void*>(GetProcessWideVirtualMemoryCage()
                                     ->constants()
                                     .empty_backing_store_buffer());
#else
  return nullptr;
#endif
}

}  // namespace internal
}  // namespace v8

#endif  // V8_SECURITY_VM_CAGE_H_
