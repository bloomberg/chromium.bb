// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_TYPED_BUFFER_H_
#define REMOTING_BASE_TYPED_BUFFER_H_

#include <assert.h>

#include <algorithm>

#include "base/basictypes.h"
#include "base/move.h"

namespace remoting {

// A scoper for a variable-length structure such as SID, SECURITY_DESCRIPTOR and
// similar. These structures typically consist of a header followed by variable-
// length data, so the size may not match sizeof(T). The class supports
// move-only semantics and typed buffer getters.
template <typename T>
class TypedBuffer {
  MOVE_ONLY_TYPE_FOR_CPP_03(TypedBuffer, RValue)

 public:
  TypedBuffer() : buffer_(NULL), length_(0) {
  }

  // Creates an instance of the object allocating a buffer of the given size.
  explicit TypedBuffer(uint32 length) : buffer_(NULL), length_(length) {
    if (length_ > 0)
      buffer_ = reinterpret_cast<T*>(new uint8[length_]);
  }

  // Move constructor for C++03 move emulation of this type.
  TypedBuffer(RValue rvalue) : buffer_(NULL), length_(0) {
    TypedBuffer temp;
    temp.Swap(*rvalue.object);
    Swap(temp);
  }

  ~TypedBuffer() {
    if (buffer_) {
      delete[] reinterpret_cast<uint8*>(buffer_);
      buffer_ = NULL;
    }
  }

  // Move operator= for C++03 move emulation of this type.
  TypedBuffer& operator=(RValue rvalue) {
    TypedBuffer temp;
    temp.Swap(*rvalue.object);
    Swap(temp);
    return *this;
  }

  // Accessors to get the owned buffer.
  // operator* and operator-> will assert() if there is no current buffer.
  T& operator*() const {
    assert(buffer_ != NULL);
    return *buffer_;
  }
  T* operator->() const  {
    assert(buffer_ != NULL);
    return buffer_;
  }
  T* get() const { return buffer_; }

  uint32 length() const { return length_; }

  // Helper returning a pointer to the structure starting at a specified byte
  // offset.
  T* GetAtOffset(uint32 offset) {
    return reinterpret_cast<T*>(reinterpret_cast<uint8*>(buffer_) + offset);
  }

  // Allow TypedBuffer<T> to be used in boolean expressions, but not
  // implicitly convertible to a real bool (which is dangerous).
  typedef T* TypedBuffer::*Testable;
  operator Testable() const { return buffer_ ? &TypedBuffer::buffer_ : NULL; }

  // Swap two buffers.
  void Swap(TypedBuffer& other) {
    std::swap(buffer_, other.buffer_);
    std::swap(length_, other.length_);
  }

 private:
  // Points to the owned buffer.
  T* buffer_;

  // Length of the owned buffer in bytes.
  uint32 length_;
};

}  // namespace remoting

#endif  // REMOTING_BASE_TYPED_BUFFER_H_
