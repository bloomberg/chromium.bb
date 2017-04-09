// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BackwardsTextBuffer_h
#define BackwardsTextBuffer_h

#include "core/editing/iterators/TextBufferBase.h"

namespace blink {

class CORE_EXPORT BackwardsTextBuffer final : public TextBufferBase {
  STACK_ALLOCATED();
  WTF_MAKE_NONCOPYABLE(BackwardsTextBuffer);

 public:
  BackwardsTextBuffer() {}
  const UChar* Data() const override;

 private:
  UChar* CalcDestination(size_t length) override;
  void ShiftData(size_t old_capacity) override;
};

}  // namespace blink

#endif  // TextBuffer_h
