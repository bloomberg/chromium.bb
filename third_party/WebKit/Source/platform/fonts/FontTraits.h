/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 * Copyright (C) 2014 Google Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
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

#ifndef FontTraits_h
#define FontTraits_h

#include "platform/wtf/Allocator.h"
#include "platform/wtf/Assertions.h"

namespace blink {

enum FontWeight {
  kFontWeight100 = 0,
  kFontWeight200 = 1,
  kFontWeight300 = 2,
  kFontWeight400 = 3,
  kFontWeight500 = 4,
  kFontWeight600 = 5,
  kFontWeight700 = 6,
  kFontWeight800 = 7,
  kFontWeight900 = 8,
  kFontWeightNormal = kFontWeight400,
  kFontWeightBold = kFontWeight700
};

// Converts a FontWeight to its corresponding numeric value
inline int NumericFontWeight(FontWeight weight) {
  return (weight - kFontWeight100 + 1) * 100;
}

// Numeric values matching OS/2 & Windows Metrics usWidthClass table.
// https://www.microsoft.com/typography/otspec/os2.htm
enum FontStretch {
  kFontStretchUltraCondensed = 1,
  kFontStretchExtraCondensed = 2,
  kFontStretchCondensed = 3,
  kFontStretchSemiCondensed = 4,
  kFontStretchNormal = 5,
  kFontStretchSemiExpanded = 6,
  kFontStretchExpanded = 7,
  kFontStretchExtraExpanded = 8,
  kFontStretchUltraExpanded = 9
};

enum FontStyle {
  kFontStyleNormal = 0,
  kFontStyleOblique = 1,
  kFontStyleItalic = 2
};

typedef unsigned FontTraitsBitfield;

struct FontTraits {
  DISALLOW_NEW();
  FontTraits(FontStyle style, FontWeight weight, FontStretch stretch) {
    traits_.style_ = style;
    traits_.weight_ = weight;
    traits_.stretch_ = stretch;
    traits_.filler_ = 0;
    DCHECK_EQ(bitfield_ >> 10, 0u);
  }
  FontTraits(FontTraitsBitfield bitfield) : bitfield_(bitfield) {
    DCHECK_EQ(traits_.filler_, 0u);
    DCHECK_EQ(bitfield_ >> 10, 0u);
  }
  FontStyle Style() const { return static_cast<FontStyle>(traits_.style_); }
  FontWeight Weight() const { return static_cast<FontWeight>(traits_.weight_); }
  FontStretch Stretch() const {
    return static_cast<FontStretch>(traits_.stretch_);
  }
  FontTraitsBitfield Bitfield() const { return bitfield_; }

  union {
    struct {
      unsigned style_ : 2;
      unsigned weight_ : 4;
      unsigned stretch_ : 4;
      unsigned filler_ : 22;
    } traits_;
    FontTraitsBitfield bitfield_;
  };
};

}  // namespace blink
#endif  // FontTraits_h
