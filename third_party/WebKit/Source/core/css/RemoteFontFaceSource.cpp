// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/css/RemoteFontFaceSource.h"

#include "FetchInitiatorTypeNames.h"
#include "core/css/CSSCustomFontData.h"
#include "core/css/CSSFontFace.h"
#include "core/css/CSSFontSelector.h"
#include "core/fetch/ResourceFetcher.h"
#include "platform/fonts/FontCache.h"
#include "platform/fonts/FontDescription.h"
#include "platform/fonts/SimpleFontData.h"
#include "public/platform/Platform.h"
#include "wtf/CurrentTime.h"

namespace WebCore {

RemoteFontFaceSource::RemoteFontFaceSource(FontResource* font)
    : m_font(font)
{
    m_font->addClient(this);
}

RemoteFontFaceSource::~RemoteFontFaceSource()
{
    m_font->removeClient(this);
    pruneTable();
}

void RemoteFontFaceSource::pruneTable()
{
    if (m_fontDataTable.isEmpty())
        return;

    for (FontDataTable::iterator it = m_fontDataTable.begin(); it != m_fontDataTable.end(); ++it) {
        SimpleFontData* fontData = it->value.get();
        if (fontData && fontData->customFontData())
            fontData->customFontData()->clearFontFaceSource();
    }
    m_fontDataTable.clear();
}

bool RemoteFontFaceSource::isLoading() const
{
    return !m_font->stillNeedsLoad() && !m_font->isLoaded();
}

bool RemoteFontFaceSource::isLoaded() const
{
    return m_font->isLoaded();
}

bool RemoteFontFaceSource::isValid() const
{
    return !m_font->errorOccurred();
}

void RemoteFontFaceSource::didStartFontLoad(FontResource*)
{
    // Avoid duplicated reports when multiple CSSFontFaceSource are registered
    // at this FontResource.
    if (!m_fontDataTable.isEmpty())
        m_histograms.loadStarted();
}

void RemoteFontFaceSource::fontLoaded(FontResource*)
{
    if (!m_fontDataTable.isEmpty())
        m_histograms.recordRemoteFont(m_font.get());

    pruneTable();
    if (m_face)
        m_face->fontLoaded(this);
}

void RemoteFontFaceSource::fontLoadWaitLimitExceeded(FontResource*)
{
    pruneTable();
    if (m_face)
        m_face->fontLoadWaitLimitExceeded(this);

    m_histograms.recordFallbackTime(m_font.get());
}

void RemoteFontFaceSource::corsFailed(FontResource*)
{
    pruneTable();

    if (m_face) {
        Document* document = m_face->fontSelector() ? m_face->fontSelector()->document() : 0;
        if (document) {
            FetchRequest request(ResourceRequest(m_font->url()), FetchInitiatorTypeNames::css);
            m_font->removeClient(this);
            m_font = document->fetcher()->fetchFont(request);
            m_font->addClient(this);
            m_face->fontSelector()->beginLoadingFontSoon(m_font.get());
        } else {
            m_face->fontLoaded(this);
        }
    }
}

PassRefPtr<SimpleFontData> RemoteFontFaceSource::createFontData(const FontDescription& fontDescription)
{
    if (!isLoaded())
        return createLoadingFallbackFontData(fontDescription);

    // Create new FontPlatformData from our CGFontRef, point size and ATSFontRef.
    if (!m_font->ensureCustomFontData())
        return nullptr;

    m_histograms.recordFallbackTime(m_font.get());

    return SimpleFontData::create(
        m_font->platformDataFromCustomData(fontDescription.effectiveFontSize(),
            fontDescription.isSyntheticBold(), fontDescription.isSyntheticItalic(),
            fontDescription.orientation(), fontDescription.widthVariant()), CustomFontData::create());
}

PassRefPtr<SimpleFontData> RemoteFontFaceSource::createLoadingFallbackFontData(const FontDescription& fontDescription)
{
    // This temporary font is not retained and should not be returned.
    FontCachePurgePreventer fontCachePurgePreventer;
    SimpleFontData* temporaryFont = FontCache::fontCache()->getNonRetainedLastResortFallbackFont(fontDescription);
    if (!temporaryFont) {
        ASSERT_NOT_REACHED();
        return nullptr;
    }
    RefPtr<CSSCustomFontData> cssFontData = CSSCustomFontData::create(this, m_font->exceedsFontLoadWaitLimit() ? CSSCustomFontData::VisibleFallback : CSSCustomFontData::InvisibleFallback);
    return SimpleFontData::create(temporaryFont->platformData(), cssFontData);
}

void RemoteFontFaceSource::beginLoadIfNeeded()
{
    if (m_face)
        m_face->beginLoadIfNeeded(this);
}

bool RemoteFontFaceSource::ensureFontData()
{
    return m_font->ensureCustomFontData();
}

void RemoteFontFaceSource::FontLoadHistograms::loadStarted()
{
    if (!m_loadStartTime)
        m_loadStartTime = currentTimeMS();
}

void RemoteFontFaceSource::FontLoadHistograms::fallbackFontPainted()
{
    if (!m_fallbackPaintTime)
        m_fallbackPaintTime = currentTimeMS();
}

void RemoteFontFaceSource::FontLoadHistograms::recordFallbackTime(const FontResource* font)
{
    if (m_fallbackPaintTime <= 0)
        return;
    int duration = static_cast<int>(currentTimeMS() - m_fallbackPaintTime);
    blink::Platform::current()->histogramCustomCounts("WebFont.BlankTextShownTime", duration, 0, 10000, 50);
    m_fallbackPaintTime = -1;
}

void RemoteFontFaceSource::FontLoadHistograms::recordRemoteFont(const FontResource* font)
{
    if (m_loadStartTime > 0 && font && !font->isLoading()) {
        int duration = static_cast<int>(currentTimeMS() - m_loadStartTime);
        blink::Platform::current()->histogramCustomCounts(histogramName(font), duration, 0, 10000, 50);
        m_loadStartTime = -1;

        enum { Miss, Hit, DataUrl, CacheHitEnumMax };
        int histogramValue = font->url().protocolIsData() ? DataUrl
            : font->response().wasCached() ? Hit
            : Miss;
        blink::Platform::current()->histogramEnumeration("WebFont.CacheHit", histogramValue, CacheHitEnumMax);

        if (!font->errorOccurred()) {
            enum { CORSFail, CORSSuccess, CORSEnumMax };
            int corsValue = font->options().corsEnabled == IsCORSEnabled ? CORSSuccess : CORSFail;
            blink::Platform::current()->histogramEnumeration("WebFont.CORSSuccess", corsValue, CORSEnumMax);
        }
    }
}

const char* RemoteFontFaceSource::FontLoadHistograms::histogramName(const FontResource* font)
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

} // namespace WebCore
