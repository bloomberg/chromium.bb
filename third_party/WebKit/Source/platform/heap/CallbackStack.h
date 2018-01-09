// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CallbackStack_h
#define CallbackStack_h

#include "platform/heap/BlinkGC.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Assertions.h"
#include "platform/wtf/Threading.h"
#include "platform/wtf/ThreadingPrimitives.h"

namespace blink {

// The CallbackStack contains all the visitor callbacks used to trace and mark
// objects. A specific CallbackStack instance contains at most bufferSize
// elements.
// If more space is needed a new CallbackStack instance is created and chained
// together with the former instance. I.e. a logical CallbackStack can be made
// of multiple chained CallbackStack object instances.
class PLATFORM_EXPORT CallbackStack final {
  USING_FAST_MALLOC(CallbackStack);

 public:
  class Item {
    DISALLOW_NEW();

   public:
    Item() = default;
    Item(void* object, VisitorCallback callback)
        : object_(object), callback_(callback) {}
    void* Object() { return object_; }
    VisitorCallback Callback() { return callback_; }
    void Call(Visitor* visitor) { callback_(visitor, object_); }

   private:
    void* object_;
    VisitorCallback callback_;
  };

  static std::unique_ptr<CallbackStack> Create();
  ~CallbackStack();

  void Commit();
  void Decommit();

  Item* AllocateEntry();
  Item* Pop();

  bool IsEmpty() const;

  void InvokeEphemeronCallbacks(Visitor*);

#if DCHECK_IS_ON()
  bool HasCallbackForObject(const void*);
#endif
  bool HasJustOneBlock() const;

  static const size_t kMinimalBlockSize;
  static const size_t kDefaultBlockSize = (1 << 13);

 private:
  class Block {
    USING_FAST_MALLOC(Block);

   public:
    explicit Block(Block* next);
    ~Block();

#if DCHECK_IS_ON()
    void Clear();
#endif
    Block* Next() const { return next_; }
    void SetNext(Block* next) { next_ = next; }

    bool IsEmptyBlock() const { return current_ == &(buffer_[0]); }

    size_t BlockSize() const { return block_size_; }

    Item* AllocateEntry() {
      if (LIKELY(current_ < limit_))
        return current_++;
      return nullptr;
    }

    Item* Pop() {
      if (UNLIKELY(IsEmptyBlock()))
        return nullptr;
      return --current_;
    }

    void InvokeEphemeronCallbacks(Visitor*);

#if DCHECK_IS_ON()
    bool HasCallbackForObject(const void*);
#endif

   private:
    size_t block_size_;

    Item* buffer_;
    Item* limit_;
    Item* current_;
    Block* next_;
  };

  CallbackStack();
  Item* PopSlow();
  Item* AllocateEntrySlow();
  void InvokeOldestCallbacks(Block*, Block*, Visitor*);

  Block* first_;
  Block* last_;
};

class CallbackStackMemoryPool final {
  USING_FAST_MALLOC(CallbackStackMemoryPool);

 public:
  // 2048 * 8 * sizeof(Item) = 256 KB (64bit) is pre-allocated for the
  // underlying buffer of CallbackStacks.
  static const size_t kBlockSize = 2048;
  static const size_t kPooledBlockCount = 8;
  static const size_t kBlockBytes = kBlockSize * sizeof(CallbackStack::Item);

  static CallbackStackMemoryPool& Instance();

  void Initialize();
  CallbackStack::Item* Allocate();
  void Free(CallbackStack::Item*);

 private:
  Mutex mutex_;
  int free_list_first_;
  int free_list_next_[kPooledBlockCount];
  CallbackStack::Item* pooled_memory_;
};

ALWAYS_INLINE CallbackStack::Item* CallbackStack::AllocateEntry() {
  DCHECK(first_);
  Item* item = first_->AllocateEntry();
  if (LIKELY(!!item))
    return item;

  return AllocateEntrySlow();
}

ALWAYS_INLINE CallbackStack::Item* CallbackStack::Pop() {
  Item* item = first_->Pop();
  if (LIKELY(!!item))
    return item;

  return PopSlow();
}

}  // namespace blink

#endif  // CallbackStack_h
