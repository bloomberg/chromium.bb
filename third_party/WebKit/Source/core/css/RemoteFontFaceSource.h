// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RemoteFontFaceSource_h
#define RemoteFontFaceSource_h

#include "core/css/CSSFontFaceSource.h"
#include "core/fetch/FontResource.h"
#include "wtf/Allocator.h"

namespace blink {

class FontLoader;

enum FontDisplay {
    FontDisplayAuto,
    FontDisplayBlock,
    FontDisplaySwap,
    FontDisplayFallback,
    FontDisplayOptional,
    FontDisplayEnumMax
};

class RemoteFontFaceSource final : public CSSFontFaceSource, public FontResourceClient {
    WILL_BE_USING_PRE_FINALIZER(RemoteFontFaceSource, dispose);
public:
    enum DisplayPeriod { BlockPeriod, SwapPeriod, FailurePeriod };

    explicit RemoteFontFaceSource(PassRefPtrWillBeRawPtr<FontResource>, PassRefPtrWillBeRawPtr<FontLoader>, FontDisplay);
    ~RemoteFontFaceSource() override;
    void dispose();

    bool isLoading() const override;
    bool isLoaded() const override;
    bool isValid() const override;
    DisplayPeriod displayPeriod() const { return m_period; }

    void beginLoadIfNeeded() override;

    void didStartFontLoad(FontResource*) override;
    void fontLoaded(FontResource*) override;
    void fontLoadShortLimitExceeded(FontResource*) override;
    void fontLoadLongLimitExceeded(FontResource*) override;
    String debugName() const override { return "RemoteFontFaceSource"; }

    // For UMA reporting
    bool hadBlankText() override { return m_histograms.hadBlankText(); }
    void paintRequested() { m_histograms.fallbackFontPainted(); }

    DECLARE_VIRTUAL_TRACE();

protected:
    PassRefPtr<SimpleFontData> createFontData(const FontDescription&) override;
    PassRefPtr<SimpleFontData> createLoadingFallbackFontData(const FontDescription&);
    void pruneTable();

private:
    class FontLoadHistograms {
        DISALLOW_NEW();
    public:
        FontLoadHistograms() : m_loadStartTime(0), m_fallbackPaintTime(0), m_isLongLimitExceeded(false) { }
        void loadStarted();
        void fallbackFontPainted();
        void fontLoaded(bool isInterventionTriggered);
        void longLimitExceeded(bool isInterventionTriggered);
        void recordFallbackTime(const FontResource*);
        void recordRemoteFont(const FontResource*);
        bool hadBlankText() { return m_fallbackPaintTime; }
    private:
        void recordLoadTimeHistogram(const FontResource*, int duration);
        void recordInterventionResult(bool triggered);
        double m_loadStartTime;
        double m_fallbackPaintTime;
        bool m_isLongLimitExceeded;
    };

    void switchToSwapPeriod();
    void switchToFailurePeriod();

    RefPtrWillBeMember<FontResource> m_font;
    RefPtrWillBeMember<FontLoader> m_fontLoader;
    const FontDisplay m_display;
    DisplayPeriod m_period;
    FontLoadHistograms m_histograms;
    bool m_isInterventionTriggered;
};

} // namespace blink

#endif
