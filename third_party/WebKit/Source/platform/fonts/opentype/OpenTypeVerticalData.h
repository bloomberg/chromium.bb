/*
 * Copyright (C) 2012 Koji Ishii <kojiishi@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef OpenTypeVerticalData_h
#define OpenTypeVerticalData_h

#include "base/memory/scoped_refptr.h"
#include "platform/PlatformExport.h"
#include "platform/fonts/Glyph.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/HashMap.h"
#include "platform/wtf/RefCounted.h"
#include "platform/wtf/Vector.h"

#include <SkPaint.h>
#include <SkRefCnt.h>
#include <SkTypeface.h>

namespace blink {

class PLATFORM_EXPORT OpenTypeVerticalData
    : public RefCounted<OpenTypeVerticalData> {
 public:
  static scoped_refptr<OpenTypeVerticalData> CreateUnscaled(
      sk_sp<SkTypeface> typeface) {
    return base::AdoptRef(new OpenTypeVerticalData(typeface));
  }

  void SetScaleAndFallbackMetrics(float size_per_unit,
                                  float ascent,
                                  int height);

  bool IsOpenType() const { return !advance_widths_.IsEmpty(); }
  bool HasVerticalMetrics() const { return !advance_heights_.IsEmpty(); }
  float AdvanceHeight(Glyph) const;

  void GetVerticalTranslationsForGlyphs(const SkPaint&,
                                        const Glyph*,
                                        size_t,
                                        float* out_xy_array) const;

 private:
  explicit OpenTypeVerticalData(sk_sp<SkTypeface>);

  void LoadMetrics(sk_sp<SkTypeface>);
  bool HasVORG() const { return !vert_origin_y_.IsEmpty(); }

  HashMap<Glyph, Glyph> vertical_glyph_map_;
  Vector<uint16_t> advance_widths_;
  Vector<uint16_t> advance_heights_;
  Vector<int16_t> top_side_bearings_;
  int16_t default_vert_origin_y_;
  HashMap<Glyph, int16_t> vert_origin_y_;

  float size_per_unit_;
  float ascent_fallback_;
  int height_fallback_;
};

}  // namespace blink

#endif  // OpenTypeVerticalData_h
