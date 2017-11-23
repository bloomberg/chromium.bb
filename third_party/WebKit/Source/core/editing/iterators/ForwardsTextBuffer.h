// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ForwardsTextBuffer_h
#define ForwardsTextBuffer_h

#include "base/macros.h"
#include "core/editing/iterators/TextBufferBase.h"

namespace blink {

class CORE_EXPORT ForwardsTextBuffer final : public TextBufferBase {
  STACK_ALLOCATED();

 public:
  ForwardsTextBuffer() = default;
  const UChar* Data() const override;

 private:
  UChar* CalcDestination(size_t length) override;

  DISALLOW_COPY_AND_ASSIGN(ForwardsTextBuffer);
};

}  // namespace blink

#endif  // TextBuffer_h
