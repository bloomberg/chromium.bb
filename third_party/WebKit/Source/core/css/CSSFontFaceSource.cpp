/*
 * Copyright (C) 2007, 2008, 2010, 2011 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/css/CSSFontFaceSource.h"

#include "core/css/CSSFontFace.h"
#include "core/css/CSSFontSelector.h"
#include "core/fetch/FontResource.h"
#include "core/platform/HistogramSupport.h"
#include "core/platform/graphics/FontCache.h"
#include "core/platform/graphics/FontDescription.h"
#include "core/platform/graphics/SimpleFontData.h"
#include "wtf/CurrentTime.h"

#if ENABLE(SVG_FONTS)
#include "SVGNames.h"
#include "core/svg/SVGFontData.h"
#include "core/svg/SVGFontElement.h"
#include "core/svg/SVGFontFaceElement.h"
#endif

namespace WebCore {

CSSFontFaceSource::CSSFontFaceSource(const String& str, FontResource* font)
    : m_string(str)
    , m_font(font)
    , m_face(0)
#if ENABLE(SVG_FONTS)
    , m_hasExternalSVGFont(false)
#endif
{
    if (m_font)
        m_font->addClient(this);
}

CSSFontFaceSource::~CSSFontFaceSource()
{
    if (m_font)
        m_font->removeClient(this);
    pruneTable();
}

void CSSFontFaceSource::pruneTable()
{
    if (m_fontDataTable.isEmpty())
        return;

    for (FontDataTable::iterator it = m_fontDataTable.begin(); it != m_fontDataTable.end(); ++it) {
        if (SimpleFontData* fontData = it->value.get())
            fontData->clearCSSFontFaceSource();
    }
    m_fontDataTable.clear();
}

bool CSSFontFaceSource::isLocal() const
{
    if (m_font)
        return false;
#if ENABLE(SVG_FONTS)
    if (m_svgFontFaceElement)
        return false;
#endif
    return true;
}

bool CSSFontFaceSource::isLoading() const
{
    if (m_font)
        return !m_font->stillNeedsLoad() && !m_font->isLoaded();
    return false;
}

bool CSSFontFaceSource::isLoaded() const
{
    if (m_font)
        return m_font->isLoaded();
    return true;
}

bool CSSFontFaceSource::isValid() const
{
    if (m_font)
        return !m_font->errorOccurred();
    return true;
}

void CSSFontFaceSource::didStartFontLoad(FontResource*)
{
    // Avoid duplicated reports when multiple CSSFontFaceSource are registered
    // at this FontResource.
    if (!m_fontDataTable.isEmpty())
        m_histograms.loadStarted();
}

void CSSFontFaceSource::fontLoaded(FontResource*)
{
    if (!m_fontDataTable.isEmpty())
        m_histograms.recordRemoteFont(m_font.get());

    pruneTable();
    if (m_face)
        m_face->fontLoaded(this);
}

PassRefPtr<SimpleFontData> CSSFontFaceSource::getFontData(const FontDescription& fontDescription, bool syntheticBold, bool syntheticItalic, CSSFontSelector* fontSelector)
{
    // If the font hasn't loaded or an error occurred, then we've got nothing.
    if (!isValid())
        return 0;

    if (isLocal()) {
        // We're local. Just return a SimpleFontData from the normal cache.
        // We don't want to check alternate font family names here, so pass true as the checkingAlternateName parameter.
        RefPtr<SimpleFontData> fontData = fontCache()->getFontResourceData(fontDescription, m_string, true);
        m_histograms.recordLocalFont(fontData);
        return fontData;
    }

    // See if we have a mapping in our FontData cache.
    unsigned hashKey = (fontDescription.computedPixelSize() + 1) << 5 | fontDescription.widthVariant() << 3
                       | (fontDescription.orientation() == Vertical ? 4 : 0) | (syntheticBold ? 2 : 0) | (syntheticItalic ? 1 : 0);

    RefPtr<SimpleFontData>& fontData = m_fontDataTable.add(hashKey, 0).iterator->value;
    if (fontData)
        return fontData; // No release, because fontData is a reference to a RefPtr that is held in the m_fontDataTable.

    // If we are still loading, then we let the system pick a font.
    if (isLoaded()) {
        if (m_font) {
#if ENABLE(SVG_FONTS)
            if (m_hasExternalSVGFont) {
                // For SVG fonts parse the external SVG document, and extract the <font> element.
                if (!m_font->ensureSVGFontData())
                    return 0;

                if (!m_externalSVGFontElement) {
                    String fragmentIdentifier;
                    size_t start = m_string.find('#');
                    if (start != notFound)
                        fragmentIdentifier = m_string.string().substring(start + 1);
                    m_externalSVGFontElement = m_font->getSVGFontById(fragmentIdentifier);
                }

                if (!m_externalSVGFontElement)
                    return 0;

                SVGFontFaceElement* fontFaceElement = 0;

                // Select first <font-face> child
                for (Node* fontChild = m_externalSVGFontElement->firstChild(); fontChild; fontChild = fontChild->nextSibling()) {
                    if (fontChild->hasTagName(SVGNames::font_faceTag)) {
                        fontFaceElement = toSVGFontFaceElement(fontChild);
                        break;
                    }
                }

                if (fontFaceElement) {
                    if (!m_svgFontFaceElement) {
                        // We're created using a CSS @font-face rule, that means we're not associated with a SVGFontFaceElement.
                        // Use the imported <font-face> tag as referencing font-face element for these cases.
                        m_svgFontFaceElement = fontFaceElement;
                    }

                    fontData = SimpleFontData::create(SVGFontData::create(fontFaceElement), fontDescription.computedPixelSize(), syntheticBold, syntheticItalic);
                }
            } else
#endif
            {
                // Create new FontPlatformData from our CGFontRef, point size and ATSFontRef.
                if (!m_font->ensureCustomFontData())
                    return 0;

                fontData = SimpleFontData::create(m_font->platformDataFromCustomData(fontDescription.computedPixelSize(), syntheticBold, syntheticItalic,
                    fontDescription.orientation(), fontDescription.widthVariant()), true, false);
            }
        } else {
#if ENABLE(SVG_FONTS)
            // In-Document SVG Fonts
            if (m_svgFontFaceElement)
                fontData = SimpleFontData::create(SVGFontData::create(m_svgFontFaceElement.get()), fontDescription.computedPixelSize(), syntheticBold, syntheticItalic);
#endif
        }
    } else {
        // This temporary font is not retained and should not be returned.
        FontCachePurgePreventer fontCachePurgePreventer;
        SimpleFontData* temporaryFont = fontCache()->getNonRetainedLastResortFallbackFont(fontDescription);
        fontData = SimpleFontData::create(temporaryFont->platformData(), true, true);
        fontData->setCSSFontFaceSource(this);
    }

    return fontData; // No release, because fontData is a reference to a RefPtr that is held in the m_fontDataTable.
}

#if ENABLE(SVG_FONTS)
SVGFontFaceElement* CSSFontFaceSource::svgFontFaceElement() const
{
    return m_svgFontFaceElement.get();
}

void CSSFontFaceSource::setSVGFontFaceElement(PassRefPtr<SVGFontFaceElement> element)
{
    m_svgFontFaceElement = element;
}

bool CSSFontFaceSource::isSVGFontFaceSource() const
{
    return m_svgFontFaceElement || m_hasExternalSVGFont;
}
#endif

bool CSSFontFaceSource::isDecodeError() const
{
    if (m_font)
        return m_font->status() == Resource::DecodeError;
    return false;
}

bool CSSFontFaceSource::ensureFontData()
{
    if (!m_font)
        return false;
#if ENABLE(SVG_FONTS)
    if (m_hasExternalSVGFont)
        return m_font->ensureSVGFontData();
#endif
    return m_font->ensureCustomFontData();
}

bool CSSFontFaceSource::isLocalFontAvailable(const FontDescription& fontDescription)
{
    if (!isLocal())
        return false;
    return fontCache()->isPlatformFontAvailable(fontDescription, m_string, true);
}

void CSSFontFaceSource::willUseFontData()
{
    if (m_font)
        m_font->willUseFontData();
}

void CSSFontFaceSource::beginLoadingFontSoon()
{
    ASSERT(m_face);
    ASSERT(m_font);
    m_face->beginLoadingFontSoon(m_font.get());
}

void CSSFontFaceSource::FontLoadHistograms::loadStarted()
{
    if (!m_loadStartTime)
        m_loadStartTime = currentTimeMS();
}

void CSSFontFaceSource::FontLoadHistograms::recordLocalFont(bool loadSuccess)
{
    if (!m_loadStartTime) {
        HistogramSupport::histogramEnumeration("WebFont.LocalFontUsed", loadSuccess ? 1 : 0, 2);
        m_loadStartTime = -1; // Do not count this font again.
    }
}

void CSSFontFaceSource::FontLoadHistograms::recordRemoteFont(const FontResource* font)
{
    if (m_loadStartTime > 0 && font && !font->isLoading()) {
        int duration = static_cast<int>(currentTimeMS() - m_loadStartTime);
        HistogramSupport::histogramCustomCounts(histogramName(font), duration, 0, 10000, 50);
        m_loadStartTime = -1;
    }
}

const char* CSSFontFaceSource::FontLoadHistograms::histogramName(const FontResource* font)
{
    if (font->errorOccurred())
        return "WebFont.DownloadTime.LoadError";

    unsigned size = font->encodedSize();
    if (size < 10 * 1024)
        return "WebFont.DownloadTime.0.Under10KB";
    if (size < 50 * 1024)
        return "WebFont.DownloadTime.1.10KBTo50KB";
    if (size < 100 * 1024)
        return "WebFont.DownloadTime.2.50KBTo100KB";
    if (size < 1024 * 1024)
        return "WebFont.DownloadTime.3.100KBTo1MB";
    return "WebFont.DownloadTime.4.Over1MB";
}

}
