// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSPropertyPlaceUtils_h
#define CSSPropertyPlaceUtils_h

#include "platform/wtf/Allocator.h"

namespace blink {

class CSSParserTokenRange;
class CSSValue;

using ConsumePlaceAlignmentValue = CSSValue* (*)(CSSParserTokenRange&);

class CSSPropertyPlaceUtils {
  STATIC_ONLY(CSSPropertyPlaceUtils);

 public:
  static bool ConsumePlaceAlignment(CSSParserTokenRange&,
                                    ConsumePlaceAlignmentValue,
                                    CSSValue*& align_value,
                                    CSSValue*& justify_value);
};

}  // namespace blink

#endif  // CSSPropertyPlaceUtils_h
