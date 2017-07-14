// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FlexibleArrayBufferView_h
#define FlexibleArrayBufferView_h

#include "core/CoreExport.h"
#include "core/typed_arrays/DOMArrayBufferView.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Noncopyable.h"

namespace blink {

class CORE_EXPORT FlexibleArrayBufferView {
  STACK_ALLOCATED();
  WTF_MAKE_NONCOPYABLE(FlexibleArrayBufferView);

 public:
  FlexibleArrayBufferView() : small_data_(nullptr), small_length_(0) {}

  void SetFull(DOMArrayBufferView* full) { full_ = full; }
  void SetSmall(void* data, size_t length) {
    small_data_ = data;
    small_length_ = length;
  }

  void Clear() {
    full_ = nullptr;
    small_data_ = nullptr;
    small_length_ = 0;
  }

  bool IsEmpty() const { return !full_ && !small_data_; }
  bool IsFull() const { return full_; }

  DOMArrayBufferView* Full() const {
    DCHECK(IsFull());
    return full_;
  }

  // WARNING: The pointer returned by baseAddressMaybeOnStack() may point to
  // temporary storage that is only valid during the life-time of the
  // FlexibleArrayBufferView object.
  void* BaseAddressMaybeOnStack() const {
    DCHECK(!IsEmpty());
    return IsFull() ? full_->BaseAddress() : small_data_;
  }

  unsigned ByteOffset() const {
    DCHECK(!IsEmpty());
    return IsFull() ? full_->byteOffset() : 0;
  }

  unsigned ByteLength() const {
    DCHECK(!IsEmpty());
    return IsFull() ? full_->byteLength() : small_length_;
  }

  operator bool() const { return !IsEmpty(); }

 private:
  Member<DOMArrayBufferView> full_;

  void* small_data_;
  size_t small_length_;
};

}  // namespace blink

#endif  // FlexibleArrayBufferView_h
