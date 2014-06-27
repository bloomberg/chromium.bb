// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(simonb): Extend for 64-bit target libraries.

#include "leb128.h"

#include <stdint.h>
#include <vector>

namespace relocation_packer {

// Empty constructor and destructor to silence chromium-style.
Leb128Encoder::Leb128Encoder() { }
Leb128Encoder::~Leb128Encoder() { }

// Add a single value to the encoding.  Values are encoded with variable
// length.  The least significant 7 bits of each byte hold 7 bits of data,
// and the most significant bit is set on each byte except the last.
void Leb128Encoder::Enqueue(uint32_t value) {
  while (value > 127) {
    encoding_.push_back((1 << 7) | (value & 127));
    value >>= 7;
  }
  encoding_.push_back(value);
}

// Add a vector of values to the encoding.
void Leb128Encoder::EnqueueAll(const std::vector<uint32_t>& values) {
  for (size_t i = 0; i < values.size(); ++i)
    Enqueue(values[i]);
}

// Create a new decoder for the given encoded stream.
Leb128Decoder::Leb128Decoder(const std::vector<uint8_t>& encoding) {
  encoding_ = encoding;
  cursor_ = 0;
}

// Empty destructor to silence chromium-style.
Leb128Decoder::~Leb128Decoder() { }

// Decode and retrieve a single value from the encoding.  Read forwards until
// a byte without its most significant bit is found, then read the 7 bit
// fields of the bytes spanned to re-form the value.
uint32_t Leb128Decoder::Dequeue() {
  size_t extent = cursor_;
  while (encoding_[extent] >> 7)
    extent++;

  uint32_t value = 0;
  for (size_t i = extent; i > cursor_; --i) {
    value = (value << 7) | (encoding_[i] & 127);
  }
  value = (value << 7) | (encoding_[cursor_] & 127);

  cursor_ = extent + 1;
  return value;
}

// Decode and retrieve all remaining values from the encoding.
void Leb128Decoder::DequeueAll(std::vector<uint32_t>* values) {
  while (cursor_ < encoding_.size())
    values->push_back(Dequeue());
}

}  // namespace relocation_packer
