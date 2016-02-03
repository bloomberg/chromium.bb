// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/RemoteFontFaceSource.h"

#include "core/css/CSSCustomFontData.h"
#include "core/css/CSSFontFace.h"
#include "core/css/FontLoader.h"
#include "core/page/NetworkStateNotifier.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/fonts/FontCache.h"
#include "platform/fonts/FontDescription.h"
#include "platform/fonts/SimpleFontData.h"
#include "public/platform/Platform.h"
#include "wtf/CurrentTime.h"

namespace blink {

RemoteFontFaceSource::RemoteFontFaceSource(FontResource* font, PassRefPtrWillBeRawPtr<FontLoader> fontLoader, FontDisplay display)
    : m_font(font)
    , m_fontLoader(fontLoader)
    , m_display(display)
    , m_period(display == FontDisplaySwap ? SwapPeriod : BlockPeriod)
    , m_isInterventionTriggered(false)
{
#if ENABLE(OILPAN)
    ThreadState::current()->registerPreFinalizer(this);
#endif
    m_font->addClient(this);

    if (RuntimeEnabledFeatures::webFontsInterventionEnabled()) {
        // TODO(crbug.com/515343): Consider to use better signals.
        if (networkStateNotifier().connectionType() == WebConnectionTypeCellular2G && display == FontDisplayAuto) {

            m_isInterventionTriggered = true;
            m_period = SwapPeriod;
        }
    }
}

RemoteFontFaceSource::~RemoteFontFaceSource()
{
#if !ENABLE(OILPAN)
    dispose();
#endif
}

void RemoteFontFaceSource::dispose()
{
    m_font->removeClient(this);
    pruneTable();
}

void RemoteFontFaceSource::pruneTable()
{
    if (m_fontDataTable.isEmpty())
        return;

    for (const auto& item : m_fontDataTable) {
        SimpleFontData* fontData = item.value.get();
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
    // We may send duplicated reports when multiple CSSFontFaceSource are
    // registered at this FontResource. Associating the same URL to different
    // font-family causes the case, but we treat them as indivisual resources.
    m_histograms.loadStarted();
}

void RemoteFontFaceSource::fontLoaded(FontResource*)
{
    m_histograms.recordRemoteFont(m_font.get());
    m_histograms.fontLoaded(m_isInterventionTriggered);

    m_font->ensureCustomFontData();
    if (m_font->status() == Resource::DecodeError)
        m_fontLoader->didFailToDecode(m_font.get());

    pruneTable();
    if (m_face) {
        m_fontLoader->fontFaceInvalidated();
        m_face->fontLoaded(this);
    }
    // Should not do anything after this line since the m_face->fontLoaded()
    // above may trigger deleting this object.
}

void RemoteFontFaceSource::fontLoadShortLimitExceeded(FontResource*)
{
    if (m_display == FontDisplayFallback)
        switchToSwapPeriod();
    else if (m_display == FontDisplayOptional)
        switchToFailurePeriod();
}

void RemoteFontFaceSource::fontLoadLongLimitExceeded(FontResource*)
{
    if (m_display == FontDisplayBlock || (!m_isInterventionTriggered && m_display == FontDisplayAuto))
        switchToSwapPeriod();
    else if (m_display == FontDisplayFallback)
        switchToFailurePeriod();

    m_histograms.longLimitExceeded(m_isInterventionTriggered);
}

void RemoteFontFaceSource::switchToSwapPeriod()
{
    ASSERT(m_period == BlockPeriod);
    m_period = SwapPeriod;

    pruneTable();
    if (m_face) {
        m_fontLoader->fontFaceInvalidated();
        m_face->didBecomeVisibleFallback(this);
    }

    m_histograms.recordFallbackTime(m_font.get());
}

void RemoteFontFaceSource::switchToFailurePeriod()
{
    if (m_period == BlockPeriod)
        switchToSwapPeriod();
    ASSERT(m_period == SwapPeriod);
    m_period = FailurePeriod;
}

PassRefPtr<SimpleFontData> RemoteFontFaceSource::createFontData(const FontDescription& fontDescription)
{
    if (!isLoaded())
        return createLoadingFallbackFontData(fontDescription);

    if (!m_font->ensureCustomFontData() || m_period == FailurePeriod)
        return nullptr;

    m_histograms.recordFallbackTime(m_font.get());

    return SimpleFontData::create(
        m_font->platformDataFromCustomData(fontDescription.effectiveFontSize(),
            fontDescription.isSyntheticBold(), fontDescription.isSyntheticItalic(),
            fontDescription.orientation()), CustomFontData::create());
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
    RefPtr<CSSCustomFontData> cssFontData = CSSCustomFontData::create(this, m_period == BlockPeriod ? CSSCustomFontData::InvisibleFallback : CSSCustomFontData::VisibleFallback);
    return SimpleFontData::create(temporaryFont->platformData(), cssFontData);
}

void RemoteFontFaceSource::beginLoadIfNeeded()
{
    if (m_font->stillNeedsLoad())
        m_fontLoader->addFontToBeginLoading(m_font.get());

    if (m_face)
        m_face->didBeginLoad();
}

DEFINE_TRACE(RemoteFontFaceSource)
{
    visitor->trace(m_fontLoader);
    CSSFontFaceSource::trace(visitor);
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

void RemoteFontFaceSource::FontLoadHistograms::fontLoaded(bool isInterventionTriggered)
{
    if (!m_isLongLimitExceeded)
        recordInterventionResult(isInterventionTriggered);
}

void RemoteFontFaceSource::FontLoadHistograms::longLimitExceeded(bool isInterventionTriggered)
{
    m_isLongLimitExceeded = true;
    recordInterventionResult(isInterventionTriggered);
}

void RemoteFontFaceSource::FontLoadHistograms::recordFallbackTime(const FontResource* font)
{
    if (m_fallbackPaintTime <= 0)
        return;
    int duration = static_cast<int>(currentTimeMS() - m_fallbackPaintTime);
    Platform::current()->histogramCustomCounts("WebFont.BlankTextShownTime", duration, 0, 10000, 50);
    m_fallbackPaintTime = -1;
}

void RemoteFontFaceSource::FontLoadHistograms::recordRemoteFont(const FontResource* font)
{
    if (m_loadStartTime > 0 && font && !font->isLoading()) {
        int duration = static_cast<int>(currentTimeMS() - m_loadStartTime);
        Platform::current()->histogramCustomCounts(histogramName(font), duration, 0, 10000, 50);
        m_loadStartTime = -1;

        enum { Miss, Hit, DataUrl, CacheHitEnumMax };
        int histogramValue = font->url().protocolIsData() ? DataUrl
            : font->response().wasCached() ? Hit
            : Miss;
        Platform::current()->histogramEnumeration("WebFont.CacheHit", histogramValue, CacheHitEnumMax);

        enum { CORSFail, CORSSuccess, CORSEnumMax };
        int corsValue = font->isCORSFailed() ? CORSFail : CORSSuccess;
        Platform::current()->histogramEnumeration("WebFont.CORSSuccess", corsValue, CORSEnumMax);
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

void RemoteFontFaceSource::FontLoadHistograms::recordInterventionResult(bool triggered)
{
    if (!RuntimeEnabledFeatures::webFontsInterventionEnabled())
        return;
    // interventionResult takes 0-3 values.
    int interventionResult = 0;
    if (m_isLongLimitExceeded)
        interventionResult |= 1 << 0;
    if (triggered)
        interventionResult |= 1 << 1;
    const int boundary = 1 << 2;
    Platform::current()->histogramEnumeration("WebFont.InterventionResult", interventionResult, boundary);
}

} // namespace blink
