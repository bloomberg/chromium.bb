// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OrientationIterator_h
#define OrientationIterator_h

#include <memory>
#include "platform/fonts/FontOrientation.h"
#include "platform/fonts/ScriptRunIterator.h"
#include "platform/fonts/UTF16TextIterator.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Noncopyable.h"

namespace blink {

class PLATFORM_EXPORT OrientationIterator {
  USING_FAST_MALLOC(OrientationIterator);
  WTF_MAKE_NONCOPYABLE(OrientationIterator);

 public:
  enum RenderOrientation {
    kOrientationKeep,
    kOrientationRotateSideways,
    kOrientationInvalid
  };

  OrientationIterator(const UChar* buffer,
                      unsigned buffer_size,
                      FontOrientation run_orientation);

  bool Consume(unsigned* orientation_limit, RenderOrientation*);

 private:
  std::unique_ptr<UTF16TextIterator> utf16_iterator_;
  unsigned buffer_size_;
  bool at_end_;
};

}  // namespace blink

#endif
