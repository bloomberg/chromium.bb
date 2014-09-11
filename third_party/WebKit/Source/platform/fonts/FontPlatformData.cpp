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

#include "SkTypeface.h"
#include "platform/fonts/harfbuzz/HarfBuzzFace.h"
#include "wtf/HashMap.h"
#include "wtf/text/StringHash.h"
#include "wtf/text/WTFString.h"

using namespace std;

namespace blink {

FontPlatformData::FontPlatformData(WTF::HashTableDeletedValueType)
    : m_textSize(0)
    , m_syntheticBold(false)
    , m_syntheticItalic(false)
    , m_orientation(Horizontal)
    , m_isColorBitmapFont(false)
    , m_isCompositeFontReference(false)
    , m_widthVariant(RegularWidth)
#if OS(MACOSX)
    , m_font(nullptr)
#endif
    , m_isHashTableDeletedValue(true)
{
}

FontPlatformData::FontPlatformData()
    : m_textSize(0)
    , m_syntheticBold(false)
    , m_syntheticItalic(false)
    , m_orientation(Horizontal)
    , m_isColorBitmapFont(false)
    , m_isCompositeFontReference(false)
    , m_widthVariant(RegularWidth)
#if OS(MACOSX)
    , m_font(nullptr)
#endif
    , m_isHashTableDeletedValue(false)
{
}

FontPlatformData::FontPlatformData(float size, bool syntheticBold, bool syntheticItalic, FontOrientation orientation, FontWidthVariant widthVariant)
    : m_textSize(size)
    , m_syntheticBold(syntheticBold)
    , m_syntheticItalic(syntheticItalic)
    , m_orientation(orientation)
    , m_isColorBitmapFont(false)
    , m_isCompositeFontReference(false)
    , m_widthVariant(widthVariant)
#if OS(MACOSX)
    , m_font(nullptr)
#endif
    , m_isHashTableDeletedValue(false)
{
}

#if OS(MACOSX)
FontPlatformData::FontPlatformData(CGFontRef cgFont, float size, bool syntheticBold, bool syntheticItalic, FontOrientation orientation, FontWidthVariant widthVariant)
    : m_textSize(size)
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
#endif

FontPlatformData::FontPlatformData(const FontPlatformData& source)
    : m_textSize(source.m_textSize)
    , m_syntheticBold(source.m_syntheticBold)
    , m_syntheticItalic(source.m_syntheticItalic)
    , m_orientation(source.m_orientation)
    , m_isColorBitmapFont(source.m_isColorBitmapFont)
    , m_isCompositeFontReference(source.m_isCompositeFontReference)
    , m_widthVariant(source.m_widthVariant)
    , m_isHashTableDeletedValue(false)
{
    platformDataInit(source);
}

const FontPlatformData& FontPlatformData::operator=(const FontPlatformData& other)
{
    // Check for self-assignment.
    if (this == &other)
        return *this;

    m_syntheticBold = other.m_syntheticBold;
    m_syntheticItalic = other.m_syntheticItalic;
    m_orientation = other.m_orientation;
    m_textSize = other.m_textSize;
    m_widthVariant = other.m_widthVariant;
    m_isColorBitmapFont = other.m_isColorBitmapFont;
    m_isCompositeFontReference = other.m_isCompositeFontReference;

    return platformDataAssign(other);
}

bool FontPlatformData::operator==(const FontPlatformData& a) const
{

    return platformIsEqual(a)
        && m_textSize == a.m_textSize
        && m_isHashTableDeletedValue == a.m_isHashTableDeletedValue
        && m_syntheticBold == a.m_syntheticBold
        && m_syntheticItalic == a.m_syntheticItalic
        && m_orientation == a.m_orientation
#if !OS(MACOSX)
        && m_style == a.m_style
#endif
        && m_isColorBitmapFont == a.m_isColorBitmapFont
        && m_isCompositeFontReference == a.m_isCompositeFontReference
        && m_widthVariant == a.m_widthVariant;
}


} // namespace blink
