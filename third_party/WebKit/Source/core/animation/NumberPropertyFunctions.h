// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NumberPropertyFunctions_h
#define NumberPropertyFunctions_h

#include "core/CSSPropertyNames.h"

namespace blink {

class ComputedStyle;

class NumberPropertyFunctions {
 public:
  static bool GetInitialNumber(CSSPropertyID, double& result);
  static bool GetNumber(CSSPropertyID, const ComputedStyle&, double& result);
  static double ClampNumber(CSSPropertyID, double);
  static bool SetNumber(CSSPropertyID, ComputedStyle&, double);
};

}  // namespace blink

#endif  // NumberPropertyFunctions_h
