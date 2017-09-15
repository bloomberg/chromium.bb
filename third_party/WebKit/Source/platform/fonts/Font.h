/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2006, 2007, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Holger Hans Peter Freyther
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef Font_h
#define Font_h

#include "platform/LayoutUnit.h"
#include "platform/PlatformExport.h"
#include "platform/fonts/FontDescription.h"
#include "platform/fonts/FontFallbackList.h"
#include "platform/fonts/FontFallbackPriority.h"
#include "platform/fonts/SimpleFontData.h"
#include "platform/graphics/paint/PaintCanvas.h"
#include "platform/graphics/paint/PaintFlags.h"
#include "platform/text/TabSize.h"
#include "platform/text/TextDirection.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/HashMap.h"
#include "platform/wtf/HashSet.h"
#include "platform/wtf/MathExtras.h"
#include "platform/wtf/text/CharacterNames.h"

// To avoid conflicts with the CreateWindow macro from the Windows SDK...
#undef DrawText

namespace blink {

struct CharacterRange;
class FloatPoint;
class FloatRect;
class FontFallbackIterator;
class FontData;
class FontSelector;
class ShapeCache;
class TextRun;
struct TextRunPaintInfo;
struct NGTextFragmentPaintInfo;

class PLATFORM_EXPORT Font {
  DISALLOW_NEW();

 public:
  Font();
  Font(const FontDescription&);
  ~Font();

  Font(const Font&);
  Font& operator=(const Font&);

  bool operator==(const Font& other) const;
  bool operator!=(const Font& other) const { return !(*this == other); }

  const FontDescription& GetFontDescription() const {
    return font_description_;
  }

  void Update(FontSelector*) const;

  enum CustomFontNotReadyAction {
    kDoNotPaintIfFontNotReady,
    kUseFallbackIfFontNotReady
  };
  bool DrawText(PaintCanvas*,
                const TextRunPaintInfo&,
                const FloatPoint&,
                float device_scale_factor,
                const PaintFlags&) const;
  bool DrawText(PaintCanvas*,
                const NGTextFragmentPaintInfo&,
                const FloatPoint&,
                float device_scale_factor,
                const PaintFlags&) const;
  bool DrawBidiText(PaintCanvas*,
                    const TextRunPaintInfo&,
                    const FloatPoint&,
                    CustomFontNotReadyAction,
                    float device_scale_factor,
                    const PaintFlags&) const;
  void DrawEmphasisMarks(PaintCanvas*,
                         const TextRunPaintInfo&,
                         const AtomicString& mark,
                         const FloatPoint&,
                         float device_scale_factor,
                         const PaintFlags&) const;
  void DrawEmphasisMarks(PaintCanvas*,
                         const NGTextFragmentPaintInfo&,
                         const AtomicString& mark,
                         const FloatPoint&,
                         float device_scale_factor,
                         const PaintFlags&) const;

  struct TextIntercept {
    float begin_, end_;
  };

  // Compute the text intercepts along the axis of the advance and write them
  // into the specified Vector of TextIntercepts. The number of those is zero or
  // a multiple of two, and is at most the number of glyphs * 2 in the TextRun
  // part of TextRunPaintInfo. Specify bounds for the upper and lower extend of
  // a line crossing through the text, parallel to the baseline.
  // TODO(drott): crbug.com/655154 Fix this for
  // upright in vertical.
  void GetTextIntercepts(const TextRunPaintInfo&,
                         float device_scale_factor,
                         const PaintFlags&,
                         const std::tuple<float, float>& bounds,
                         Vector<TextIntercept>&) const;
  void GetTextIntercepts(const NGTextFragmentPaintInfo&,
                         float device_scale_factor,
                         const PaintFlags&,
                         const std::tuple<float, float>& bounds,
                         Vector<TextIntercept>&) const;

  // Glyph bounds will be the minimum rect containing all glyph strokes, in
  // coordinates using (<text run x position>, <baseline position>) as the
  // origin.
  float Width(const TextRun&,
              HashSet<const SimpleFontData*>* fallback_fonts = nullptr,
              FloatRect* glyph_bounds = nullptr) const;

