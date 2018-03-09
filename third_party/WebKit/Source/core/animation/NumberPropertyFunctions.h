// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NumberPropertyFunctions_h
#define NumberPropertyFunctions_h

#include "core/css_property_names.h"
#include "platform/wtf/Optional.h"

namespace blink {

class ComputedStyle;
class CSSProperty;

class NumberPropertyFunctions {
 public:
  static Optional<double> GetInitialNumber(const CSSProperty&);
  static Optional<double> GetNumber(const CSSProperty&, const ComputedStyle&);
  static double ClampNumber(const CSSProperty&, double);
  static bool SetNumber(const CSSProperty&, ComputedStyle&, double);
};

}  // namespace blink

#endif  // NumberPropertyFunctions_h
