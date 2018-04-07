
/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/graphics/image_orientation.h"

#include "third_party/blink/renderer/platform/transforms/affine_transform.h"

namespace blink {

AffineTransform ImageOrientation::TransformFromDefault(
    const FloatSize& drawn_size) const {
  float w = drawn_size.Width();
  float h = drawn_size.Height();

  switch (orientation_) {
    case kOriginTopLeft:
      return AffineTransform();
    case kOriginTopRight:
      return AffineTransform(-1, 0, 0, 1, w, 0);
    case kOriginBottomRight:
      return AffineTransform(-1, 0, 0, -1, w, h);
    case kOriginBottomLeft:
      return AffineTransform(1, 0, 0, -1, 0, h);
    case kOriginLeftTop:
      return AffineTransform(0, 1, 1, 0, 0, 0);
    case kOriginRightTop:
      return AffineTransform(0, 1, -1, 0, w, 0);
    case kOriginRightBottom:
      return AffineTransform(0, -1, -1, 0, w, h);
    case kOriginLeftBottom:
      return AffineTransform(0, -1, 1, 0, 0, h);
    default:
      NOTREACHED();
  }

  NOTREACHED();
  return AffineTransform();
}

}  // namespace blink
