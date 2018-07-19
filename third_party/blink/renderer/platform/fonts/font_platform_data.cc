/*
 * Copyright (C) 2011 Brent Fulgham
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

#include "third_party/blink/renderer/platform/fonts/font_platform_data.h"

#include "SkTypeface.h"
#include "build/build_config.h"
#include "hb-ot.h"
#include "hb.h"
#include "third_party/blink/public/platform/linux/web_sandbox_support.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/fonts/font_cache.h"
#include "third_party/blink/renderer/platform/fonts/shaping/harfbuzz_face.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/layout_test_support.h"
#include "third_party/blink/renderer/platform/text/character.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

#if defined(OS_MACOSX)
#include "third_party/skia/include/ports/SkTypeface_mac.h"
#endif

namespace blink {

FontPlatformData::FontPlatformData(WTF::HashTableDeletedValueType)
    : text_size_(0),
      synthetic_bold_(false),
      synthetic_italic_(false),
      avoid_embedded_bitmaps_(false),
      orientation_(FontOrientation::kHorizontal),
      is_hash_table_deleted_value_(true)
#if defined(OS_WIN)
      ,
      paint_text_flags_(0)
#endif
{
}

FontPlatformData::FontPlatformData()
    : text_size_(0),
      synthetic_bold_(false),
      synthetic_italic_(false),
      avoid_embedded_bitmaps_(false),
      orientation_(FontOrientation::kHorizontal),
      is_hash_table_deleted_value_(false)
#if defined(OS_WIN)
      ,
      paint_text_flags_(0)
#endif
{
}

FontPlatformData::FontPlatformData(float size,
                                   bool synthetic_bold,
                                   bool synthetic_italic,
                                   FontOrientation orientation)
    : text_size_(size),
      synthetic_bold_(synthetic_bold),
      synthetic_italic_(synthetic_italic),
      avoid_embedded_bitmaps_(false),
      orientation_(orientation),
      is_hash_table_deleted_value_(false)
#if defined(OS_WIN)
      ,
      paint_text_flags_(0)
#endif
{
}

FontPlatformData::FontPlatformData(const FontPlatformData& source)
    : paint_typeface_(source.paint_typeface_),
#if !defined(OS_WIN)
      family_(source.family_),
#endif
      text_size_(source.text_size_),
      synthetic_bold_(source.synthetic_bold_),
      synthetic_italic_(source.synthetic_italic_),
      avoid_embedded_bitmaps_(source.avoid_embedded_bitmaps_),
      orientation_(source.orientation_),
#if !defined(OS_WIN) && !defined(OS_MACOSX)
      style_(source.style_),
#endif
      harfbuzz_face_(nullptr),
      is_hash_table_deleted_value_(false)
#if defined(OS_WIN)
      ,
      paint_text_flags_(source.paint_text_flags_)
#endif
{
}

FontPlatformData::FontPlatformData(const FontPlatformData& src, float text_size)
    : FontPlatformData(src.paint_typeface_,
#if !defined(OS_WIN)
                       src.family_.data(),
#else
                       CString(),
#endif
                       text_size,
                       src.synthetic_bold_,
                       src.synthetic_italic_,
                       src.orientation_) {
}

FontPlatformData::FontPlatformData(const PaintTypeface& paint_tf,
                                   const CString& family,
                                   float text_size,
                                   bool synthetic_bold,
                                   bool synthetic_italic,
                                   FontOrientation orientation)
    : paint_typeface_(paint_tf),
#if !defined(OS_WIN)
      family_(family),
#endif
      text_size_(text_size),
      synthetic_bold_(synthetic_bold),
      synthetic_italic_(synthetic_italic),
      avoid_embedded_bitmaps_(false),
      orientation_(orientation),
      is_hash_table_deleted_value_(false)
#if defined(OS_WIN)
      ,
      paint_text_flags_(0)
#endif
{
#if !defined(OS_WIN) && !defined(OS_MACOSX)
  style_ = WebFontRenderStyle::GetDefault();
  auto system_style = QuerySystemRenderStyle(
      family_, text_size_, paint_typeface_.ToSkTypeface()->fontStyle());

  // In layout tests, ignore system preference for subpixel positioning,
  // or explicitly disable if requested.
  if (LayoutTestSupport::IsRunningLayoutTest()) {
    system_style.use_subpixel_positioning =
        LayoutTestSupport::IsTextSubpixelPositioningAllowedForTest()
            ? WebFontRenderStyle::kNoPreference
            : 0;
  }

  style_.OverrideWith(system_style);
#endif

#if defined(OS_WIN)
  QuerySystemForRenderStyle();
#endif
}

FontPlatformData::~FontPlatformData() = default;

#if defined(OS_MACOSX)
CTFontRef FontPlatformData::CtFont() const {
  return SkTypeface_GetCTFontRef(paint_typeface_.ToSkTypeface().get());
};

CGFontRef FontPlatformData::CgFont() const {
  if (!CtFont())
    return nullptr;
  return CTFontCopyGraphicsFont(CtFont(), 0);
}
#endif

const FontPlatformData& FontPlatformData::operator=(
    const FontPlatformData& other) {
  // Check for self-assignment.
  if (this == &other)
    return *this;

  paint_typeface_ = other.paint_typeface_;
#if !defined(OS_WIN)
  family_ = other.family_;
#endif
  text_size_ = other.text_size_;
  synthetic_bold_ = other.synthetic_bold_;
  synthetic_italic_ = other.synthetic_italic_;
  avoid_embedded_bitmaps_ = other.avoid_embedded_bitmaps_;
  harfbuzz_face_ = nullptr;
  orientation_ = other.orientation_;
#if !defined(OS_WIN) && !defined(OS_MACOSX)
  style_ = other.style_;
#endif

#if defined(OS_WIN)
  paint_text_flags_ = 0;
#endif

  return *this;
}

bool FontPlatformData::operator==(const FontPlatformData& a) const {
  // If either of the typeface pointers are null then we test for pointer
  // equality. Otherwise, we call SkTypeface::Equal on the valid pointers.
  bool typefaces_equal = false;
  if (!Typeface() || !a.Typeface())
    typefaces_equal = Typeface() == a.Typeface();
  else
    typefaces_equal = SkTypeface::Equal(Typeface(), a.Typeface());

  return typefaces_equal && text_size_ == a.text_size_ &&
         is_hash_table_deleted_value_ == a.is_hash_table_deleted_value_ &&
         synthetic_bold_ == a.synthetic_bold_ &&
         synthetic_italic_ == a.synthetic_italic_ &&
         avoid_embedded_bitmaps_ == a.avoid_embedded_bitmaps_
#if !defined(OS_WIN) && !defined(OS_MACOSX)
         && style_ == a.style_
#endif
         && orientation_ == a.orientation_;
}

SkFontID FontPlatformData::UniqueID() const {
  return Typeface()->uniqueID();
}

String FontPlatformData::FontFamilyName() const {
  DCHECK(this->Typeface());
  SkTypeface::LocalizedStrings* font_family_iterator =
      this->Typeface()->createFamilyNameIterator();
  SkTypeface::LocalizedString localized_string;
  while (font_family_iterator->next(&localized_string) &&
         !localized_string.fString.size()) {
  }
  font_family_iterator->unref();
  return String::FromUTF8(localized_string.fString.c_str());
}

SkTypeface* FontPlatformData::Typeface() const {
  return paint_typeface_.ToSkTypeface().get();
}

HarfBuzzFace* FontPlatformData::GetHarfBuzzFace() const {
  if (!harfbuzz_face_)
    harfbuzz_face_ =
        HarfBuzzFace::Create(const_cast<FontPlatformData*>(this), UniqueID());

  return harfbuzz_face_.get();
}

bool FontPlatformData::HasSpaceInLigaturesOrKerning(
    TypesettingFeatures features) const {
  HarfBuzzFace* hb_face = GetHarfBuzzFace();
  if (!hb_face)
    return false;

  return hb_face->HasSpaceInLigaturesOrKerning(features);
}

unsigned FontPlatformData::GetHash() const {
  unsigned h = SkTypeface::UniqueID(Typeface());
  h ^= 0x01010101 * ((static_cast<int>(is_hash_table_deleted_value_) << 3) |
                     (static_cast<int>(orientation_) << 2) |
                     (static_cast<int>(synthetic_bold_) << 1) |
                     static_cast<int>(synthetic_italic_));

  // This memcpy is to avoid a reinterpret_cast that breaks strict-aliasing
  // rules. Memcpy is generally optimized enough so that performance doesn't
  // matter here.
  uint32_t text_size_bytes;
  memcpy(&text_size_bytes, &text_size_, sizeof(uint32_t));
  h ^= text_size_bytes;

  return h;
}

#if !defined(OS_MACOSX)
bool FontPlatformData::FontContainsCharacter(UChar32 character) {
  PaintFont font;
  SetupPaintFont(&font);
  font.SetTextEncoding(SkPaint::kUTF32_TextEncoding);

  uint16_t glyph;
  font.ToSkPaint().textToGlyphs(&character, sizeof(character), &glyph);
  return glyph;
}
#endif

#if !defined(OS_MACOSX) && !defined(OS_WIN)
// static
WebFontRenderStyle FontPlatformData::QuerySystemRenderStyle(
    const CString& family,
    float text_size,
    SkFontStyle font_style) {
  WebFontRenderStyle result;

#if !defined(OS_ANDROID) && !defined(OS_FUCHSIA)
  // If the font name is missing (i.e. probably a web font) or the sandbox is
  // disabled, use the system defaults.
  if (family.length() && Platform::Current()->GetSandboxSupport()) {
    bool is_bold = font_style.weight() >= SkFontStyle::kSemiBold_Weight;
    bool is_italic = font_style.slant() != SkFontStyle::kUpright_Slant;
    Platform::Current()->GetSandboxSupport()->GetWebFontRenderStyleForStrike(
        family.data(), text_size, is_bold, is_italic,
        FontCache::DeviceScaleFactor(), &result);
  }
#endif

  return result;
}

void FontPlatformData::SetupPaintFont(PaintFont* font,
                                      float device_scale_factor,
                                      const Font*) const {
  style_.ApplyToPaintFont(*font, device_scale_factor);

  const float ts = text_size_ >= 0 ? text_size_ : 12;
  font->SetTextSize(SkFloatToScalar(ts));
  font->SetTypeface(paint_typeface_);
  font->SetFakeBoldText(synthetic_bold_);
  font->SetTextSkewX(synthetic_italic_ ? -SK_Scalar1 / 4 : 0);

  font->SetEmbeddedBitmapText(!avoid_embedded_bitmaps_);
}
#endif

const PaintTypeface& FontPlatformData::GetPaintTypeface() const {
  return paint_typeface_;
}

}  // namespace blink