  int OffsetForPosition(const TextRun&,
                        float position,
                        bool include_partial_glyphs) const;
  FloatRect SelectionRectForText(const TextRun&,
                                 const FloatPoint&,
                                 int h,
                                 int from = 0,
                                 int to = -1) const;
  CharacterRange GetCharacterRange(const TextRun&,
                                   unsigned from,
                                   unsigned to) const;
  Vector<CharacterRange> IndividualCharacterRanges(const TextRun&) const;

  // Metrics that we query the FontFallbackList for.
  float SpaceWidth() const {
    DCHECK(PrimaryFont());
    return (PrimaryFont() ? PrimaryFont()->SpaceWidth() : 0) +
           GetFontDescription().LetterSpacing();
  }
  float TabWidth(const SimpleFontData*, const TabSize&, float position) const;
  float TabWidth(const TabSize& tab_size, float position) const {
    return TabWidth(PrimaryFont(), tab_size, position);
  }
  LayoutUnit TabWidth(const TabSize&, LayoutUnit position) const;

  int EmphasisMarkAscent(const AtomicString&) const;
  int EmphasisMarkDescent(const AtomicString&) const;
  int EmphasisMarkHeight(const AtomicString&) const;

  // This may fail and return a nullptr in case the last resort font cannot be
  // loaded. This *should* not happen but in reality it does ever now and then
  // when, for whatever reason, the last resort font cannot be loaded.
  const SimpleFontData* PrimaryFont() const;
  const FontData* FontDataAt(unsigned) const;

  // Access the shape cache associated with this particular font object.
  // Should *not* be retained across layout calls as it may become invalid.
  ShapeCache* GetShapeCache() const;

  // Whether the font supports shaping word by word instead of shaping the
  // full run in one go. Allows better caching for fonts where space cannot
  // participate in kerning and/or ligatures.
  bool CanShapeWordByWord() const;

  void SetCanShapeWordByWordForTesting(bool b) {
    can_shape_word_by_word_ = b;
    shape_word_by_word_computed_ = true;
  }

  void ReportNotDefGlyph() const;

 private:
  enum ForTextEmphasisOrNot { kNotForTextEmphasis, kForTextEmphasis };

  GlyphData GetEmphasisMarkGlyphData(const AtomicString&) const;

  bool ComputeCanShapeWordByWord() const;

 public:
  FontSelector* GetFontSelector() const;
  RefPtr<FontFallbackIterator> CreateFontFallbackIterator(
      FontFallbackPriority) const;

  void WillUseFontData(const String& text) const;

  bool LoadingCustomFonts() const;
  bool IsFallbackValid() const;

 private:
  bool ShouldSkipDrawing() const {
    return font_fallback_list_ && font_fallback_list_->ShouldSkipDrawing();
  }

  FontDescription font_description_;
  mutable RefPtr<FontFallbackList> font_fallback_list_;
  mutable unsigned can_shape_word_by_word_ : 1;
  mutable unsigned shape_word_by_word_computed_ : 1;

  // For m_fontDescription & m_fontFallbackList access.
  friend class CachingWordShaper;
};

inline Font::~Font() {}

inline const SimpleFontData* Font::PrimaryFont() const {
  DCHECK(font_fallback_list_);
  return font_fallback_list_->PrimarySimpleFontData(font_description_);
}

inline const FontData* Font::FontDataAt(unsigned index) const {
  DCHECK(font_fallback_list_);
  return font_fallback_list_->FontDataAt(font_description_, index);
}

inline FontSelector* Font::GetFontSelector() const {
  return font_fallback_list_ ? font_fallback_list_->GetFontSelector() : 0;
}

inline float Font::TabWidth(const SimpleFontData* font_data,
                            const TabSize& tab_size,
                            float position) const {
  if (!font_data)
    return GetFontDescription().LetterSpacing();
  float base_tab_width = tab_size.GetPixelSize(font_data->SpaceWidth());
  if (!base_tab_width)
    return GetFontDescription().LetterSpacing();
  float distance_to_tab_stop = base_tab_width - fmodf(position, base_tab_width);

  // Let the minimum width be the half of the space width so that it's always
  // recognizable.  if the distance to the next tab stop is less than that,
  // advance an additional tab stop.
  if (distance_to_tab_stop < font_data->SpaceWidth() / 2)
    distance_to_tab_stop += base_tab_width;

  return distance_to_tab_stop;
}

}  // namespace blink

#endif
