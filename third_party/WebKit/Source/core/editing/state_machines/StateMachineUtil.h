// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef StateMachineUtil_h
#define StateMachineUtil_h

#include "core/CoreExport.h"
#include "platform/wtf/text/Unicode.h"

namespace blink {

// Returns true if there is a grapheme boundary between prevCodePoint and
// nextCodePoint.
// DO NOT USE this function directly since this doesn't care about preceding
// regional indicator symbols. Use ForwardGraphemeBoundaryStateMachine or
// BackwardGraphemeBoundaryStateMachine instead.
CORE_EXPORT bool IsGraphemeBreak(UChar32 prev_code_point,
                                 UChar32 next_code_point);

}  // namespace blink

#endif  // StateMachineUtil_h
