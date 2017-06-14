// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSPropertyCounterUtils_h
#define CSSPropertyCounterUtils_h

#include "platform/wtf/Allocator.h"

namespace blink {

class CSSParserTokenRange;
class CSSValue;

class CSSPropertyCounterUtils {
  STATIC_ONLY(CSSPropertyCounterUtils);

 public:
  static const int kResetDefaultValue = 0;
  static const int kIncrementDefaultValue = 1;

  static CSSValue* ConsumeCounter(CSSParserTokenRange&, int);
};

}  // namespace blink

#endif  // CSSPropertyCounterUtils_h
