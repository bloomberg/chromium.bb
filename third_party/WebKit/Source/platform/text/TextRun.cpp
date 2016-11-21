/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/text/TextRun.h"

#include "platform/RuntimeEnabledFeatures.h"
#include "platform/text/Character.h"

namespace blink {

struct ExpectedTextRunSize {
  DISALLOW_NEW();
  const void* pointer;
  int integers[2];
  float float1;
  float float2;
  float float3;
  uint32_t bitfields : 10;
  TabSize tabSize;
};

static_assert(sizeof(TextRun) == sizeof(ExpectedTextRunSize),
              "TextRun should have expected size");

void TextRun::setText(const String& string) {
  m_len = string.length();
  if (!m_len) {
    m_data.characters8 = 0;
    m_is8Bit = true;
    return;
  }
  m_is8Bit = string.is8Bit();
  if (m_is8Bit)
    m_data.characters8 = string.characters8();
  else
    m_data.characters16 = string.characters16();
}

std::unique_ptr<UChar[]> TextRun::normalizedUTF16(
    unsigned* resultLength) const {
  const UChar* source;
  String stringFor8BitRun;
  if (is8Bit()) {
    stringFor8BitRun = String::make16BitFrom8BitSource(characters8(), length());
    source = stringFor8BitRun.characters16();
  } else {
    source = characters16();
  }

  UChar* buffer = new UChar[m_len + 1];
  *resultLength = 0;

  bool error = false;
  unsigned position = 0;
  while (position < m_len) {
    UChar32 character;
    U16_NEXT(source, position, m_len, character);
    // Don't normalize tabs as they are not treated as spaces for word-end.
    if (normalizeSpace() &&
        Character::isNormalizedCanvasSpaceCharacter(character)) {
      character = spaceCharacter;
    } else if (Character::treatAsSpace(character) &&
               character != noBreakSpaceCharacter) {
      character = spaceCharacter;
    } else if (!RuntimeEnabledFeatures::
                   renderUnicodeControlCharactersEnabled() &&
               Character::legacyTreatAsZeroWidthSpaceInComplexScript(
                   character)) {
      character = zeroWidthSpaceCharacter;
    } else if (Character::treatAsZeroWidthSpaceInComplexScript(character)) {
      character = zeroWidthSpaceCharacter;
    }

    U16_APPEND(buffer, *resultLength, m_len, character, error);
    DCHECK(!error);
  }

  DCHECK(*resultLength <= m_len);
  return wrapArrayUnique(buffer);
}

}  // namespace blink
