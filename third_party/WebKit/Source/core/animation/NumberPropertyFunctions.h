// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NumberPropertyFunctions_h
#define NumberPropertyFunctions_h

#include "core/CSSPropertyNames.h"

namespace blink {

class ComputedStyle;
class CSSProperty;

class NumberPropertyFunctions {
 public:
  static bool GetInitialNumber(const CSSProperty&, double& result);
  static bool GetNumber(const CSSProperty&,
                        const ComputedStyle&,
                        double& result);
  static double ClampNumber(const CSSProperty&, double);
  static bool SetNumber(const CSSProperty&, ComputedStyle&, double);
};

}  // namespace blink

#endif  // NumberPropertyFunctions_h
