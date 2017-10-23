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

#include "platform/PlatformExport.h"
#include "platform/fonts/Glyph.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/HashMap.h"
#include "platform/wtf/RefCounted.h"
#include "platform/wtf/RefPtr.h"
#include "platform/wtf/Vector.h"

namespace blink {

class FontPlatformData;
class SimpleFontData;

class PLATFORM_EXPORT OpenTypeVerticalData
    : public RefCounted<OpenTypeVerticalData> {
 public:
  static scoped_refptr<OpenTypeVerticalData> Create(
      const FontPlatformData& platform_data) {
    return WTF::AdoptRef(new OpenTypeVerticalData(platform_data));
  }

  bool IsOpenType() const { return !advance_widths_.IsEmpty(); }
  bool HasVerticalMetrics() const { return !advance_heights_.IsEmpty(); }
  float AdvanceHeight(const SimpleFontData*, Glyph) const;

  bool InFontCache() const { return in_font_cache_; }
  void SetInFontCache(bool in_font_cache) { in_font_cache_ = in_font_cache; }

  void GetVerticalTranslationsForGlyphs(const SimpleFontData*,
                                        const Glyph*,
                                        size_t,
                                        float* out_xy_array) const;

 private:
  explicit OpenTypeVerticalData(const FontPlatformData&);

  void LoadMetrics(const FontPlatformData&);
  bool HasVORG() const { return !vert_origin_y_.IsEmpty(); }

  HashMap<Glyph, Glyph> vertical_glyph_map_;
  Vector<uint16_t> advance_widths_;
  Vector<uint16_t> advance_heights_;
  Vector<int16_t> top_side_bearings_;
  int16_t default_vert_origin_y_;
  HashMap<Glyph, int16_t> vert_origin_y_;

  bool
      in_font_cache_;  // for mark & sweep in FontCache::purgeInactiveFontData()
};

}  // namespace blink

#endif  // OpenTypeVerticalData_h
