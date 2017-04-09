// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/iterators/BackwardsTextBuffer.h"

namespace blink {

const UChar* BackwardsTextBuffer::Data() const {
  return BufferEnd() - size();
}

UChar* BackwardsTextBuffer::CalcDestination(size_t length) {
  DCHECK_LE(size() + length, Capacity());
  return BufferEnd() - size() - length;
}

void BackwardsTextBuffer::ShiftData(size_t old_capacity) {
  DCHECK_LE(old_capacity, Capacity());
  DCHECK_LE(size(), old_capacity);
  std::copy_backward(BufferBegin() + old_capacity - size(),
                     BufferBegin() + old_capacity, BufferEnd());
}

}  // namespace blink
