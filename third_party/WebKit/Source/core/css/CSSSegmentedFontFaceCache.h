/*
 * Copyright (C) 2007, 2008, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#ifndef CSSSegmentedFontFaceCache_h
#define CSSSegmentedFontFaceCache_h

#include "wtf/Forward.h"
#include "wtf/HashMap.h"
#include "wtf/text/StringHash.h"

namespace WebCore {

class CSSFontSelector;
class CSSSegmentedFontFace;
class FontData;
class FontDescription;
class StyleRuleFontFace;
class Settings;

class CSSSegmentedFontFaceCache {
public:
    CSSSegmentedFontFaceCache();

    // FIXME: Remove CSSFontSelector as argument. Passing CSSFontSelector here is
    // a result of egregious spaghettification in CSSFontFace/FontFaceSet.
    void addFontFaceRule(CSSFontSelector*, const StyleRuleFontFace*);
    PassRefPtr<FontData> getFontData(Settings*, const FontDescription&, const AtomicString&);
    CSSSegmentedFontFace* getFontFace(const FontDescription&, const AtomicString& family);

    unsigned version() const { return m_version; }

private:
    HashMap<String, OwnPtr<HashMap<unsigned, RefPtr<CSSSegmentedFontFace> > >, CaseFoldingHash> m_fontFaces;
    HashMap<String, OwnPtr<Vector<RefPtr<CSSSegmentedFontFace> > >, CaseFoldingHash> m_locallyInstalledFontFaces;
    HashMap<String, OwnPtr<HashMap<unsigned, RefPtr<CSSSegmentedFontFace> > >, CaseFoldingHash> m_fonts;

    // FIXME: See if this could be ditched
    // Used to compare Font instances, and the usage seems suspect.
    unsigned m_version;
};

}

#endif
