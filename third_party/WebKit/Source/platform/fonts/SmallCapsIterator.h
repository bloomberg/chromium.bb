// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SmallCapsIterator_h
#define SmallCapsIterator_h

#include <memory>
#include "platform/fonts/FontOrientation.h"
#include "platform/fonts/ScriptRunIterator.h"
#include "platform/fonts/UTF16TextIterator.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Noncopyable.h"

namespace blink {

class PLATFORM_EXPORT SmallCapsIterator {
  USING_FAST_MALLOC(SmallCapsIterator);
  WTF_MAKE_NONCOPYABLE(SmallCapsIterator);

 public:
  enum SmallCapsBehavior {
    kSmallCapsSameCase,
    kSmallCapsUppercaseNeeded,
    kSmallCapsInvalid
  };

  SmallCapsIterator(const UChar* buffer, unsigned buffer_size);

  bool Consume(unsigned* caps_limit, SmallCapsBehavior*);

 private:
  std::unique_ptr<UTF16TextIterator> utf16_iterator_;
  unsigned buffer_size_;
  UChar32 next_u_char32_;
  bool at_end_;

  SmallCapsBehavior current_small_caps_behavior_;
  SmallCapsBehavior previous_small_caps_behavior_;
};

}  // namespace blink

#endif
