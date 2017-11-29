/*
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef UTF16TextIterator_h
#define UTF16TextIterator_h

#include "base/macros.h"
#include "platform/PlatformExport.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/text/CharacterNames.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class PLATFORM_EXPORT UTF16TextIterator {
  USING_FAST_MALLOC(UTF16TextIterator);

 public:
  // The passed in UChar pointer starts at 'offset'. The iterator operates on
  // the range [offset, endOffset].
  // 'length' denotes the maximum length of the UChar array, which might exceed
  // 'endOffset'.
  UTF16TextIterator(const UChar*, int length);

  inline bool Consume(UChar32& character) {
    if (offset_ >= length_)
      return false;

    character = *characters_;
    current_glyph_length_ = 1;
    if (!U16_IS_SURROGATE(character))
      return true;

    return ConsumeSurrogatePair(character);
  }

  void Advance() {
    characters_ += current_glyph_length_;
    offset_ += current_glyph_length_;
  }

  int Offset() const { return offset_; }
  const UChar* Characters() const { return characters_; }
  const UChar* GlyphEnd() const { return characters_ + current_glyph_length_; }

 private:
  bool IsValidSurrogatePair(UChar32&);
  bool ConsumeSurrogatePair(UChar32&);
  void ConsumeMultipleUChar();

  const UChar* characters_;
  const UChar* characters_end_;
  int offset_;
  int length_;
  unsigned current_glyph_length_;

  DISALLOW_COPY_AND_ASSIGN(UTF16TextIterator);
};

}  // namespace blink

#endif
