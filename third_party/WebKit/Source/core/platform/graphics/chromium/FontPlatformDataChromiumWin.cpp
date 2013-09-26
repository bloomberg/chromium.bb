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
#include "core/platform/graphics/FontPlatformData.h"

#include <windows.h>
#include <mlang.h>
#include <objidl.h>
#include "core/platform/LayoutTestSupport.h"
#include "core/platform/SharedBuffer.h"
#include "core/platform/graphics/FontCache.h"
#include "core/platform/graphics/GraphicsContext.h"
#include "core/platform/graphics/skia/SkiaFontWin.h"
#include "core/platform/win/HWndDC.h"
#include "public/platform/Platform.h"
#include "public/platform/win/WebSandboxSupport.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "third_party/skia/include/ports/SkTypeface_win.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/StdLibExtras.h"

namespace WebCore {

void FontPlatformData::setupPaint(SkPaint* paint, GraphicsContext* context) const
{
    const float ts = m_size >= 0 ? m_size : 12;
    paint->setTextSize(SkFloatToScalar(m_size));
    paint->setTypeface(typeface());

    // Only set painting flags when we're actually painting.
    if (context) {
        int textFlags = paintTextFlags();
        if (!context->couldUseLCDRenderedText()) {
            textFlags &= ~SkPaint::kLCDRenderText_Flag;
            // If we *just* clear our request for LCD, then GDI seems to
            // sometimes give us AA text, and sometimes give us BW text. Since the
            // original intent was LCD, we want to force AA (rather than BW), so we
            // add a special bit to tell Skia to do its best to avoid the BW: by
            // drawing LCD offscreen and downsampling that to AA.
            textFlags |= SkPaint::kGenA8FromLCD_Flag;
        }

        static const uint32_t textFlagsMask = SkPaint::kAntiAlias_Flag |
            SkPaint::kLCDRenderText_Flag |
            SkPaint::kGenA8FromLCD_Flag;

        SkASSERT(!(textFlags & ~textFlagsMask));
        uint32_t flags = paint->getFlags();
        flags &= ~textFlagsMask;
        flags |= textFlags;
        paint->setFlags(flags);
    }
}

// Lookup the current system settings for font smoothing.
// We cache these values for performance, but if the browser has a way to be
// notified when these change, we could re-query them at that time.
static uint32_t getDefaultGDITextFlags()
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

static bool isWebFont(const LOGFONT& lf)
{
    // web-fonts have artifical names constructed to always be
    // 1. 24 characters, followed by a '\0'
    // 2. the last two characters are '=='
    return '=' == lf.lfFaceName[22] && '=' == lf.lfFaceName[23] && '\0' == lf.lfFaceName[24];
}

static int computePaintTextFlags(const LOGFONT& lf)
{
    int textFlags = 0;
    switch (lf.lfQuality) {
    case NONANTIALIASED_QUALITY:
        textFlags = 0;
        break;
    case ANTIALIASED_QUALITY:
        textFlags = SkPaint::kAntiAlias_Flag;
        break;
    case CLEARTYPE_QUALITY:
        textFlags = (SkPaint::kAntiAlias_Flag | SkPaint::kLCDRenderText_Flag);
        break;
    default:
        textFlags = getDefaultGDITextFlags();
        break;
    }

    // only allow features that SystemParametersInfo allows
    textFlags &= getDefaultGDITextFlags();

    /*
     *  FontPlatformData(...) will read our logfont, and try to honor the the lfQuality
     *  setting (computing the corresponding SkPaint flags for AA and LCD). However, it
     *  will limit the quality based on its query of SPI_GETFONTSMOOTHING. This could mean
     *  we end up drawing the text in BW, even though our lfQuality requested antialiasing.
     *
     *  Many web-fonts are so poorly hinted that they are terrible to read when drawn in BW.
     *  In these cases, we have decided to FORCE these fonts to be drawn with at least grayscale AA,
     *  even when the System (getDefaultGDITextFlags) tells us to draw only in BW.
     */
    if (isWebFont(lf) && !isRunningLayoutTest())
        textFlags |= SkPaint::kAntiAlias_Flag;
    return textFlags;
}

PassRefPtr<SkTypeface> CreateTypefaceFromHFont(HFONT hfont, int* size, int* paintTextFlags)
{
    LOGFONT info;
    GetObject(hfont, sizeof(info), &info);
    if (size) {
        int height = info.lfHeight;
        if (height < 0)
            height = -height;
        *size = height;
    }
    if (paintTextFlags)
        *paintTextFlags = computePaintTextFlags(info);
    return adoptRef(SkCreateTypefaceFromLOGFONT(info));
}

FontPlatformData::FontPlatformData(WTF::HashTableDeletedValueType)
    : m_font(0)
    , m_size(-1)
    , m_orientation(Horizontal)
    , m_scriptCache(0)
    , m_typeface(SkTypeface::RefDefault())
    , m_paintTextFlags(0)
    , m_isHashTableDeletedValue(true)
{
}

FontPlatformData::FontPlatformData()
    : m_font(0)
    , m_size(0)
    , m_orientation(Horizontal)
    , m_scriptCache(0)
    , m_typeface(SkTypeface::RefDefault())
    , m_paintTextFlags(0)
    , m_isHashTableDeletedValue(false)
{
}

FontPlatformData::FontPlatformData(HFONT font, float size, FontOrientation orientation)
    : m_font(RefCountedHFONT::create(font))
    , m_size(size)
    , m_orientation(orientation)
    , m_scriptCache(0)
    , m_typeface(CreateTypefaceFromHFont(font, 0, &m_paintTextFlags))
    , m_isHashTableDeletedValue(false)
{
}

// FIXME: this constructor is needed for SVG fonts but doesn't seem to do much
FontPlatformData::FontPlatformData(float size, bool bold, bool oblique)
    : m_font(0)
    , m_size(size)
    , m_orientation(Horizontal)
    , m_scriptCache(0)
    , m_typeface(SkTypeface::RefDefault())
    , m_paintTextFlags(0)
    , m_isHashTableDeletedValue(false)
{
}

FontPlatformData::FontPlatformData(const FontPlatformData& data)
    : m_font(data.m_font)
    , m_size(data.m_size)
    , m_orientation(data.m_orientation)
    , m_scriptCache(0)
    , m_typeface(data.m_typeface)
    , m_paintTextFlags(data.m_paintTextFlags)
    , m_isHashTableDeletedValue(false)
{
}

FontPlatformData::FontPlatformData(const FontPlatformData& data, float textSize)
    : m_font(data.m_font)
    , m_size(textSize)
    , m_orientation(data.m_orientation)
    , m_scriptCache(0)
    , m_typeface(data.m_typeface)
    , m_paintTextFlags(data.m_paintTextFlags)
    , m_isHashTableDeletedValue(false)
{
}

FontPlatformData::FontPlatformData(SkTypeface* tf, const char* family, float textSize, bool fakeBold, bool fakeItalic, FontOrientation orientation)
    : m_font(0)
    , m_size(textSize)
    , m_orientation(orientation)
    , m_scriptCache(0)
    , m_typeface(tf)
    , m_isHashTableDeletedValue(false)
{
    // FIXME: This can be removed together with m_font once the last few
    // uses of hfont() has been eliminated.
    LOGFONT logFont;
    SkLOGFONTFromTypeface(tf, &logFont);
    logFont.lfHeight = -textSize;
    HFONT hFont = CreateFontIndirect(&logFont);
    if (hFont)
        m_font = RefCountedHFONT::create(hFont);
    m_paintTextFlags = computePaintTextFlags(logFont);
}

FontPlatformData& FontPlatformData::operator=(const FontPlatformData& data)
{
    if (this != &data) {
        m_font = data.m_font;
        m_size = data.m_size;
        m_orientation = data.m_orientation;
        m_typeface = data.m_typeface;
        m_paintTextFlags = data.m_paintTextFlags;

        // The following fields will get re-computed if necessary.
        ScriptFreeCache(&m_scriptCache);
        m_scriptCache = 0;
        m_scriptFontProperties.clear();
    }
    return *this;
}

FontPlatformData::~FontPlatformData()
{
    ScriptFreeCache(&m_scriptCache);
    m_scriptCache = 0;
}

String FontPlatformData::fontFamilyName() const
{
    HWndDC dc(0);
    HGDIOBJ oldFont = static_cast<HFONT>(SelectObject(dc, hfont()));
    WCHAR name[LF_FACESIZE];
    unsigned resultLength = GetTextFace(dc, LF_FACESIZE, name);
    if (resultLength > 0)
        resultLength--; // ignore the null terminator
    SelectObject(dc, oldFont);
    return String(name, resultLength);
}

bool FontPlatformData::isFixedPitch() const
{
#if ENABLE(GDI_FONTS_ON_WINDOWS)
    // TEXTMETRICS have this. Set m_treatAsFixedPitch based off that.
    HWndDC dc(0);
    HGDIOBJ oldFont = SelectObject(dc, hfont());

    // Yes, this looks backwards, but the fixed pitch bit is actually set if the font
    // is *not* fixed pitch. Unbelievable but true.
    TEXTMETRIC textMetric = { 0 };
    if (!GetTextMetrics(dc, &textMetric)) {
        if (ensureFontLoaded(hfont())) {
            // Retry GetTextMetrics.
            // FIXME: Handle gracefully the error if this call also fails.
            // See http://crbug.com/6401.
            if (!GetTextMetrics(dc, &textMetric))
                LOG_ERROR("Unable to get the text metrics after second attempt");
        }
    }

    bool treatAsFixedPitch = !(textMetric.tmPitchAndFamily & TMPF_FIXED_PITCH);

    SelectObject(dc, oldFont);

    return treatAsFixedPitch;
#else
    return typeface() && typeface()->isFixedPitch();
#endif
}

FontPlatformData::RefCountedHFONT::~RefCountedHFONT()
{
    DeleteObject(m_hfont);
}

SCRIPT_FONTPROPERTIES* FontPlatformData::scriptFontProperties() const
{
    if (!m_scriptFontProperties) {
        m_scriptFontProperties = adoptPtr(new SCRIPT_FONTPROPERTIES);
        memset(m_scriptFontProperties.get(), 0, sizeof(SCRIPT_FONTPROPERTIES));
        m_scriptFontProperties->cBytes = sizeof(SCRIPT_FONTPROPERTIES);
        HRESULT result = ScriptGetFontProperties(0, scriptCache(), m_scriptFontProperties.get());
        if (result == E_PENDING) {
            HWndDC dc(0);
            HGDIOBJ oldFont = SelectObject(dc, hfont());
            HRESULT hr = ScriptGetFontProperties(dc, scriptCache(), m_scriptFontProperties.get());
            if (S_OK != hr) {
                if (FontPlatformData::ensureFontLoaded(hfont())) {
                    // FIXME: Handle gracefully the error if this call also fails.
                    hr = ScriptGetFontProperties(dc, scriptCache(), m_scriptFontProperties.get());
                    if (S_OK != hr) {
                        LOG_ERROR("Unable to get the font properties after second attempt");
                    }
                }
            }

            SelectObject(dc, oldFont);
        }
    }
    return m_scriptFontProperties.get();
}

#ifndef NDEBUG
String FontPlatformData::description() const
{
    return String();
}
#endif

bool FontPlatformData::ensureFontLoaded(HFONT font)
{
    WebKit::WebSandboxSupport* sandboxSupport = WebKit::Platform::current()->sandboxSupport();
    // if there is no sandbox, then we can assume the font
    // was able to be loaded successfully already
    return sandboxSupport ? sandboxSupport->ensureFontLoaded(font) : true;
}

}
