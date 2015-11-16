/*
 * Copyright (C) 2015 Google Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef CachingWordShapeIterator_h
#define CachingWordShapeIterator_h

#include "platform/fonts/Font.h"
#include "platform/fonts/SimpleFontData.h"
#include "platform/fonts/shaping/CachingWordShapeIterator.h"
#include "platform/fonts/shaping/HarfBuzzShaper.h"
#include "platform/fonts/shaping/ShapeCache.h"
#include "wtf/text/CharacterNames.h"

namespace blink {

class CachingWordShapeIterator final {
    WTF_MAKE_NONCOPYABLE(CachingWordShapeIterator);
    USING_FAST_MALLOC(CachingWordShapeIterator);
public:
    CachingWordShapeIterator(ShapeCache* cache, const TextRun& run,
        const Font* font)
        : m_shapeCache(cache), m_textRun(run), m_font(font)
        , m_widthSoFar(0), m_startIndex(0)
    {
        ASSERT(font);
        const FontDescription& fontDescription = font->fontDescription();

        // Word and letter spacing can change the width of a word, as can tabs
        // as we segment solely based on on space characters.
        // If expansion is used (for justified text) the spacing between words
        // change and thus we need to shape the entire run.
        m_wordResultCachable = !fontDescription.wordSpacing()
            && !fontDescription.letterSpacing()
            && m_textRun.expansion() == 0.0f;

        // Shaping word by word is faster as each word is cached. If we cannot
        // use the cache or if the font doesn't support word by word shaping
        // fall back on shaping the entire run.
        m_shapeByWord = m_wordResultCachable && m_font->canShapeWordByWord();
    }

    bool next(RefPtr<ShapeResult>* wordResult)
    {
        if (UNLIKELY(m_textRun.allowTabs()))
            return nextForAllowTabs(wordResult);

        if (!m_shapeByWord) {
            if (m_startIndex)
                return false;
            *wordResult = shapeWord(m_textRun, m_font);
            m_startIndex = 1;
            return *wordResult;
        }

        return nextWord(wordResult);
    }

private:
    PassRefPtr<ShapeResult> shapeWord(const TextRun& wordRun, const Font* font)
    {
        ShapeCacheEntry* cacheEntry = m_wordResultCachable
            ? m_shapeCache->add(wordRun, ShapeCacheEntry())
            : nullptr;
        if (cacheEntry && cacheEntry->m_shapeResult)
            return cacheEntry->m_shapeResult;

        HarfBuzzShaper shaper(font, wordRun);
        RefPtr<ShapeResult> shapeResult = shaper.shapeResult();
        if (!shapeResult)
            return nullptr;

        if (cacheEntry)
            cacheEntry->m_shapeResult = shapeResult;

        return shapeResult.release();
    }

    bool nextWord(RefPtr<ShapeResult>* wordResult)
    {
        unsigned length = m_textRun.length();
        if (m_startIndex < length) {
            if (m_textRun[m_startIndex] == spaceCharacter
                || m_textRun[m_startIndex] == tabulationCharacter) {
                TextRun wordRun = m_textRun.subRun(m_startIndex, 1);
                *wordResult = shapeWord(wordRun, m_font);
                m_startIndex++;
                return *wordResult;
            }

            return nextUntilCharacterOrTab(wordResult, spaceCharacter);
        }
        return false;
    }

    bool nextUntilCharacterOrTab(RefPtr<ShapeResult>* wordResult, UChar delimiter)
    {
        unsigned length = m_textRun.length();
        ASSERT(m_startIndex < length);
        for (unsigned i = m_startIndex + 1; ; i++) {
            if (i == length || m_textRun[i] == delimiter
                || m_textRun[i] == tabulationCharacter) {
                TextRun wordRun = m_textRun.subRun(m_startIndex,
                    i - m_startIndex);
                m_startIndex = i;
                *wordResult = shapeWord(wordRun, m_font);
                return *wordResult;
            }
        }
    }

    bool nextForAllowTabs(RefPtr<ShapeResult>* wordResult)
    {
        unsigned length = m_textRun.length();
        if (m_startIndex >= length)
            return false;

        if (UNLIKELY(m_textRun[m_startIndex] == tabulationCharacter)) {
            for (unsigned i = m_startIndex + 1; ; i++) {
                if (i == length || m_textRun[i] != tabulationCharacter) {
                    *wordResult = ShapeResult::createForTabulationCharacters(
                        m_font, m_textRun, m_widthSoFar, i - m_startIndex);
                    m_startIndex = i;
                    break;
                }
            }
        } else if (!m_shapeByWord) {
            if (!nextUntilCharacterOrTab(wordResult, 0))
                return false;
        } else {
            if (!nextWord(wordResult))
                return false;
        }
        if (!*wordResult)
            return false;
        m_widthSoFar += (*wordResult)->width();
        return true;
    }

    ShapeCache* m_shapeCache;
    const TextRun& m_textRun;
    const Font* m_font;
    float m_widthSoFar; // Used only when allowTabs()
    unsigned m_startIndex : 30;
    unsigned m_wordResultCachable : 1;
    unsigned m_shapeByWord : 1;
};

} // namespace blink

#endif // CachingWordShapeIterator_h
