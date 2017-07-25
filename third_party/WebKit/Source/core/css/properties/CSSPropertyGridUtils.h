// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSPropertyGridUtils_h
#define CSSPropertyGridUtils_h

#include "platform/wtf/Allocator.h"

namespace blink {

class CSSCustomIdentValue;
class CSSParserTokenRange;
class CSSValue;

class CSSPropertyGridUtils {
  STATIC_ONLY(CSSPropertyGridUtils);

 public:
  // TODO(jiameng): move the following functions to anonymous namespace after
  // all grid-related shorthands have their APIs implemented:
  // - ConsumeCustomIdentForGridLine
  // - ConsumeGridLine
  static CSSCustomIdentValue* ConsumeCustomIdentForGridLine(
      CSSParserTokenRange&);
  static CSSValue* ConsumeGridLine(CSSParserTokenRange&);
  static bool ConsumeGridItemPositionShorthand(bool important,
                                               CSSParserTokenRange&,
                                               CSSValue*& start_value,
                                               CSSValue*& end_value);
};

}  // namespace blink

#endif  // CSSPropertyGridUtils_h
