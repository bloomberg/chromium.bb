/*
 * Copyright (C) 2005, 2008, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Alexey Proskuryakov
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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
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

#include "platform/fonts/SimpleFontData.h"

#include <unicode/unorm.h>
#include <unicode/utf16.h>
#include <memory>
#include "SkPath.h"
#include "SkTypeface.h"
#include "SkTypes.h"
#include "platform/fonts/FontDescription.h"
#include "platform/fonts/VDMXParser.h"
#include "platform/fonts/skia/SkiaTextMetrics.h"
#include "platform/geometry/FloatRect.h"
#include "platform/wtf/MathExtras.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/allocator/Partitions.h"
#include "platform/wtf/text/CharacterNames.h"
#include "platform/wtf/text/Unicode.h"

namespace blink {

const float kSmallCapsFontSizeMultiplier = 0.7f;
const float kEmphasisMarkFontSizeMultiplier = 0.5f;

#if OS(LINUX) || OS(ANDROID)
// This is the largest VDMX table which we'll try to load and parse.
static const size_t kMaxVDMXTableSize = 1024 * 1024;  // 1 MB
#endif

SimpleFontData::SimpleFontData(const FontPlatformData& platform_data,
                               PassRefPtr<CustomFontData> custom_data,
                               bool is_text_orientation_fallback,
                               bool subpixel_ascent_descent)
    : max_char_width_(-1),
      avg_char_width_(-1),
      platform_data_(platform_data),
      is_text_orientation_fallback_(is_text_orientation_fallback),
      vertical_data_(nullptr),
      has_vertical_glyphs_(false),
      custom_font_data_(std::move(custom_data)) {
  PlatformInit(subpixel_ascent_descent);
  PlatformGlyphInit();
  if (platform_data.IsVerticalAnyUpright() && !is_text_orientation_fallback) {
    vertical_data_ = platform_data.VerticalData();
    has_vertical_glyphs_ =
        vertical_data_.Get() && vertical_data_->HasVerticalMetrics();
  }
}

SimpleFontData::SimpleFontData(const FontPlatformData& platform_data,
                               PassRefPtr<OpenTypeVerticalData> vertical_data)
    : platform_data_(platform_data),
      is_text_orientation_fallback_(false),
      vertical_data_(vertical_data),
      has_vertical_glyphs_(false) {}

void SimpleFontData::PlatformInit(bool subpixel_ascent_descent) {
  if (!platform_data_.size()) {
    font_metrics_.Reset();
    avg_char_width_ = 0;
    max_char_width_ = 0;
    return;
  }

  SkPaint::FontMetrics metrics;

  platform_data_.SetupPaint(&paint_);
  paint_.setTextEncoding(SkPaint::kGlyphID_TextEncoding);
  paint_.getFontMetrics(&metrics);
  SkTypeface* face = paint_.getTypeface();
  DCHECK(face);

  int vdmx_ascent = 0, vdmx_descent = 0;
  bool is_vdmx_valid = false;

#if OS(LINUX) || OS(ANDROID)
  // Manually digging up VDMX metrics is only applicable when bytecode hinting
  // using FreeType.  With DirectWrite or CoreText, no bytecode hinting is ever
  // done.  This code should be pushed into FreeType (hinted font metrics).
  static const uint32_t kVdmxTag = SkSetFourByteTag('V', 'D', 'M', 'X');
  int pixel_size = platform_data_.size() + 0.5;
  if (!paint_.isAutohinted() &&
      (paint_.getHinting() == SkPaint::kFull_Hinting ||
       paint_.getHinting() == SkPaint::kNormal_Hinting)) {
    size_t vdmx_size = face->getTableSize(kVdmxTag);
    if (vdmx_size && vdmx_size < kMaxVDMXTableSize) {
      uint8_t* vdmx_table = (uint8_t*)WTF::Partitions::FastMalloc(
          vdmx_size, WTF_HEAP_PROFILER_TYPE_NAME(SimpleFontData));
      if (vdmx_table &&
          face->getTableData(kVdmxTag, 0, vdmx_size, vdmx_table) == vdmx_size &&
          ParseVDMX(&vdmx_ascent, &vdmx_descent, vdmx_table, vdmx_size,
                    pixel_size))
        is_vdmx_valid = true;
      WTF::Partitions::FastFree(vdmx_table);
    }
  }
#endif

  float ascent;
  float descent;

  // Beware those who step here: This code is designed to match Win32 font
  // metrics *exactly* except:
  // - the adjustment of ascent/descent on Linux/Android
  // - metrics.fAscent and .fDesscent are not rounded to int for tiny fonts
  if (is_vdmx_valid) {
    ascent = vdmx_ascent;
    descent = -vdmx_descent;
  } else {
    // For tiny fonts, the rounding of fAscent and fDescent results in equal
    // baseline for different types of text baselines (crbug.com/338908).
    // Please see CanvasRenderingContext2D::getFontBaseline for the heuristic.
    if (subpixel_ascent_descent &&
        (-metrics.fAscent < 3 || -metrics.fAscent + metrics.fDescent < 2)) {
      ascent = -metrics.fAscent;
      descent = metrics.fDescent;
    } else {
      ascent = SkScalarRoundToScalar(-metrics.fAscent);
      descent = SkScalarRoundToScalar(metrics.fDescent);
    }
#if OS(LINUX) || OS(ANDROID)
    // When subpixel positioning is enabled, if the descent is rounded down, the
    // descent part of the glyph may be truncated when displayed in a 'overflow:
    // hidden' container.  To avoid that, borrow 1 unit from the ascent when
    // possible.
    // FIXME: This can be removed if sub-pixel ascent/descent is supported.
    if (PlatformData().GetFontRenderStyle().use_subpixel_positioning &&
        descent < SkScalarToFloat(metrics.fDescent) && ascent >= 1) {
      ++descent;
      --ascent;
    }
#endif
  }

#if OS(MACOSX)
  // We are preserving this ascent hack to match Safari's ascent adjustment
  // in their SimpleFontDataMac.mm, for details see crbug.com/445830.
  // We need to adjust Times, Helvetica, and Courier to closely match the
  // vertical metrics of their Microsoft counterparts that are the de facto
  // web standard. The AppKit adjustment of 20% is too big and is
  // incorrectly added to line spacing, so we use a 15% adjustment instead
  // and add it to the ascent.
  DEFINE_STATIC_LOCAL(AtomicString, times_name, ("Times"));
  DEFINE_STATIC_LOCAL(AtomicString, helvetica_name, ("Helvetica"));
  DEFINE_STATIC_LOCAL(AtomicString, courier_name, ("Courier"));
  String family_name = platform_data_.FontFamilyName();
  if (family_name == times_name || family_name == helvetica_name ||
      family_name == courier_name)
    ascent += floorf(((ascent + descent) * 0.15f) + 0.5f);
#endif

  font_metrics_.SetAscent(ascent);
  font_metrics_.SetDescent(descent);

  float x_height;
  if (metrics.fXHeight) {
    x_height = metrics.fXHeight;
#if OS(MACOSX)
    // Mac OS CTFontGetXHeight reports the bounding box height of x,
    // including parts extending below the baseline and apparently no x-height
    // value from the OS/2 table. However, the CSS ex unit
    // expects only parts above the baseline, hence measuring the glyph:
    // http://www.w3.org/TR/css3-values/#ex-unit
    const Glyph x_glyph = GlyphForCharacter('x');
    if (x_glyph) {
      FloatRect glyph_bounds(BoundsForGlyph(x_glyph));
      // SkGlyph bounds, y down, based on rendering at (0,0).
      x_height = -glyph_bounds.Y();
    }
#endif
    font_metrics_.SetXHeight(x_height);
  } else {
    x_height = ascent * 0.56;  // Best guess from Windows font metrics.
    font_metrics_.SetXHeight(x_height);
    font_metrics_.SetHasXHeight(false);
  }

  float line_gap = SkScalarToFloat(metrics.fLeading);
  font_metrics_.SetLineGap(line_gap);
  font_metrics_.SetLineSpacing(lroundf(ascent) + lroundf(descent) +
                               lroundf(line_gap));

  if (PlatformData().IsVerticalAnyUpright() && !IsTextOrientationFallback()) {
    static const uint32_t kVheaTag = SkSetFourByteTag('v', 'h', 'e', 'a');
    static const uint32_t kVorgTag = SkSetFourByteTag('V', 'O', 'R', 'G');
    size_t vhea_size = face->getTableSize(kVheaTag);
    size_t vorg_size = face->getTableSize(kVorgTag);
    if ((vhea_size > 0) || (vorg_size > 0))
      has_vertical_glyphs_ = true;
  }

// In WebKit/WebCore/platform/graphics/SimpleFontData.cpp, m_spaceWidth is
// calculated for us, but we need to calculate m_maxCharWidth and
// m_avgCharWidth in order for text entry widgets to be sized correctly.
#if OS(WIN)
  max_char_width_ = SkScalarRoundToInt(metrics.fMaxCharWidth);

  // Older version of the DirectWrite API doesn't implement support for max
  // char width. Fall back on a multiple of the ascent. This is entirely
  // arbitrary but comes pretty close to the expected value in most cases.
  if (max_char_width_ < 1)
    max_char_width_ = ascent * 2;
#elif OS(MACOSX)
  // FIXME: The current avg/max character width calculation is not ideal,
  // it should check either the OS2 table or, better yet, query FontMetrics.
  // Sadly FontMetrics provides incorrect data on Mac at the moment.
  // https://crbug.com/420901
  max_char_width_ = std::max(avg_char_width_, font_metrics_.FloatAscent());
#else
  // Better would be to rely on either fMaxCharWidth or fAveCharWidth.
  // skbug.com/3087
  max_char_width_ = SkScalarRoundToInt(metrics.fXMax - metrics.fXMin);

#endif

#if !OS(MACOSX)
  if (metrics.fAvgCharWidth) {
    avg_char_width_ = SkScalarRoundToInt(metrics.fAvgCharWidth);
  } else {
#endif
    avg_char_width_ = x_height;
    const Glyph x_glyph = GlyphForCharacter('x');
    if (x_glyph) {
      avg_char_width_ = WidthForGlyph(x_glyph);
    }
#if !OS(MACOSX)
  }
#endif

  if (int units_per_em = face->getUnitsPerEm())
    font_metrics_.SetUnitsPerEm(units_per_em);
}

void SimpleFontData::PlatformGlyphInit() {
  SkTypeface* typeface = PlatformData().Typeface();
  if (!typeface->countGlyphs()) {
    space_glyph_ = 0;
    space_width_ = 0;
    zero_glyph_ = 0;
    missing_glyph_data_.font_data = this;
    missing_glyph_data_.glyph = 0;
    return;
  }

  // Nasty hack to determine if we should round or ceil space widths.
  // If the font is monospace or fake monospace we ceil to ensure that
  // every character and the space are the same width.  Otherwise we round.
  space_glyph_ = GlyphForCharacter(' ');
  float width = WidthForGlyph(space_glyph_);
  space_width_ = width;
  zero_glyph_ = GlyphForCharacter('0');
  font_metrics_.SetZeroWidth(WidthForGlyph(zero_glyph_));

  missing_glyph_data_.font_data = this;
  missing_glyph_data_.glyph = 0;
}

const SimpleFontData* SimpleFontData::FontDataForCharacter(UChar32) const {
  return this;
}

Glyph SimpleFontData::GlyphForCharacter(UChar32 codepoint) const {
  uint16_t glyph;
  SkTypeface* typeface = PlatformData().Typeface();
  CHECK(typeface);
  typeface->charsToGlyphs(&codepoint, SkTypeface::kUTF32_Encoding, &glyph, 1);
  return glyph;
}

bool SimpleFontData::IsSegmented() const {
  return false;
}

PassRefPtr<SimpleFontData> SimpleFontData::VerticalRightOrientationFontData()
    const {
  if (!derived_font_data_)
    derived_font_data_ = DerivedFontData::Create();
  if (!derived_font_data_->vertical_right_orientation) {
    FontPlatformData vertical_right_platform_data(platform_data_);
    vertical_right_platform_data.SetOrientation(FontOrientation::kHorizontal);
    derived_font_data_->vertical_right_orientation =
        Create(vertical_right_platform_data,
               IsCustomFont() ? CustomFontData::Create() : nullptr, true);
  }
  return derived_font_data_->vertical_right_orientation;
}

PassRefPtr<SimpleFontData> SimpleFontData::UprightOrientationFontData() const {
  if (!derived_font_data_)
    derived_font_data_ = DerivedFontData::Create();
  if (!derived_font_data_->upright_orientation)
    derived_font_data_->upright_orientation =
        Create(platform_data_,
               IsCustomFont() ? CustomFontData::Create() : nullptr, true);
  return derived_font_data_->upright_orientation;
}

PassRefPtr<SimpleFontData> SimpleFontData::SmallCapsFontData(
    const FontDescription& font_description) const {
  if (!derived_font_data_)
    derived_font_data_ = DerivedFontData::Create();
  if (!derived_font_data_->small_caps)
    derived_font_data_->small_caps =
        CreateScaledFontData(font_description, kSmallCapsFontSizeMultiplier);

  return derived_font_data_->small_caps;
}

PassRefPtr<SimpleFontData> SimpleFontData::EmphasisMarkFontData(
    const FontDescription& font_description) const {
  if (!derived_font_data_)
    derived_font_data_ = DerivedFontData::Create();
  if (!derived_font_data_->emphasis_mark)
    derived_font_data_->emphasis_mark =
        CreateScaledFontData(font_description, kEmphasisMarkFontSizeMultiplier);

  return derived_font_data_->emphasis_mark;
}

bool SimpleFontData::IsTextOrientationFallbackOf(
    const SimpleFontData* font_data) const {
  if (!IsTextOrientationFallback() || !font_data->derived_font_data_)
    return false;
  return font_data->derived_font_data_->upright_orientation == this ||
         font_data->derived_font_data_->vertical_right_orientation == this;
}

std::unique_ptr<SimpleFontData::DerivedFontData>
SimpleFontData::DerivedFontData::Create() {
  return WTF::WrapUnique(new DerivedFontData());
}

PassRefPtr<SimpleFontData> SimpleFontData::CreateScaledFontData(
    const FontDescription& font_description,
    float scale_factor) const {
  const float scaled_size =
      lroundf(font_description.ComputedSize() * scale_factor);
  return SimpleFontData::Create(
      FontPlatformData(platform_data_, scaled_size),
      IsCustomFont() ? CustomFontData::Create() : nullptr);
}

FloatRect SimpleFontData::PlatformBoundsForGlyph(Glyph glyph) const {
  if (!platform_data_.size())
    return FloatRect();

  static_assert(sizeof(glyph) == 2, "Glyph id should not be truncated.");

  SkRect bounds;
  SkiaTextMetrics(&paint_).GetSkiaBoundsForGlyph(glyph, &bounds);
  return FloatRect(bounds);
}

float SimpleFontData::PlatformWidthForGlyph(Glyph glyph) const {
  if (!platform_data_.size())
    return 0;

  static_assert(sizeof(glyph) == 2, "Glyph id should not be truncated.");

  return SkiaTextMetrics(&paint_).GetSkiaWidthForGlyph(glyph);
}

}  // namespace blink
