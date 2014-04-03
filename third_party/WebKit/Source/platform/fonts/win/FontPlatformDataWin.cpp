/*
 * Copyright (C) 2006, 2007 Apple Computer, Inc.
 * Copyright (c) 2006, 2007, 2008, 2009, 2012 Google Inc. All rights reserved.
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

#include "config.h"
#include "platform/fonts/FontPlatformData.h"

#include "SkTypeface.h"
#include "platform/LayoutTestSupport.h"
#include "platform/fonts/FontCache.h"
#include "platform/fonts/harfbuzz/HarfBuzzFace.h"
#include "platform/graphics/GraphicsContext.h"
#include "public/platform/Platform.h"
#include "public/platform/win/WebSandboxSupport.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/StdLibExtras.h"
#include <windows.h>

namespace WebCore {

void FontPlatformData::setupPaint(SkPaint* paint, GraphicsContext* context) const
{
    const float ts = m_textSize >= 0 ? m_textSize : 12;
    paint->setTextSize(SkFloatToScalar(m_textSize));
    paint->setTypeface(typeface());
    paint->setFakeBoldText(m_syntheticBold);
    paint->setTextSkewX(m_syntheticItalic ? -SK_Scalar1 / 4 : 0);

    uint32_t textFlags = paintTextFlags();
    uint32_t flags = paint->getFlags();
    static const uint32_t textFlagsMask = SkPaint::kAntiAlias_Flag |
        SkPaint::kLCDRenderText_Flag |
        SkPaint::kGenA8FromLCD_Flag;
    flags &= ~textFlagsMask;

    if (ts >= m_minSizeForAntiAlias) {
        paint->setSubpixelText(m_useSubpixelPositioning);

        // Only set painting flags when we're actually painting.
        if (context && !context->couldUseLCDRenderedText()) {
            textFlags &= ~SkPaint::kLCDRenderText_Flag;
            // If we *just* clear our request for LCD, then GDI seems to
            // sometimes give us AA text, and sometimes give us BW text. Since the
            // original intent was LCD, we want to force AA (rather than BW), so we
            // add a special bit to tell Skia to do its best to avoid the BW: by
            // drawing LCD offscreen and downsampling that to AA.
            textFlags |= SkPaint::kGenA8FromLCD_Flag;
        }
        SkASSERT(!(textFlags & ~textFlagsMask));
        flags |= textFlags;

    } else {
        paint->setSubpixelText(false);
    }

    paint->setFlags(flags);
}

// Lookup the current system settings for font smoothing.
// We cache these values for performance, but if the browser has a way to be
// notified when these change, we could re-query them at that time.
static uint32_t getSystemTextFlags()
{
    static bool gInited;
    static uint32_t gFlags;
    if (!gInited) {
        BOOL enabled;
        gFlags = 0;
        if (SystemParametersInfo(SPI_GETFONTSMOOTHING, 0, &enabled, 0) && enabled) {
            gFlags |= SkPaint::kAntiAlias_Flag;

            UINT smoothType;
            if (SystemParametersInfo(SPI_GETFONTSMOOTHINGTYPE, 0, &smoothType, 0)) {
                if (FE_FONTSMOOTHINGCLEARTYPE == smoothType)
                    gFlags |= SkPaint::kLCDRenderText_Flag;
            }
        }
        gInited = true;
    }
    return gFlags;
}

static bool isWebFont(const String& familyName)
{
    // Web-fonts have artifical names constructed to always be:
    // 1. 24 characters, followed by a '\0'
    // 2. the last two characters are '=='
    return familyName.length() == 24
        && '=' == familyName[22] && '=' == familyName[23];
}

static int computePaintTextFlags(String fontFamilyName)
{
    int textFlags = getSystemTextFlags();

    // Many web-fonts are so poorly hinted that they are terrible to read when drawn in BW.
    // In these cases, we have decided to FORCE these fonts to be drawn with at least grayscale AA,
    // even when the System (getSystemTextFlags) tells us to draw only in BW.
    if (isWebFont(fontFamilyName) && !isRunningLayoutTest())
        textFlags |= SkPaint::kAntiAlias_Flag;
    return textFlags;
}

FontPlatformData::FontPlatformData(WTF::HashTableDeletedValueType)
    : m_textSize(-1)
    , m_syntheticBold(false)
    , m_syntheticItalic(false)
    , m_orientation(Horizontal)
    , m_typeface(adoptRef(SkTypeface::RefDefault()))
    , m_paintTextFlags(0)
    , m_isHashTableDeletedValue(true)
    , m_useSubpixelPositioning(false)
    , m_minSizeForAntiAlias(0)
{
}

FontPlatformData::FontPlatformData()
    : m_textSize(0)
    , m_syntheticBold(false)
    , m_syntheticItalic(false)
    , m_orientation(Horizontal)
    , m_typeface(adoptRef(SkTypeface::RefDefault()))
    , m_paintTextFlags(0)
    , m_isHashTableDeletedValue(false)
    , m_useSubpixelPositioning(false)
    , m_minSizeForAntiAlias(0)
{
}

// FIXME: this constructor is needed for SVG fonts but doesn't seem to do much
FontPlatformData::FontPlatformData(float size, bool bold, bool oblique)
    : m_textSize(size)
    , m_syntheticBold(false)
    , m_syntheticItalic(false)
    , m_orientation(Horizontal)
    , m_typeface(adoptRef(SkTypeface::RefDefault()))
    , m_paintTextFlags(0)
    , m_isHashTableDeletedValue(false)
    , m_useSubpixelPositioning(false)
    , m_minSizeForAntiAlias(0)
{
}

FontPlatformData::FontPlatformData(const FontPlatformData& data)
    : m_textSize(data.m_textSize)
    , m_syntheticBold(data.m_syntheticBold)
    , m_syntheticItalic(data.m_syntheticItalic)
    , m_orientation(data.m_orientation)
    , m_typeface(data.m_typeface)
    , m_paintTextFlags(data.m_paintTextFlags)
    , m_isHashTableDeletedValue(false)
    , m_useSubpixelPositioning(data.m_useSubpixelPositioning)
    , m_minSizeForAntiAlias(data.m_minSizeForAntiAlias)
{
}

FontPlatformData::FontPlatformData(const FontPlatformData& data, float textSize)
    : m_textSize(textSize)
    , m_syntheticBold(data.m_syntheticBold)
    , m_syntheticItalic(data.m_syntheticItalic)
    , m_orientation(data.m_orientation)
    , m_typeface(data.m_typeface)
    , m_paintTextFlags(data.m_paintTextFlags)
    , m_isHashTableDeletedValue(false)
    , m_useSubpixelPositioning(data.m_useSubpixelPositioning)
    , m_minSizeForAntiAlias(data.m_minSizeForAntiAlias)
{
}

FontPlatformData::FontPlatformData(PassRefPtr<SkTypeface> tf, const char* family,
    float textSize, bool syntheticBold, bool syntheticItalic, FontOrientation orientation,
    bool useSubpixelPositioning)
    : m_textSize(textSize)
    , m_syntheticBold(syntheticBold)
    , m_syntheticItalic(syntheticItalic)
    , m_orientation(orientation)
    , m_typeface(tf)
    , m_isHashTableDeletedValue(false)
    , m_useSubpixelPositioning(useSubpixelPositioning)
    , m_minSizeForAntiAlias(0)
{
    m_paintTextFlags = computePaintTextFlags(fontFamilyName());
}

FontPlatformData::~FontPlatformData()
{
}

FontPlatformData& FontPlatformData::operator=(const FontPlatformData& data)
{
    if (this != &data) {
        m_textSize = data.m_textSize;
        m_syntheticBold = data.m_syntheticBold;
        m_syntheticItalic = data.m_syntheticItalic;
        m_orientation = data.m_orientation;
        m_typeface = data.m_typeface;
        m_paintTextFlags = data.m_paintTextFlags;
        m_minSizeForAntiAlias = data.m_minSizeForAntiAlias;
    }
    return *this;
}

String FontPlatformData::fontFamilyName() const
{
    // FIXME: This returns the requested name, perhaps a better solution would be to
    // return the list of names provided by SkTypeface::createFamilyNameIterator.
    ASSERT(typeface());
    SkString familyName;
    typeface()->getFamilyName(&familyName);
    return String::fromUTF8(familyName.c_str());
}

bool FontPlatformData::isFixedPitch() const
{
    return typeface() && typeface()->isFixedPitch();
}

bool FontPlatformData::operator==(const FontPlatformData& a) const
{
    return SkTypeface::Equal(m_typeface.get(), a.m_typeface.get())
        && m_textSize == a.m_textSize
        && m_syntheticBold == a.m_syntheticBold
        && m_syntheticItalic == a.m_syntheticItalic
        && m_orientation == a.m_orientation
        && m_isHashTableDeletedValue == a.m_isHashTableDeletedValue;
}

HarfBuzzFace* FontPlatformData::harfBuzzFace() const
{
    if (!m_harfBuzzFace)
        m_harfBuzzFace = HarfBuzzFace::create(const_cast<FontPlatformData*>(this), uniqueID());

    return m_harfBuzzFace.get();
}

SkFontID FontPlatformData::uniqueID() const
{
    return m_typeface->uniqueID();
}

bool FontPlatformData::defaultUseSubpixelPositioning()
{
    return FontCache::fontCache()->useSubpixelPositioning();
}

#ifndef NDEBUG
String FontPlatformData::description() const
{
    return String();
}
#endif

} // namespace WebCore
