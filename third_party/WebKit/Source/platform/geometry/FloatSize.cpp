/*
 * Copyright (C) 2003, 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2005 Nokia.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/geometry/FloatSize.h"

#include <math.h>
#include <limits>
#include "platform/geometry/IntSize.h"
#include "platform/geometry/LayoutSize.h"
#include "platform/wtf/MathExtras.h"
#include "platform/wtf/text/WTFString.h"
#include "third_party/skia/include/core/SkSize.h"

namespace blink {

FloatSize::FloatSize(const SkSize& size)
    : width_(size.width()), height_(size.height()) {}

FloatSize::FloatSize(const LayoutSize& size)
    : width_(size.Width().ToFloat()), height_(size.Height().ToFloat()) {}

float FloatSize::DiagonalLength() const {
  return hypotf(width_, height_);
}

bool FloatSize::IsZero() const {
  return fabs(width_) < std::numeric_limits<float>::epsilon() &&
         fabs(height_) < std::numeric_limits<float>::epsilon();
}

bool FloatSize::IsExpressibleAsIntSize() const {
  return isWithinIntRange(width_) && isWithinIntRange(height_);
}

FloatSize FloatSize::NarrowPrecision(double width, double height) {
  return FloatSize(clampTo<float>(width), clampTo<float>(height));
}

FloatSize::operator SkSize() const {
  return SkSize::Make(width_, height_);
}

String FloatSize::ToString() const {
  return String::Format("%lgx%lg", Width(), Height());
}

}  // namespace blink
