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

#include "config.h"
#include "platform/fonts/FontPlatformData.h"

#include "SkEndian.h"
#include "SkTypeface.h"
#include "platform/fonts/FontCache.h"
#include "platform/fonts/shaping/HarfBuzzFace.h"
#include "wtf/HashMap.h"
#include "wtf/text/StringHash.h"
#include "wtf/text/WTFString.h"

#if OS(MACOSX)
#include "third_party/skia/include/ports/SkTypeface_mac.h"
#endif

using namespace std;

namespace blink {

FontPlatformData::FontPlatformData(WTF::HashTableDeletedValueType)
    : m_typeface(nullptr)
#if !OS(WIN)
    , m_family(CString())
#endif
    , m_textSize(0)
    , m_syntheticBold(false)
    , m_syntheticItalic(false)
    , m_orientation(Horizontal)
#if OS(MACOSX)
    , m_isColorBitmapFont(false)
    , m_isCompositeFontReference(false)
#endif
    , m_widthVariant(RegularWidth)
#if OS(MACOSX)
    , m_font(nullptr)
#else
    , m_style(FontRenderStyle())
#endif
    , m_isHashTableDeletedValue(true)
#if OS(WIN)
    , m_paintTextFlags(0)
    , m_useSubpixelPositioning(false)
    , m_minSizeForAntiAlias(0)
    , m_minSizeForSubpixel(0)
#endif
{
}

FontPlatformData::FontPlatformData()
    : m_typeface(nullptr)
#if !OS(WIN)
    , m_family(CString())
#endif
    , m_textSize(0)
    , m_syntheticBold(false)
    , m_syntheticItalic(false)
    , m_orientation(Horizontal)
#if OS(MACOSX)
    , m_isColorBitmapFont(false)
    , m_isCompositeFontReference(false)
#endif
    , m_widthVariant(RegularWidth)
#if OS(MACOSX)
    , m_font(nullptr)
#else
    , m_style(FontRenderStyle())
#endif
    , m_isHashTableDeletedValue(false)
#if OS(WIN)
    , m_paintTextFlags(0)
    , m_useSubpixelPositioning(false)
    , m_minSizeForAntiAlias(0)
    , m_minSizeForSubpixel(0)
#endif
{
}

FontPlatformData::FontPlatformData(float size, bool syntheticBold, bool syntheticItalic, FontOrientation orientation, FontWidthVariant widthVariant)
    : m_typeface(nullptr)
#if !OS(WIN)
    , m_family(CString())
#endif
    , m_textSize(size)
    , m_syntheticBold(syntheticBold)
    , m_syntheticItalic(syntheticItalic)
    , m_orientation(orientation)
#if OS(MACOSX)
    , m_isColorBitmapFont(false)
    , m_isCompositeFontReference(false)
#endif
    , m_widthVariant(widthVariant)
#if OS(MACOSX)
    , m_font(nullptr)
#else
    , m_style(FontRenderStyle())
#endif
    , m_isHashTableDeletedValue(false)
#if OS(WIN)
    , m_paintTextFlags(0)
    , m_useSubpixelPositioning(false)
    , m_minSizeForAntiAlias(0)
    , m_minSizeForSubpixel(0)
#endif
{
}

FontPlatformData::FontPlatformData(const FontPlatformData& source)
    : m_typeface(source.m_typeface)
#if !OS(WIN)
    , m_family(source.m_family)
#endif
    , m_textSize(source.m_textSize)
    , m_syntheticBold(source.m_syntheticBold)
    , m_syntheticItalic(source.m_syntheticItalic)
    , m_orientation(source.m_orientation)
#if OS(MACOSX)
    , m_isColorBitmapFont(source.m_isColorBitmapFont)
    , m_isCompositeFontReference(source.m_isCompositeFontReference)
#endif
    , m_widthVariant(source.m_widthVariant)
#if !OS(MACOSX)
    , m_style(source.m_style)
#endif
    , m_harfBuzzFace(nullptr)
    , m_isHashTableDeletedValue(false)
#if OS(WIN)
    , m_paintTextFlags(source.m_paintTextFlags)
    , m_useSubpixelPositioning(source.m_useSubpixelPositioning)
    , m_minSizeForAntiAlias(source.m_minSizeForAntiAlias)
    , m_minSizeForSubpixel(source.m_minSizeForSubpixel)
#endif
{
#if OS(MACOSX)
    platformDataInit(source);
#endif
}

FontPlatformData::FontPlatformData(const FontPlatformData& src, float textSize)
    : m_typeface(src.m_typeface)
#if !OS(WIN)
    , m_family(src.m_family)
#endif
    , m_textSize(textSize)
    , m_syntheticBold(src.m_syntheticBold)
    , m_syntheticItalic(src.m_syntheticItalic)
    , m_orientation(src.m_orientation)
#if OS(MACOSX)
    , m_isColorBitmapFont(src.m_isColorBitmapFont)
    , m_isCompositeFontReference(src.m_isCompositeFontReference)
#endif
    , m_widthVariant(RegularWidth)
#if !OS(MACOSX)
    , m_style(src.m_style)
#endif
    , m_harfBuzzFace(nullptr)
    , m_isHashTableDeletedValue(false)
#if OS(WIN)
    , m_paintTextFlags(src.m_paintTextFlags)
    , m_useSubpixelPositioning(src.m_useSubpixelPositioning)
    , m_minSizeForAntiAlias(src.m_minSizeForAntiAlias)
    , m_minSizeForSubpixel(src.m_minSizeForSubpixel)
#endif
{
#if OS(MACOSX)
    platformDataInit(src);
#else
    querySystemForRenderStyle(FontDescription::subpixelPositioning());
#endif
}

#if OS(MACOSX)
FontPlatformData::FontPlatformData(CGFontRef cgFont, float size, bool syntheticBold, bool syntheticItalic, FontOrientation orientation, FontWidthVariant widthVariant)
    : m_typeface(nullptr)
    , m_family(CString())
    , m_textSize(size)
    , m_syntheticBold(syntheticBold)
    , m_syntheticItalic(syntheticItalic)
    , m_orientation(orientation)
    , m_isColorBitmapFont(false)
    , m_isCompositeFontReference(false)
    , m_widthVariant(widthVariant)
    , m_font(nullptr)
    , m_cgFont(cgFont)
    , m_isHashTableDeletedValue(false)
{
}

#else

FontPlatformData::FontPlatformData(PassRefPtr<SkTypeface> tf, const char* family, float textSize, bool syntheticBold, bool syntheticItalic, FontOrientation orientation, bool subpixelTextPosition)
    : m_typeface(tf)
#if !OS(WIN)
    , m_family(family)
#endif
    , m_textSize(textSize)
    , m_syntheticBold(syntheticBold)
    , m_syntheticItalic(syntheticItalic)
    , m_orientation(orientation)
    , m_widthVariant(RegularWidth)
    , m_isHashTableDeletedValue(false)
#if OS(WIN)
    , m_paintTextFlags(0)
    , m_useSubpixelPositioning(subpixelTextPosition)
    , m_minSizeForAntiAlias(0)
    , m_minSizeForSubpixel(0)
#endif
{
    querySystemForRenderStyle(subpixelTextPosition);
}

#endif

FontPlatformData::~FontPlatformData()
{
#if OS(MACOSX)
    if (m_font)
        CFRelease(m_font);
#endif
}

const FontPlatformData& FontPlatformData::operator=(const FontPlatformData& other)
{
    // Check for self-assignment.
    if (this == &other)
        return *this;

    m_typeface = other.m_typeface;
#if !OS(WIN)
    m_family = other.m_family;
#endif
    m_textSize = other.m_textSize;
    m_syntheticBold = other.m_syntheticBold;
    m_syntheticItalic = other.m_syntheticItalic;
    m_harfBuzzFace = nullptr;
    m_orientation = other.m_orientation;
    m_widthVariant = other.m_widthVariant;
#if OS(MACOSX)
    m_isColorBitmapFont = other.m_isColorBitmapFont;
    m_isCompositeFontReference = other.m_isCompositeFontReference;
#else
    m_style = other.m_style;
#endif
    m_widthVariant = other.m_widthVariant;

#if OS(WIN)
    m_paintTextFlags = 0;
    m_minSizeForAntiAlias = other.m_minSizeForAntiAlias;
    m_minSizeForSubpixel = other.m_minSizeForSubpixel;
    m_useSubpixelPositioning = other.m_useSubpixelPositioning;
#endif

#if OS(MACOSX)
    return platformDataAssign(other);
#else
    return *this;
#endif
}

bool FontPlatformData::operator==(const FontPlatformData& a) const
{
    // If either of the typeface pointers are null then we test for pointer
    // equality. Otherwise, we call SkTypeface::Equal on the valid pointers.
    bool typefacesEqual = false;
#if !OS(MACOSX)
    if (!m_typeface || !a.m_typeface)
        typefacesEqual = m_typeface == a.m_typeface;
    else
        typefacesEqual = SkTypeface::Equal(m_typeface.get(), a.m_typeface.get());
#else
    if (m_font || a.m_font)
        typefacesEqual = m_font == a.m_font;
    else
        typefacesEqual = m_cgFont == a.m_cgFont;
#endif

    return typefacesEqual
        && m_textSize == a.m_textSize
        && m_isHashTableDeletedValue == a.m_isHashTableDeletedValue
        && m_syntheticBold == a.m_syntheticBold
        && m_syntheticItalic == a.m_syntheticItalic
        && m_orientation == a.m_orientation
#if !OS(MACOSX)
        && m_style == a.m_style
#else
        && m_isColorBitmapFont == a.m_isColorBitmapFont
        && m_isCompositeFontReference == a.m_isCompositeFontReference
#endif
        && m_widthVariant == a.m_widthVariant;
}

SkFontID FontPlatformData::uniqueID() const
{
    return typeface()->uniqueID();
}

String FontPlatformData::fontFamilyName() const
{
    // FIXME(crbug.com/326582): come up with a proper way of handling SVG.
    if (!this->typeface())
        return "";
    SkTypeface::LocalizedStrings* fontFamilyIterator = this->typeface()->createFamilyNameIterator();
    SkTypeface::LocalizedString localizedString;
    while (fontFamilyIterator->next(&localizedString) && !localizedString.fString.size()) { }
    fontFamilyIterator->unref();
    return String(localizedString.fString.c_str());
}

bool FontPlatformData::isFixedPitch() const
{
    return typeface() && typeface()->isFixedPitch();
}

SkTypeface* FontPlatformData::typeface() const
{
#if OS(MACOSX)
    if (!m_typeface)
        m_typeface = adoptRef(SkCreateTypefaceFromCTFont(ctFont()));
#endif
    return m_typeface.get();
}

HarfBuzzFace* FontPlatformData::harfBuzzFace() const
{
    if (!m_harfBuzzFace)
        m_harfBuzzFace = HarfBuzzFace::create(const_cast<FontPlatformData*>(this), uniqueID());

    return m_harfBuzzFace.get();
}

#if !OS(MACOSX)
unsigned FontPlatformData::hash() const
{
    unsigned h = SkTypeface::UniqueID(m_typeface.get());
    h ^= 0x01010101 * ((static_cast<int>(m_isHashTableDeletedValue) << 3) | (static_cast<int>(m_orientation) << 2) | (static_cast<int>(m_syntheticBold) << 1) | static_cast<int>(m_syntheticItalic));

    // This memcpy is to avoid a reinterpret_cast that breaks strict-aliasing
    // rules. Memcpy is generally optimized enough so that performance doesn't
    // matter here.
    uint32_t textSizeBytes;
    memcpy(&textSizeBytes, &m_textSize, sizeof(uint32_t));
    h ^= textSizeBytes;

    return h;
}

bool FontPlatformData::fontContainsCharacter(UChar32 character)
{
    SkPaint paint;
    setupPaint(&paint);
    paint.setTextEncoding(SkPaint::kUTF32_TextEncoding);

    uint16_t glyph;
    paint.textToGlyphs(&character, sizeof(character), &glyph);
    return glyph;
}

#endif

#if ENABLE(OPENTYPE_VERTICAL)
PassRefPtr<OpenTypeVerticalData> FontPlatformData::verticalData() const
{
    return FontCache::fontCache()->getVerticalData(typeface()->uniqueID(), *this);
}

PassRefPtr<SharedBuffer> FontPlatformData::openTypeTable(uint32_t table) const
{
    RefPtr<SharedBuffer> buffer;

    SkFontTableTag tag = SkEndianSwap32(table);
    const size_t tableSize = m_typeface->getTableSize(tag);
    if (tableSize) {
        Vector<char> tableBuffer(tableSize);
        m_typeface->getTableData(tag, 0, tableSize, &tableBuffer[0]);
        buffer = SharedBuffer::adoptVector(tableBuffer);
    }
    return buffer.release();
}
#endif

} // namespace blink
