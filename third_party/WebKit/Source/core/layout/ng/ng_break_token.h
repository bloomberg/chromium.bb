// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGBreakToken_h
#define NGBreakToken_h

#include "core/CoreExport.h"
#include "platform/heap/Handle.h"

namespace blink {

class CORE_EXPORT NGBreakToken
    : public GarbageCollectedFinalized<NGBreakToken> {
 public:
  virtual ~NGBreakToken() {}

  enum NGBreakTokenType { kBlockBreakToken, kTextBreakToken };
  NGBreakTokenType Type() const { return static_cast<NGBreakTokenType>(type_); }

  DEFINE_INLINE_VIRTUAL_TRACE() {}

 protected:
  NGBreakToken(NGBreakTokenType type) : type_(type) {}

 private:
  unsigned type_ : 1;
};

}  // namespace blink

#endif  // NGBreakToken_h
