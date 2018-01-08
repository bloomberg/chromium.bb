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
class CSSProperty;

class LengthPropertyFunctions {
  STATIC_ONLY(LengthPropertyFunctions);

 public:
  static ValueRange GetValueRange(const CSSProperty&);
  static bool IsZoomedLength(const CSSProperty&);
  static bool GetPixelsForKeyword(const CSSProperty&,
                                  CSSValueID,
                                  double& result_pixels);
  static bool GetInitialLength(const CSSProperty&, Length& result);
  static bool GetLength(const CSSProperty&,
                        const ComputedStyle&,
                        Length& result);
  static bool SetLength(const CSSProperty&, ComputedStyle&, const Length&);
};

}  // namespace blink

#endif  // LengthPropertyFunctions_h
