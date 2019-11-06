// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/der/input.h"

#include <string.h>

#include <algorithm>

#include "base/logging.h"

namespace net {

namespace der {

Input::Input() : data_(nullptr), len_(0) {
}

Input::Input(const uint8_t* data, size_t len) : data_(data), len_(len) {
}

Input::Input(const base::StringPiece& in)
    : data_(reinterpret_cast<const uint8_t*>(in.data())), len_(in.length()) {}

Input::Input(const std::string* s) : Input(base::StringPiece(*s)) {}

std::string Input::AsString() const {
  return std::string(reinterpret_cast<const char*>(data_), len_);
}

base::StringPiece Input::AsStringPiece() const {
  return base::StringPiece(reinterpret_cast<const char*>(data_), len_);
}

bool operator==(const Input& lhs, const Input& rhs) {
  if (lhs.Length() != rhs.Length())
    return false;
  return memcmp(lhs.UnsafeData(), rhs.UnsafeData(), lhs.Length()) == 0;
}

bool operator!=(const Input& lhs, const Input& rhs) {
  return !(lhs == rhs);
}

bool operator<(const Input& lhs, const Input& rhs) {
  return std::lexicographical_compare(
      lhs.UnsafeData(), lhs.UnsafeData() + lhs.Length(), rhs.UnsafeData(),
      rhs.UnsafeData() + rhs.Length());
}

ByteReader::ByteReader(const Input& in)
    : data_(in.UnsafeData()), len_(in.Length()) {
}

bool ByteReader::ReadByte(uint8_t* byte_p) {
  if (!HasMore())
    return false;
  *byte_p = *data_;
  Advance(1);
  return true;
}

bool ByteReader::ReadBytes(size_t len, Input* out) {
  if (len > len_)
    return false;
  *out = Input(data_, len);
  Advance(len);
  return true;
}

// Returns whether there is any more data to be read.
bool ByteReader::HasMore() {
  return len_ > 0;
}

void ByteReader::Advance(size_t len) {
  CHECK_LE(len, len_);
  data_ += len;
  len_ -= len;
}

}  // namespace der

}  // namespace net
