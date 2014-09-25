/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef Character_h
#define Character_h

#include "platform/PlatformExport.h"
#include "platform/text/TextDirection.h"
#include "platform/text/TextPath.h"
#include "wtf/HashSet.h"
#include "wtf/text/WTFString.h"
#include "wtf/unicode/CharacterNames.h"

namespace blink {

class PLATFORM_EXPORT Character {
public:
    static CodePath characterRangeCodePath(const LChar*, unsigned) { return SimplePath; }
    static CodePath characterRangeCodePath(const UChar*, unsigned len);

    static bool isCJKIdeograph(UChar32);
    static bool isCJKIdeographOrSymbol(UChar32);

    static unsigned expansionOpportunityCount(const LChar*, size_t length, TextDirection, bool& isAfterExpansion);
    static unsigned expansionOpportunityCount(const UChar*, size_t length, TextDirection, bool& isAfterExpansion);

    static bool treatAsSpace(UChar c) { return c == space || c == characterTabulation || c == newlineCharacter || c == noBreakSpace; }
    static bool treatAsZeroWidthSpace(UChar c) { return treatAsZeroWidthSpaceInComplexScript(c) || c == 0x200c || c == 0x200d; }
    static bool treatAsZeroWidthSpaceInComplexScript(UChar c) { return c < 0x20 || (c >= 0x7F && c < 0xA0) || c == softHyphen || c == zeroWidthSpace || (c >= 0x200e && c <= 0x200f) || (c >= 0x202a && c <= 0x202e) || c == zeroWidthNoBreakSpace || c == objectReplacementCharacter; }
    static bool canReceiveTextEmphasis(UChar32);

    static inline UChar normalizeSpaces(UChar character)
    {
        if (treatAsSpace(character))
            return space;

        if (treatAsZeroWidthSpace(character))
            return zeroWidthSpace;

        return character;
    }

    static inline bool isNormalizedCanvasSpaceCharacter(UChar c)
    {
        // According to specification all space characters should be replaced with 0x0020 space character.
        // http://www.whatwg.org/specs/web-apps/current-work/multipage/the-canvas-element.html#text-preparation-algorithm
        // The space characters according to specification are : U+0020, U+0009, U+000A, U+000C, and U+000D.
        // http://www.whatwg.org/specs/web-apps/current-work/multipage/common-microsyntaxes.html#space-character
        // This function returns true for 0x000B also, so that this is backward compatible.
        // Otherwise, the test LayoutTests/canvas/philip/tests/2d.text.draw.space.collapse.space.html will fail
        return c == 0x0009 || (c >= 0x000A && c <= 0x000D);
    }

    static String normalizeSpaces(const LChar*, unsigned length);
    static String normalizeSpaces(const UChar*, unsigned length);

private:
    Character();
};

} // namespace blink

#endif
