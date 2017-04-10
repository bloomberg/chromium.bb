// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LengthPropertyFunctions_h
#define LengthPropertyFunctions_h

#include "core/CSSPropertyNames.h"
#include "core/CSSValueKeywords.h"
#include "platform/Length.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class ComputedStyle;

class LengthPropertyFunctions {
  STATIC_ONLY(LengthPropertyFunctions);

 public:
  static ValueRange GetValueRange(CSSPropertyID);
  static bool IsZoomedLength(CSSPropertyID);
  static bool GetPixelsForKeyword(CSSPropertyID,
                                  CSSValueID,
                                  double& result_pixels);
  static bool GetInitialLength(CSSPropertyID, Length& result);
  static bool GetLength(CSSPropertyID, const ComputedStyle&, Length& result);
  static bool SetLength(CSSPropertyID, ComputedStyle&, const Length&);
};

}  // namespace blink

#endif  // LengthPropertyFunctions_h
