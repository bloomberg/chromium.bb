// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/wtf/typed_arrays/ArrayPiece.h"

#include "platform/wtf/Assertions.h"
#include "platform/wtf/typed_arrays/ArrayBuffer.h"
#include "platform/wtf/typed_arrays/ArrayBufferView.h"

namespace WTF {

ArrayPiece::ArrayPiece() {
  InitNull();
}

ArrayPiece::ArrayPiece(void* data, unsigned byte_length) {
  InitWithData(data, byte_length);
}

ArrayPiece::ArrayPiece(ArrayBuffer* buffer) {
  if (buffer) {
    InitWithData(buffer->Data(), buffer->ByteLength());
  } else {
    InitNull();
  }
}

ArrayPiece::ArrayPiece(ArrayBufferView* buffer) {
  if (buffer) {
    InitWithData(buffer->BaseAddress(), buffer->ByteLength());
  } else {
    InitNull();
  }
}

bool ArrayPiece::IsNull() const {
  return is_null_;
}

void* ArrayPiece::Data() const {
  DCHECK(!IsNull());
  return data_;
}

unsigned char* ArrayPiece::Bytes() const {
  return static_cast<unsigned char*>(Data());
}

unsigned ArrayPiece::ByteLength() const {
  DCHECK(!IsNull());
  return byte_length_;
}

void ArrayPiece::InitWithData(void* data, unsigned byte_length) {
  byte_length_ = byte_length;
  data_ = data;
  is_null_ = false;
}

void ArrayPiece::InitNull() {
  byte_length_ = 0;
  data_ = nullptr;
  is_null_ = true;
}

}  // namespace WTF
