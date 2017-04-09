// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SymbolsIterator_h
#define SymbolsIterator_h

#include <memory>
#include "platform/fonts/FontFallbackPriority.h"
#include "platform/fonts/FontOrientation.h"
#include "platform/fonts/ScriptRunIterator.h"
#include "platform/fonts/UTF16TextIterator.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Noncopyable.h"

namespace blink {

class PLATFORM_EXPORT SymbolsIterator {
  USING_FAST_MALLOC(SymbolsIterator);
  WTF_MAKE_NONCOPYABLE(SymbolsIterator);

 public:
  SymbolsIterator(const UChar* buffer, unsigned buffer_size);

  bool Consume(unsigned* symbols_limit, FontFallbackPriority*);

 private:
  FontFallbackPriority FontFallbackPriorityForCharacter(UChar32);

  std::unique_ptr<UTF16TextIterator> utf16_iterator_;
  unsigned buffer_size_;
  UChar32 next_char_;
  bool at_end_;

  FontFallbackPriority current_font_fallback_priority_;
  FontFallbackPriority previous_font_fallback_priority_;
};

}  // namespace blink

#endif
