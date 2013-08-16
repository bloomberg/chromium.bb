/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
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

#ifndef CSSFontFace_h
#define CSSFontFace_h

#include "core/css/CSSFontFaceRule.h"
#include "core/css/CSSFontFaceSource.h"
#include "wtf/Forward.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/Vector.h"

namespace WebCore {

class CSSSegmentedFontFace;
class FontDescription;
class SimpleFontData;

class CSSFontFace : public RefCounted<CSSFontFace> {
public:
    static PassRefPtr<CSSFontFace> create(PassRefPtr<CSSFontFaceRule> rule, bool isLocalFallback = false) { return adoptRef(new CSSFontFace(rule, isLocalFallback)); }

    struct UnicodeRange;

    void addRange(UChar32 from, UChar32 to) { m_ranges.append(UnicodeRange(from, to)); }
    const Vector<UnicodeRange>& ranges() const { return m_ranges; }

    void setSegmentedFontFace(CSSSegmentedFontFace*);
    void clearSegmentedFontFace() { m_segmentedFontFace = 0; }

    bool isLoaded() const;
    bool isValid() const;

    void addSource(PassOwnPtr<CSSFontFaceSource>);

    void fontLoaded(CSSFontFaceSource*);

    PassRefPtr<SimpleFontData> getFontData(const FontDescription&, bool syntheticBold, bool syntheticItalic);

    struct UnicodeRange {
        UnicodeRange(UChar32 from, UChar32 to)
            : m_from(from)
            , m_to(to)
        {
        }

        UChar32 from() const { return m_from; }
        UChar32 to() const { return m_to; }

    private:
        UChar32 m_from;
        UChar32 m_to;
    };

#if ENABLE(SVG_FONTS)
    bool hasSVGFontFaceSource() const;
#endif

    enum LoadState { NotLoaded, Loading, Loaded, Error };
    LoadState loadState() const { return m_loadState; }
    void willUseFontData(const FontDescription&);

private:
    CSSFontFace(PassRefPtr<CSSFontFaceRule> rule, bool isLocalFallback)
        : m_segmentedFontFace(0)
        , m_activeSource(0)
        , m_loadState(isLocalFallback ? Loaded : NotLoaded)
        , m_rule(rule)
    {
        UNUSED_PARAM(rule);
    }
    void setLoadState(LoadState);

    Vector<UnicodeRange> m_ranges;
    CSSSegmentedFontFace* m_segmentedFontFace;
    Vector<OwnPtr<CSSFontFaceSource> > m_sources;
    CSSFontFaceSource* m_activeSource;
    LoadState m_loadState;
    RefPtr<CSSFontFaceRule> m_rule;
};

}

#endif
