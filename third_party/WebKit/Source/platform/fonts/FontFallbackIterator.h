// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FontFallbackIterator_h
#define FontFallbackIterator_h

#include "platform/fonts/FontDataRange.h"
#include "wtf/HashMap.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/RefPtr.h"
#include "wtf/Vector.h"
#include "wtf/text/Unicode.h"

namespace blink {

using namespace WTF;

class FontDescription;
class FontFallbackList;
class SimpleFontData;
struct FontDataRange;
class FontFamily;

class FontFallbackIterator : public RefCounted<FontFallbackIterator> {
    WTF_MAKE_NONCOPYABLE(FontFallbackIterator);

public:
    static PassRefPtr<FontFallbackIterator> create(const FontDescription&, PassRefPtr<FontFallbackList>);

    // Returns whether a list of all remaining characters to be shaped is
    // needed.  Needed by the FontfallbackIterator in order to check whether a
    // font from a segmented range should be loaded.
    bool needsHintList() const;

    bool hasNext() const { return m_fallbackStage != OutOfLuck; };

    // Some system fallback APIs (Windows, Android) require a character, or a
    // portion of the string to be passed.  On Mac and Linux, we get a list of
    // fonts without passing in characters.
    const FontDataRange next(const Vector<UChar32>& hintList);

private:
    FontFallbackIterator(const FontDescription&, PassRefPtr<FontFallbackList>);
    bool rangeContributesForHint(const Vector<UChar32> hintList, const FontDataRange&);
    bool alreadyLoadingRangeForHintChar(UChar32 hintChar);
    void willUseRange(const AtomicString& family, const FontDataRange&);

    const PassRefPtr<SimpleFontData> uniqueSystemFontForHint(UChar32 hint);

    const FontDescription& m_fontDescription;
    RefPtr<FontFallbackList> m_fontFallbackList;
    int m_currentFontDataIndex;
    unsigned m_segmentedIndex;

    enum FallbackStage {
        FontGroupFonts,
        SegmentedFace,
        PreferencesFonts,
        SystemFonts,
        OutOfLuck
    };

    FallbackStage m_fallbackStage;
    HashMap<UChar32, RefPtr<SimpleFontData>> m_visitedSystemFonts;
    Vector<FontDataRange> m_loadingCustomFontForRanges;
};

} // namespace blink

#endif
