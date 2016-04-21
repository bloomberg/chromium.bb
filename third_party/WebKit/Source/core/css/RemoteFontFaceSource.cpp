// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/RemoteFontFaceSource.h"

#include "core/css/CSSCustomFontData.h"
#include "core/css/CSSFontFace.h"
#include "core/css/CSSFontSelector.h"
#include "core/dom/Document.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/page/NetworkStateNotifier.h"
#include "platform/Histogram.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/fonts/FontCache.h"
#include "platform/fonts/FontDescription.h"
#include "platform/fonts/SimpleFontData.h"
#include "wtf/CurrentTime.h"

namespace blink {

RemoteFontFaceSource::RemoteFontFaceSource(FontResource* font, CSSFontSelector* fontSelector, FontDisplay display)
    : m_font(font)
    , m_fontSelector(fontSelector)
    , m_display(display)
    , m_period(display == FontDisplaySwap ? SwapPeriod : BlockPeriod)
    , m_isInterventionTriggered(false)
{
    ThreadState::current()->registerPreFinalizer(this);
    m_font->addClient(this);

    // TODO(crbug.com/578029): Connect NQE signal for V2 mode.
    if (RuntimeEnabledFeatures::webFontsInterventionTriggerEnabled()
        || (networkStateNotifier().connectionType() == WebConnectionTypeCellular2G && display == FontDisplayAuto)) {
        m_isInterventionTriggered = true;
        m_period = SwapPeriod;
    }
}

RemoteFontFaceSource::~RemoteFontFaceSource()
{
}

void RemoteFontFaceSource::dispose()
{
    m_font->removeClient(this);
    m_font = nullptr;
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
    return m_font->isLoading();
}

bool RemoteFontFaceSource::isLoaded() const
{
    return m_font->isLoaded();
}

bool RemoteFontFaceSource::isValid() const
{
    return !m_font->errorOccurred();
}

void RemoteFontFaceSource::notifyFinished(Resource*)
{
    m_histograms.recordRemoteFont(m_font.get());
    m_histograms.fontLoaded(m_isInterventionTriggered);

    m_font->ensureCustomFontData();
    // FIXME: Provide more useful message such as OTS rejection reason.
    // See crbug.com/97467
    if (m_font->getStatus() == Resource::DecodeError && m_fontSelector->document()) {
        m_fontSelector->document()->addConsoleMessage(ConsoleMessage::create(OtherMessageSource, WarningMessageLevel, "Failed to decode downloaded font: " + m_font->url().elidedString()));
        if (m_font->otsParsingMessage().length() > 1)
            m_fontSelector->document()->addConsoleMessage(ConsoleMessage::create(OtherMessageSource, WarningMessageLevel, "OTS parsing error: " + m_font->otsParsingMessage()));
    }

    pruneTable();
    if (m_face) {
        m_fontSelector->fontFaceInvalidated();
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
        m_fontSelector->fontFaceInvalidated();
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
    if (m_fontSelector->document() && m_font->stillNeedsLoad()) {
        m_font->load(m_fontSelector->document()->fetcher());
        m_histograms.loadStarted();
    }
    m_font->startLoadLimitTimersIfNeeded();

    if (m_face)
        m_face->didBeginLoad();
}

DEFINE_TRACE(RemoteFontFaceSource)
{
    visitor->trace(m_font);
    visitor->trace(m_fontSelector);
    CSSFontFaceSource::trace(visitor);
}

void RemoteFontFaceSource::FontLoadHistograms::loadStarted()
{
    if (!m_loadStartTime)
        m_loadStartTime = currentTimeMS();
}

void RemoteFontFaceSource::FontLoadHistograms::fallbackFontPainted(DisplayPeriod period)
{
    if (period == BlockPeriod && !m_blankPaintTime)
        m_blankPaintTime = currentTimeMS();
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
    if (m_blankPaintTime <= 0)
        return;
    int duration = static_cast<int>(currentTimeMS() - m_blankPaintTime);
    DEFINE_STATIC_LOCAL(CustomCountHistogram, blankTextShownTimeHistogram, ("WebFont.BlankTextShownTime", 0, 10000, 50));
    blankTextShownTimeHistogram.count(duration);
    m_blankPaintTime = -1;
}

void RemoteFontFaceSource::FontLoadHistograms::recordRemoteFont(const FontResource* font)
{
    if (m_loadStartTime > 0 && font && !font->isLoading()) {
        int duration = static_cast<int>(currentTimeMS() - m_loadStartTime);
        recordLoadTimeHistogram(font, duration);
        m_loadStartTime = -1;

        enum { Miss, Hit, DataUrl, CacheHitEnumMax };
        int histogramValue = font->url().protocolIsData() ? DataUrl
            : font->response().wasCached() ? Hit
            : Miss;
        DEFINE_STATIC_LOCAL(EnumerationHistogram, cacheHitHistogram, ("WebFont.CacheHit", CacheHitEnumMax));
        cacheHitHistogram.count(histogramValue);

        enum { CORSFail, CORSSuccess, CORSEnumMax };
        int corsValue = font->isCORSFailed() ? CORSFail : CORSSuccess;
        DEFINE_STATIC_LOCAL(EnumerationHistogram, corsHistogram, ("WebFont.CORSSuccess", CORSEnumMax));
        corsHistogram.count(corsValue);
    }
}

void RemoteFontFaceSource::FontLoadHistograms::recordLoadTimeHistogram(const FontResource* font, int duration)
{
    if (font->errorOccurred()) {
        DEFINE_STATIC_LOCAL(CustomCountHistogram, loadErrorHistogram, ("WebFont.DownloadTime.LoadError", 0, 10000, 50));
        loadErrorHistogram.count(duration);
        return;
    }

    unsigned size = font->encodedSize();
    if (size < 10 * 1024) {
        DEFINE_STATIC_LOCAL(CustomCountHistogram, under10kHistogram, ("WebFont.DownloadTime.0.Under10KB", 0, 10000, 50));
        under10kHistogram.count(duration);
        return;
    }
    if (size < 50 * 1024) {
        DEFINE_STATIC_LOCAL(CustomCountHistogram, under50kHistogram, ("WebFont.DownloadTime.1.10KBTo50KB", 0, 10000, 50));
        under50kHistogram.count(duration);
        return;
    }
    if (size < 100 * 1024) {
        DEFINE_STATIC_LOCAL(CustomCountHistogram, under100kHistogram, ("WebFont.DownloadTime.2.50KBTo100KB", 0, 10000, 50));
        under100kHistogram.count(duration);
        return;
    }
    if (size < 1024 * 1024) {
        DEFINE_STATIC_LOCAL(CustomCountHistogram, under1mbHistogram, ("WebFont.DownloadTime.3.100KBTo1MB", 0, 10000, 50));
        under1mbHistogram.count(duration);
        return;
    }
    DEFINE_STATIC_LOCAL(CustomCountHistogram, over1mbHistogram, ("WebFont.DownloadTime.4.Over1MB", 0, 10000, 50));
    over1mbHistogram.count(duration);
}

void RemoteFontFaceSource::FontLoadHistograms::recordInterventionResult(bool triggered)
{
    // interventionResult takes 0-3 values.
    int interventionResult = 0;
    if (m_isLongLimitExceeded)
        interventionResult |= 1 << 0;
    if (triggered)
        interventionResult |= 1 << 1;
    const int boundary = 1 << 2;

    DEFINE_STATIC_LOCAL(EnumerationHistogram, interventionHistogram, ("WebFont.InterventionResult", boundary));
    interventionHistogram.count(interventionResult);
}

} // namespace blink
