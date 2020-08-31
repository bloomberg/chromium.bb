// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TYPED_ARRAYS_FLEXIBLE_ARRAY_BUFFER_VIEW_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TYPED_ARRAYS_FLEXIBLE_ARRAY_BUFFER_VIEW_H_

#include "base/macros.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_array_buffer_view.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_view.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

// FlexibleArrayBufferView is a performance hack to avoid overhead to
// instantiate DOMArrayBufferView and DOMArrayBuffer when the contents are
// very small.  Otherwise, FlexibleArrayBufferView is a thin wrapper to
// DOMArrayBufferView.
class CORE_EXPORT FlexibleArrayBufferView {
  STACK_ALLOCATED();

 public:
  FlexibleArrayBufferView() = default;
  FlexibleArrayBufferView(const FlexibleArrayBufferView&) = default;
  FlexibleArrayBufferView(FlexibleArrayBufferView&&) = default;
  FlexibleArrayBufferView(v8::Local<v8::ArrayBufferView> array_buffer_view) {
    SetContents(array_buffer_view);
  }
  ~FlexibleArrayBufferView() = default;

  FlexibleArrayBufferView& operator=(const FlexibleArrayBufferView&) = delete;
  FlexibleArrayBufferView& operator=(FlexibleArrayBufferView&&) = delete;

  void Clear() {
    full_ = nullptr;
    small_data_ = nullptr;
    small_length_ = 0;
  }

  void SetContents(v8::Local<v8::ArrayBufferView> array_buffer_view) {
    DCHECK(IsNull());
    size_t size = array_buffer_view->ByteLength();
    if (size <= sizeof small_buffer_) {
      array_buffer_view->CopyContents(small_buffer_, size);
      small_data_ = small_buffer_;
      small_length_ = size;
    } else {
      full_ = V8ArrayBufferView::ToImpl(array_buffer_view);
    }
  }

  // Returns true if this object represents IDL null.
  bool IsNull() const { return !full_ && !small_data_; }

  bool IsFull() const { return full_; }

  DOMArrayBufferView* Full() const {
    DCHECK(IsFull());
    return full_;
  }

  // WARNING: The pointer returned by baseAddressMaybeOnStack() may point to
  // temporary storage that is only valid during the life-time of the
  // FlexibleArrayBufferView object.
  void* BaseAddressMaybeOnStack() const {
    DCHECK(!IsNull());
    return IsFull() ? full_->BaseAddressMaybeShared() : small_data_;
  }

  size_t ByteLengthAsSizeT() const {
    DCHECK(!IsNull());
    return IsFull() ? full_->byteLengthAsSizeT() : small_length_;
  }

  unsigned DeprecatedByteLengthAsUnsigned() const {
    DCHECK(!IsNull());
    return IsFull() ? base::checked_cast<unsigned>(full_->byteLengthAsSizeT())
                    : base::checked_cast<unsigned>(small_length_);
  }

  // TODO(crbug.com/1050474): Remove this cast operator and make the callsites
  // explicitly call IsNull().
  operator bool() const { return !IsNull(); }

 private:
  DOMArrayBufferView* full_ = nullptr;

  // If the contents of the given v8::ArrayBufferView are small enough to fit
  // within |small_buffer_|, the contents are directly copied into
  // |small_buffer_| to which accesses are much faster than to a maybe-shared
  // buffer in a DOMArrayBufferView.
  void* small_data_ = nullptr;  // Non null iff |small_buffer_| is used
  size_t small_length_ = 0;     // The size actually used of |small_buffer_|
  uint8_t small_buffer_[64];
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TYPED_ARRAYS_FLEXIBLE_ARRAY_BUFFER_VIEW_H_
