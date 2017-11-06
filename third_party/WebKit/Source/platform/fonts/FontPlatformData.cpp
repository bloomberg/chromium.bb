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

#include "platform/fonts/FontPlatformData.h"

#include "SkTypeface.h"
#include "build/build_config.h"
#include "hb-ot.h"
#include "hb.h"
#include "platform/fonts/FontCache.h"
#include "platform/fonts/shaping/HarfBuzzFace.h"
#include "platform/text/Character.h"
#include "platform/wtf/ByteSwap.h"
#include "platform/wtf/HashMap.h"
#include "platform/wtf/text/StringHash.h"
#include "platform/wtf/text/WTFString.h"

#if defined(OS_MACOSX)
#include "third_party/skia/include/ports/SkTypeface_mac.h"
#endif

namespace blink {

FontPlatformData::FontPlatformData(WTF::HashTableDeletedValueType)
    : typeface_(nullptr),
      text_size_(0),
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
    : typeface_(nullptr),
      text_size_(0),
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
    : typeface_(nullptr),
      text_size_(size),
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
    : typeface_(source.typeface_),
#if !defined(OS_WIN)
      family_(source.family_),
#endif
      text_size_(source.text_size_),
      synthetic_bold_(source.synthetic_bold_),
      synthetic_italic_(source.synthetic_italic_),
      avoid_embedded_bitmaps_(source.avoid_embedded_bitmaps_),
      orientation_(source.orientation_),
#if defined(OS_LINUX) || defined(OS_ANDROID)
      style_(source.style_),
#endif
      harf_buzz_face_(nullptr),
      is_hash_table_deleted_value_(false)
#if defined(OS_WIN)
      ,
      paint_text_flags_(source.paint_text_flags_)
#endif
{
}

FontPlatformData::FontPlatformData(const FontPlatformData& src, float text_size)
    : typeface_(src.typeface_),
#if !defined(OS_WIN)
      family_(src.family_),
#endif
      text_size_(text_size),
      synthetic_bold_(src.synthetic_bold_),
      synthetic_italic_(src.synthetic_italic_),
      avoid_embedded_bitmaps_(false),
      orientation_(src.orientation_),
#if defined(OS_LINUX) || defined(OS_ANDROID)
      style_(FontRenderStyle::QuerySystem(family_,
                                          text_size_,
                                          typeface_->fontStyle())),
#endif
      harf_buzz_face_(nullptr),
      is_hash_table_deleted_value_(false)
#if defined(OS_WIN)
      ,
      paint_text_flags_(src.paint_text_flags_)
#endif
{
#if defined(OS_WIN)
  QuerySystemForRenderStyle();
#endif
}

FontPlatformData::FontPlatformData(sk_sp<SkTypeface> tf,
                                   const char* family,
                                   float text_size,
                                   bool synthetic_bold,
                                   bool synthetic_italic,
                                   FontOrientation orientation)
    : typeface_(std::move(tf)),
#if !defined(OS_WIN)
      family_(family),
#endif
      text_size_(text_size),
      synthetic_bold_(synthetic_bold),
      synthetic_italic_(synthetic_italic),
      avoid_embedded_bitmaps_(false),
      orientation_(orientation),
#if defined(OS_LINUX) || defined(OS_ANDROID)
      style_(FontRenderStyle::QuerySystem(family_,
                                          text_size_,
                                          typeface_->fontStyle())),
#endif
      is_hash_table_deleted_value_(false)
#if defined(OS_WIN)
      ,
      paint_text_flags_(0)
#endif
{
#if defined(OS_WIN)
  QuerySystemForRenderStyle();
#endif
}

FontPlatformData::~FontPlatformData() {}

#if defined(OS_MACOSX)
CTFontRef FontPlatformData::CtFont() const {
  return SkTypeface_GetCTFontRef(typeface_.get());
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

  typeface_ = other.typeface_;
#if !defined(OS_WIN)
  family_ = other.family_;
#endif
  text_size_ = other.text_size_;
  synthetic_bold_ = other.synthetic_bold_;
  synthetic_italic_ = other.synthetic_italic_;
  avoid_embedded_bitmaps_ = other.avoid_embedded_bitmaps_;
  harf_buzz_face_ = nullptr;
  orientation_ = other.orientation_;
#if defined(OS_LINUX) || defined(OS_ANDROID)
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
#if defined(OS_LINUX) || defined(OS_ANDROID)
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
  return String(localized_string.fString.c_str());
}

SkTypeface* FontPlatformData::Typeface() const {
  return typeface_.get();
}

HarfBuzzFace* FontPlatformData::GetHarfBuzzFace() const {
  if (!harf_buzz_face_)
    harf_buzz_face_ =
        HarfBuzzFace::Create(const_cast<FontPlatformData*>(this), UniqueID());

  return harf_buzz_face_.get();
}

static inline bool TableHasSpace(hb_face_t* face,
                                 hb_set_t* glyphs,
                                 hb_tag_t tag,
                                 hb_codepoint_t space) {
  unsigned count = hb_ot_layout_table_get_lookup_count(face, tag);
  for (unsigned i = 0; i < count; i++) {
    hb_ot_layout_lookup_collect_glyphs(face, tag, i, glyphs, glyphs, glyphs,
                                       nullptr);
    if (hb_set_has(glyphs, space))
      return true;
  }
  return false;
}

bool FontPlatformData::HasSpaceInLigaturesOrKerning(
    TypesettingFeatures features) const {
  HarfBuzzFace* hb_face = GetHarfBuzzFace();
  if (!hb_face)
    return false;

  hb_font_t* font = hb_face->GetScaledFont();
  DCHECK(font);
  hb_face_t* face = hb_font_get_face(font);
  DCHECK(face);

  hb_codepoint_t space;
  // If the space glyph isn't present in the font then each space character
  // will be rendering using a fallback font, which grantees that it cannot
  // affect the shape of the preceding word.
  if (!hb_font_get_glyph(font, kSpaceCharacter, 0, &space))
    return false;

  if (!hb_ot_layout_has_substitution(face) &&
      !hb_ot_layout_has_positioning(face)) {
    return false;
  }

  bool found_space_in_table = false;
  hb_set_t* glyphs = hb_set_create();
  if (features & kKerning)
    found_space_in_table = TableHasSpace(face, glyphs, HB_OT_TAG_GPOS, space);
  if (!found_space_in_table && (features & kLigatures))
    found_space_in_table = TableHasSpace(face, glyphs, HB_OT_TAG_GSUB, space);

  hb_set_destroy(glyphs);

  return found_space_in_table;
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
  SkPaint paint;
  SetupPaint(&paint);
  paint.setTextEncoding(SkPaint::kUTF32_TextEncoding);

  uint16_t glyph;
  paint.textToGlyphs(&character, sizeof(character), &glyph);
  return glyph;
}

#endif

scoped_refptr<OpenTypeVerticalData> FontPlatformData::VerticalData() const {
  return FontCache::GetFontCache()->GetVerticalData(Typeface()->uniqueID(),
                                                    *this);
}

Vector<char> FontPlatformData::OpenTypeTable(SkFontTableTag tag) const {
  Vector<char> table_buffer;

  const size_t table_size = typeface_->getTableSize(tag);
  if (table_size) {
    table_buffer.resize(table_size);
    typeface_->getTableData(tag, 0, table_size, &table_buffer[0]);
  }
  return table_buffer;
}

}  // namespace blink
