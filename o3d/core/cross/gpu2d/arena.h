/*
 * Copyright 2010, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// An arena allocates large chunks of memory internally and allocates
// individual objects out of those chunks. Objects are not freed
// individually; their storage is reclaimed all at once when the arena
// is destroyed. The arena calls the constructors of the objects
// allocated within, but NOT their destructors.

#ifndef O3D_CORE_CROSS_GPU2D_ARENA_H_
#define O3D_CORE_CROSS_GPU2D_ARENA_H_

#include <stddef.h>

#include <algorithm>
#include <list>

#include "core/cross/types.h"

namespace o3d {
namespace gpu2d {

class Arena {
 public:
  // The arena is configured with an allocator, which is responsible
  // for allocating and freeing chunks of memory at a time.
  class Allocator {
   public:
    virtual ~Allocator() {}
    virtual void* Allocate(size_t size) = 0;
    virtual void Free(void* ptr) = 0;
   protected:
    Allocator() {}
    DISALLOW_COPY_AND_ASSIGN(Allocator);
  };

  // The Arena's default allocator, which uses malloc and free to
  // allocate chunks of storage.
  class MallocAllocator : public Allocator {
   public:
    virtual void* Allocate(size_t size) {
      return ::malloc(size);
    }

    virtual void Free(void* ptr) {
      ::free(ptr);
    }
  };

  // Creates a new Arena configured with a MallocAllocator, which is
  // deleted when the arena is.
  Arena()
      : allocator_(new MallocAllocator),
        should_delete_allocator_(true),
        current_(NULL),
        current_chunk_size_(kDefaultChunkSize) {
  }

  // Creates a new Arena configured with the given Allocator. The
  // Allocator is not deleted when the arena is.
  explicit Arena(Allocator* allocator)
      : allocator_(allocator),
        should_delete_allocator_(false),
        current_(NULL),
        current_chunk_size_(kDefaultChunkSize) {
  }

  // Deletes this arena.
  ~Arena() {
    for_each(chunks_.begin(), chunks_.end(), &Arena::DeleteChunk);
    if (should_delete_allocator_) {
      delete allocator_;
    }
  }

  // Allocates an object from the arena.
  template<class T>
  T* Alloc() {
    void* ptr = NULL;
    size_t rounded_size = RoundUp(sizeof(T), MinAlignment());
    if (current_ != NULL)
      ptr = current_->Allocate(rounded_size);

    if (ptr == NULL) {
      if (rounded_size > current_chunk_size_)
        current_chunk_size_ = rounded_size;
      current_ = new Chunk(allocator_, current_chunk_size_);
      chunks_.push_back(current_);
      ptr = current_->Allocate(rounded_size);
    }

    if (ptr != NULL) {
      // Allocate a T at this spot
      new(ptr) T();
    }

    return static_cast<T*>(ptr);
  }

 private:
  enum {
    kDefaultChunkSize = 16384
  };

  // The following two structures are intended to automatically
  // determine the platform's alignment constraint for structures.
  struct AlignmentInner {
    int64 x;
  };
  struct AlignmentStruct {
    char f1;
    AlignmentInner f2;
  };

  // Returns the alignment requirement for classes and structs on the
  // current platform.
  static size_t MinAlignment() {
    return offsetof(AlignmentStruct, f2);
  }

  // Rounds up the given allocation size to the specified alignment.
  size_t RoundUp(size_t size, size_t alignment) {
    DCHECK(alignment % 2 == 0);
    return (size + alignment - 1) & ~(alignment - 1);
  }

  Allocator* allocator_;
  bool should_delete_allocator_;

  // Manages a chunk of memory and individual allocations out of it.
  class Chunk {
   public:
    // Allocates a block of memory of the given size from the passed
    // Allocator.
    Chunk(Allocator* allocator, size_t size)
        : allocator_(allocator),
          size_(size) {
      base_ = static_cast<uint8*>(allocator_->Allocate(size));
      current_offset_ = 0;
    }

    // Frees the memory allocated from the Allocator in the
    // constructor.
    ~Chunk() {
      allocator_->Free(base_);
    }

    // Returns a pointer to "size" bytes of storage, or NULL if this
    // Chunk could not satisfy the allocation.
    void* Allocate(size_t size) {
      // Check for overflow
      if (current_offset_ + size < current_offset_) {
        return NULL;
      }

      if (current_offset_ + size > size_) {
        return NULL;
      }
      void* result = base_ + current_offset_;
      current_offset_ += size;
      return result;
    }

   private:
    Allocator* allocator_;
    uint8* base_;
    size_t size_;
    size_t current_offset_;
    DISALLOW_COPY_AND_ASSIGN(Chunk);
  };

  // Callback used to delete an individual chunk.
  static void DeleteChunk(Chunk* chunk) {
    delete chunk;
  }

  Chunk* current_;
  std::list<Chunk*> chunks_;
  size_t current_chunk_size_;

  DISALLOW_COPY_AND_ASSIGN(Arena);
};

}  // namespace gpu2d
}  // namespace o3d

#endif  // O3D_CORE_CROSS_GPU2D_ARENA_H_

