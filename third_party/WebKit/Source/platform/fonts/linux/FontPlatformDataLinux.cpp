/*
 * Copyright (c) 2006, 2007, 2008, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/fonts/FontPlatformData.h"
#include "platform/graphics/skia/SkiaUtils.h"

#include "SkTypeface.h"

namespace blink {

void FontPlatformData::SetupPaint(SkPaint* paint,
                                  float device_scale_factor,
                                  const Font*) const {
  style_.ApplyToPaint(*paint, device_scale_factor);

  const float ts = text_size_ >= 0 ? text_size_ : 12;
  paint->setTextSize(SkFloatToScalar(ts));
  paint->setTypeface(typeface_);
  paint->setFakeBoldText(synthetic_bold_);
  paint->setTextSkewX(synthetic_italic_ ? -SK_Scalar1 / 4 : 0);

  // TODO(drott): Due to Skia bug 5917
  // https://bugs.chromium.org/p/skia/issues/detail?id=5917 correct advance
  // width scaling with FreeType for font sizes under 257px currently only works
  // with:
  // paint->setHinting(SkPaint::kNo_Hinting);
}

}  // namespace blink
