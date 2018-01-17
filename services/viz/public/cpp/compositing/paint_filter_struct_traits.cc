// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/viz/public/cpp/compositing/paint_filter_struct_traits.h"

namespace mojo {

PaintFilterBuffer::PaintFilterBuffer() = default;

PaintFilterBuffer::PaintFilterBuffer(const PaintFilterBuffer& other) = default;

PaintFilterBuffer::~PaintFilterBuffer() = default;

// static
size_t ArrayTraits<PaintFilterBuffer>::GetSize(const PaintFilterBuffer& b) {
  return b.data.size();
}

// static
uint8_t* ArrayTraits<PaintFilterBuffer>::GetData(PaintFilterBuffer& b) {
  return b.data.data();
}

// static
const uint8_t* ArrayTraits<PaintFilterBuffer>::GetData(
    const PaintFilterBuffer& b) {
  return b.data.data();
}

// static
uint8_t& ArrayTraits<PaintFilterBuffer>::GetAt(PaintFilterBuffer& b, size_t i) {
  return b.data[i];
}

// static
const uint8_t& ArrayTraits<PaintFilterBuffer>::GetAt(const PaintFilterBuffer& b,
                                                     size_t i) {
  return b.data[i];
}

// static
bool ArrayTraits<PaintFilterBuffer>::Resize(PaintFilterBuffer& b, size_t size) {
  b.data.resize(size);
  return true;
}

}  // namespace mojo
