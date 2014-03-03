/*
 * Copyright (C) 2007, 2008, 2011 Apple Inc. All rights reserved.
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

#ifndef CSSFontFaceSource_h
#define CSSFontFaceSource_h

#include "core/fetch/FontResource.h"
#include "core/fetch/ResourcePtr.h"
#include "platform/Timer.h"
#include "wtf/HashMap.h"
#include "wtf/text/AtomicString.h"

namespace WebCore {

class FontResource;
class CSSFontFace;
class FontDescription;
class SimpleFontData;
#if ENABLE(SVG_FONTS)
class SVGFontElement;
class SVGFontFaceElement;
#endif


class CSSFontFaceSource FINAL : public FontResourceClient {
public:
    CSSFontFaceSource(const String&, FontResource* = 0);
    virtual ~CSSFontFaceSource();

    bool isLocal() const;
    bool isLoading() const;
    bool isLoaded() const;
    bool isValid() const;

    FontResource* resource() { return m_font.get(); }
    void setFontFace(CSSFontFace* face) { m_face = face; }

    virtual void didStartFontLoad(FontResource*) OVERRIDE;
    virtual void fontLoaded(FontResource*) OVERRIDE;
    virtual void fontLoadWaitLimitExceeded(FontResource*) OVERRIDE;

    PassRefPtr<SimpleFontData> getFontData(const FontDescription&);

#if ENABLE(SVG_FONTS)
    SVGFontFaceElement* svgFontFaceElement() const;
    void setSVGFontFaceElement(PassRefPtr<SVGFontFaceElement>);
    bool isSVGFontFaceSource() const;
    void setHasExternalSVGFont(bool value) { m_hasExternalSVGFont = value; }
#endif

    bool ensureFontData();
    bool isLocalFontAvailable(const FontDescription&);
    void beginLoadIfNeeded();

private:
    typedef HashMap<unsigned, RefPtr<SimpleFontData> > FontDataTable; // The hash key is composed of size synthetic styles.

    class FontLoadHistograms {
    public:
        FontLoadHistograms() : m_loadStartTime(0) { }
        void loadStarted();
        void recordLocalFont(bool loadSuccess);
        void recordRemoteFont(const FontResource*);
    private:
        const char* histogramName(const FontResource*);
        double m_loadStartTime;
    };

    void pruneTable();
    void startLoadingTimerFired(Timer<CSSFontFaceSource>*);

    AtomicString m_string; // URI for remote, built-in font name for local.
    ResourcePtr<FontResource> m_font; // For remote fonts, a pointer to our cached resource.
    CSSFontFace* m_face; // Our owning font face.
    FontDataTable m_fontDataTable;
    FontLoadHistograms m_histograms;

#if ENABLE(SVG_FONTS)
    RefPtr<SVGFontFaceElement> m_svgFontFaceElement;
    RefPtr<SVGFontElement> m_externalSVGFontElement;
    bool m_hasExternalSVGFont;
#endif
};

}

#endif
