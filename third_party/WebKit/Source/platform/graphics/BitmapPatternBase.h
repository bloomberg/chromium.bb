// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BitmapPatternBase_h
#define BitmapPatternBase_h

#include "platform/graphics/Pattern.h"

namespace blink {

class PLATFORM_EXPORT BitmapPatternBase : public Pattern {
public:
    BitmapPatternBase(RepeatMode, int64_t externalMemoryAllocated = 0);
    virtual ~BitmapPatternBase();

    virtual PassRefPtr<SkShader> createShader() override;

protected:
    virtual SkImageInfo getBitmapInfo() = 0;
    virtual void drawBitmapToCanvas(SkCanvas&, SkPaint&) = 0;
};

} // namespace

#endif  /* BitmapPatternBase_h */
