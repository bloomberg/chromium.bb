// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TYPED_ARRAYS_DOM_ARRAY_PIECE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TYPED_ARRAYS_DOM_ARRAY_PIECE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"

namespace blink {

class ArrayBufferOrArrayBufferView;
class DOMArrayBufferView;

// This class is for passing around un-owned bytes as a pointer + length.
// It supports implicit conversion from several other data types.
//
// DOMArrayPiece has the concept of being "null". This is different from an
// empty byte range. It is invalid to call methods other than isNull() on such
// instances.
//
// IMPORTANT: The data contained by DOMArrayPiece is NOT OWNED, so caution must
//            be taken to ensure it is kept alive.
class CORE_EXPORT DOMArrayPiece {
  DISALLOW_NEW();

 public:
  DOMArrayPiece();
  DOMArrayPiece(DOMArrayBuffer* buffer);
  DOMArrayPiece(DOMArrayBufferView* view);
  DOMArrayPiece(const ArrayBufferOrArrayBufferView&);

  bool operator==(const DOMArrayBuffer& other) const {
    return ByteLengthAsSizeT() == other.ByteLengthAsSizeT() &&
           memcmp(Data(), other.Data(), ByteLengthAsSizeT()) == 0;
  }

  bool IsNull() const;
  bool IsDetached() const;
  void* Data() const;
  unsigned char* Bytes() const;
  size_t ByteLengthAsSizeT() const;

 private:
  void InitWithArrayBuffer(DOMArrayBuffer*);
  void InitWithArrayBufferView(DOMArrayBufferView*);
  void InitWithData(void* data, size_t byte_length);

  void InitNull();

  void* data_;
  size_t byte_length_;
  bool is_null_;
  bool is_detached_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TYPED_ARRAYS_DOM_ARRAY_PIECE_H_
