// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRAZY_LINKER_LEB128_H
#define CRAZY_LINKER_LEB128_H

#include <stdint.h>

// Helper classes for decoding LEB128, used in packed relocation data.
// http://en.wikipedia.org/wiki/LEB128

namespace crazy {

class Leb128Decoder {
 public:
  explicit Leb128Decoder(const uint8_t* encoding)
      : encoding_(encoding), cursor_(0) { }

  size_t Dequeue() {
    uint32_t value = 0;

    size_t shift = 0;
    uint8_t byte;

    do {
      byte = encoding_[cursor_++];
      value |= static_cast<uint32_t>(byte & 127) << shift;
      shift += 7;
    } while (byte & 128);

    return value;
  }

 private:
  const uint8_t* encoding_;
  size_t cursor_;
};

class Sleb128Decoder {
 public:
  explicit Sleb128Decoder(const uint8_t* encoding)
      : encoding_(encoding), cursor_(0) { }

  ssize_t Dequeue() {
    ssize_t value = 0;
    static const size_t size = CHAR_BIT * sizeof(value);

    size_t shift = 0;
    uint8_t byte;

    do {
      byte = encoding_[cursor_++];
      value |= (static_cast<ssize_t>(byte & 127) << shift);
      shift += 7;
    } while (byte & 128);

    if (shift < size && (byte & 64))
      value |= -(static_cast<ssize_t>(1) << shift);

    return value;
  }

 private:
  const uint8_t* encoding_;
  size_t cursor_;
};

}  // namespace crazy

#endif  // CRAZY_LINKER_LEB128_H
