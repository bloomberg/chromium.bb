/*
 * Copyright (C) 2015 Google Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef CachingWordShaper_h
#define CachingWordShaper_h

#include "base/memory/scoped_refptr.h"
#include "platform/fonts/shaping/ShapeResultBuffer.h"
#include "platform/geometry/FloatRect.h"
#include "platform/text/TextRun.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Vector.h"

namespace blink {

struct CharacterRange;
class Font;
class ShapeCache;
class SimpleFontData;
struct GlyphData;

class PLATFORM_EXPORT CachingWordShaper final {
  STACK_ALLOCATED();
  WTF_MAKE_NONCOPYABLE(CachingWordShaper);

 public:
  explicit CachingWordShaper(const Font& font) : font_(font) {}
  ~CachingWordShaper() = default;

  float Width(const TextRun&,
              HashSet<const SimpleFontData*>* fallback_fonts,
              FloatRect* glyph_bounds);
  int OffsetForPosition(const TextRun&,
                        float target_x,
                        bool include_partial_glyphs);

  void FillResultBuffer(const TextRunPaintInfo&, ShapeResultBuffer*);
  CharacterRange GetCharacterRange(const TextRun&, unsigned from, unsigned to);
  Vector<CharacterRange> IndividualCharacterRanges(const TextRun&);

  Vector<ShapeResultBuffer::RunFontData> GetRunFontData(const TextRun&) const;

  GlyphData EmphasisMarkGlyphData(const TextRun&) const;

 private:
  ShapeCache* GetShapeCache() const;

  const Font& font_;
};

}  // namespace blink

#endif  // CachingWordShaper_h
