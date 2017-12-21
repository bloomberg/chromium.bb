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
#include "platform/text/CharacterProperty.h"
#include "platform/text/TextDirection.h"
#include "platform/text/TextRun.h"
#include "platform/wtf/ASCIICType.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/HashSet.h"
#include "platform/wtf/text/CharacterNames.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class PLATFORM_EXPORT Character {
  STATIC_ONLY(Character);

 public:
  static inline bool IsInRange(UChar32 character,
                               UChar32 lower_bound,
                               UChar32 upper_bound) {
    return character >= lower_bound && character <= upper_bound;
  }

  static inline bool IsUnicodeVariationSelector(UChar32 character) {
    // http://www.unicode.org/Public/UCD/latest/ucd/StandardizedVariants.html
    return IsInRange(character, 0x180B,
                     0x180D)  // MONGOLIAN FREE VARIATION SELECTOR ONE to THREE
           ||
           IsInRange(character, 0xFE00, 0xFE0F)  // VARIATION SELECTOR-1 to 16
           || IsInRange(character, 0xE0100,
                        0xE01EF);  // VARIATION SELECTOR-17 to 256
  }

  static bool IsCJKIdeographOrSymbol(UChar32 c) {
    // Below U+02C7 is likely a common case.
    return c < 0x2C7 ? false : IsCJKIdeographOrSymbolSlow(c);
  }
  static bool IsCJKIdeographOrSymbolBase(UChar32 c) {
    return IsCJKIdeographOrSymbol(c) &&
           !(U_GET_GC_MASK(c) & (U_GC_M_MASK | U_GC_LM_MASK | U_GC_SK_MASK));
  }

  static unsigned ExpansionOpportunityCount(const LChar*,
                                            size_t length,
                                            TextDirection,
                                            bool& is_after_expansion,
                                            const TextJustify);
  static unsigned ExpansionOpportunityCount(const UChar*,
                                            size_t length,
                                            TextDirection,
                                            bool& is_after_expansion,
                                            const TextJustify);
  static unsigned ExpansionOpportunityCount(const TextRun& run,
                                            bool& is_after_expansion) {
    if (run.Is8Bit())
      return ExpansionOpportunityCount(run.Characters8(), run.length(),
                                       run.Direction(), is_after_expansion,
                                       run.GetTextJustify());
    return ExpansionOpportunityCount(run.Characters16(), run.length(),
                                     run.Direction(), is_after_expansion,
                                     run.GetTextJustify());
  }

  static bool IsUprightInMixedVertical(UChar32 character);

  // https://html.spec.whatwg.org/multipage/scripting.html#prod-potentialcustomelementname
  static bool IsPotentialCustomElementName8BitChar(LChar ch) {
    return IsASCIILower(ch) || IsASCIIDigit(ch) || ch == '-' || ch == '.' ||
           ch == '_' || ch == 0xb7 || (0xc0 <= ch && ch != 0xd7 && ch != 0xf7);
  }
  static bool IsPotentialCustomElementNameChar(UChar32 character);

  static bool TreatAsSpace(UChar32 c) {
    return c == kSpaceCharacter || c == kTabulationCharacter ||
           c == kNewlineCharacter || c == kNoBreakSpaceCharacter;
  }
  static bool TreatAsZeroWidthSpace(UChar32 c) {
    return TreatAsZeroWidthSpaceInComplexScript(c) ||
           c == kZeroWidthNonJoinerCharacter || c == kZeroWidthJoinerCharacter;
  }
  static bool LegacyTreatAsZeroWidthSpaceInComplexScript(UChar32 c) {
    return c < 0x20  // ASCII Control Characters
           ||
           (c >= 0x7F && c < 0xA0)  // ASCII Delete .. No-break spaceCharacter
           || TreatAsZeroWidthSpaceInComplexScript(c);
  }
  static bool TreatAsZeroWidthSpaceInComplexScript(UChar32 c) {
    return c == kFormFeedCharacter || c == kCarriageReturnCharacter ||
           c == kSoftHyphenCharacter || c == kZeroWidthSpaceCharacter ||
           (c >= kLeftToRightMarkCharacter && c <= kRightToLeftMarkCharacter) ||
           (c >= kLeftToRightEmbedCharacter &&
            c <= kRightToLeftOverrideCharacter) ||
           c == kZeroWidthNoBreakSpaceCharacter ||
           c == kObjectReplacementCharacter;
  }
  static bool CanTextDecorationSkipInk(UChar32);
  static bool CanReceiveTextEmphasis(UChar32);

  static bool IsGraphemeExtended(UChar32 c) {
    // http://unicode.org/reports/tr29/#Extend
    return u_hasBinaryProperty(c, UCHAR_GRAPHEME_EXTEND);
  }

  // Returns true if the character has a Emoji property.
  // See http://www.unicode.org/Public/emoji/3.0/emoji-data.txt
  static bool IsEmoji(UChar32);
  // Default presentation style according to:
  // http://www.unicode.org/reports/tr51/#Presentation_Style
  static bool IsEmojiTextDefault(UChar32);
  static bool IsEmojiEmojiDefault(UChar32);
  static bool IsEmojiModifierBase(UChar32);
  static bool IsEmojiKeycapBase(UChar32);
  static bool IsRegionalIndicator(UChar32);
  static bool IsModifier(UChar32 c) { return c >= 0x1F3FB && c <= 0x1F3FF; }
  // http://www.unicode.org/reports/tr51/proposed.html#flag-emoji-tag-sequences
  static bool IsEmojiFlagSequenceTag(UChar32);

  static inline UChar NormalizeSpaces(UChar character) {
    if (TreatAsSpace(character))
      return kSpaceCharacter;

    if (TreatAsZeroWidthSpace(character))
      return kZeroWidthSpaceCharacter;

    return character;
  }

  static inline bool IsNormalizedCanvasSpaceCharacter(UChar32 c) {
    // According to specification all space characters should be replaced with
    // 0x0020 space character.
    // http://www.whatwg.org/specs/web-apps/current-work/multipage/the-canvas-element.html#text-preparation-algorithm
    // The space characters according to specification are : U+0020, U+0009,
    // U+000A, U+000C, and U+000D.
    // http://www.whatwg.org/specs/web-apps/current-work/multipage/common-microsyntaxes.html#space-character
    // This function returns true for 0x000B also, so that this is backward
    // compatible.  Otherwise, the test
    // LayoutTests/canvas/philip/tests/2d.text.draw.space.collapse.space.html
    // will fail
    return c == 0x0009 || (c >= 0x000A && c <= 0x000D);
  }

  static String NormalizeSpaces(const LChar*, unsigned length);
  static String NormalizeSpaces(const UChar*, unsigned length);

  static bool IsCommonOrInheritedScript(UChar32);
  static bool IsUnassignedOrPrivateUse(UChar32);

 private:
  static bool IsCJKIdeographOrSymbolSlow(UChar32);
};

}  // namespace blink

#endif
