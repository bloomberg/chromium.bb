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

#ifndef CSSFontSelector_h
#define CSSFontSelector_h

#include "core/css/CSSSegmentedFontFaceCache.h"
#include "core/fetch/ResourcePtr.h"
#include "core/platform/Timer.h"
#include "core/platform/graphics/FontSelector.h"
#include "wtf/Forward.h"
#include "wtf/HashMap.h"
#include "wtf/HashSet.h"

namespace WebCore {

class CSSFontFace;
class CSSFontFaceRule;
class CSSSegmentedFontFace;
class FontResource;
class Document;
class FontDescription;
class StyleRuleFontFace;

class FontLoader {
public:
    explicit FontLoader(ResourceFetcher*);

    void addFontToBeginLoading(FontResource*);

    void clearResourceFetcher();

private:
    void beginLoadTimerFired(Timer<FontLoader>*);

    Timer<FontLoader> m_beginLoadingTimer;
    Vector<ResourcePtr<FontResource> > m_fontsToBeginLoading;
    ResourceFetcher* m_resourceFetcher;
};

class CSSFontSelector : public FontSelector {
public:
    static PassRefPtr<CSSFontSelector> create(Document* document)
    {
        return adoptRef(new CSSFontSelector(document));
    }
    virtual ~CSSFontSelector();

    virtual unsigned version() const OVERRIDE { return m_cssSegmentedFontFaceCache.version(); }

    virtual PassRefPtr<FontData> getFontData(const FontDescription&, const AtomicString&);
    CSSSegmentedFontFace* getFontFace(const FontDescription&, const AtomicString& family);
    virtual void willUseFontData(const FontDescription&, const AtomicString& family) OVERRIDE;

    void clearDocument();

    void addFontFaceRule(const StyleRuleFontFace*);

    void fontLoaded();
    virtual void fontCacheInvalidated();

    virtual void registerForInvalidationCallbacks(FontSelectorClient*);
    virtual void unregisterForInvalidationCallbacks(FontSelectorClient*);

    Document* document() const { return m_document; }

    void beginLoadingFontSoon(FontResource*);

private:
    explicit CSSFontSelector(Document*);

    void dispatchInvalidationCallbacks();

    Document* m_document;
    // FIXME: Move to Document or StyleEngine.
    CSSSegmentedFontFaceCache m_cssSegmentedFontFaceCache;
    HashSet<FontSelectorClient*> m_clients;

    FontLoader m_fontLoader;
};

} // namespace WebCore

#endif // CSSFontSelector_h
