// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NumberAttributeFunctions_h
#define NumberAttributeFunctions_h

#include "core/svg_names.h"

namespace blink {

class NumberAttributeFunctions {
 public:
  static bool IsNonNegative(const QualifiedName& attribute) {
    return attribute == SVGNames::pathLengthAttr;
  }
};

}  // namespace blink

#endif  // NumberAttributeFunctions_h
