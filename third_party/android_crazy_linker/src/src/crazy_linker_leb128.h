// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRAZY_LINKER_LEB128_H
#define CRAZY_LINKER_LEB128_H

#include <assert.h>
#include <stdint.h>

// Helper classes for decoding LEB128, used in packed relocation data.
// http://en.wikipedia.org/wiki/LEB128

namespace crazy {

class Sleb128Decoder {
 public:
  Sleb128Decoder(const uint8_t* buffer, size_t count)
      : current_(buffer), end_(buffer + count) { }

  size_t pop_front() {
    size_t value = 0;
    static const size_t size = CHAR_BIT * sizeof(value);

    size_t shift = 0;
    uint8_t byte;

    do {
      assert(current_ < end_);

      byte = *current_++;
      value |= (static_cast<size_t>(byte & 127) << shift);
      shift += 7;
    } while (byte & 128);

    if (shift < size && (byte & 64)) {
      value |= -(static_cast<size_t>(1) << shift);
    }

    return value;
  }

 private:
  const uint8_t* current_;
  const uint8_t* const end_;
};

}  // namespace crazy

#endif  // CRAZY_LINKER_LEB128_H
