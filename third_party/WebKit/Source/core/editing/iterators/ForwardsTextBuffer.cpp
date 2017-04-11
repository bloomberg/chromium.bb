// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/iterators/ForwardsTextBuffer.h"

namespace blink {

const UChar* ForwardsTextBuffer::Data() const {
  return BufferBegin();
}

UChar* ForwardsTextBuffer::CalcDestination(size_t length) {
  DCHECK_LE(Size() + length, Capacity());
  return BufferBegin() + Size();
}

}  // namespace blink
