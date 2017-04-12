// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaQueryBlockWatcher_h
#define MediaQueryBlockWatcher_h

#include "core/CoreExport.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class CSSParserToken;

class CORE_EXPORT MediaQueryBlockWatcher {
  STACK_ALLOCATED();

 public:
  MediaQueryBlockWatcher();
  void HandleToken(const CSSParserToken&);
  unsigned BlockLevel() const { return block_level_; }

 private:
  unsigned block_level_;
};

}  // namespace blink

#endif  // MediaQueryBlockWatcher_h
