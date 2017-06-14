// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSPropertyColumnUtils_h
#define CSSPropertyColumnUtils_h

#include "platform/wtf/Allocator.h"

namespace blink {

class CSSParserTokenRange;
class CSSValue;

class CSSPropertyColumnUtils {
  STATIC_ONLY(CSSPropertyColumnUtils);

 public:
  static CSSValue* ConsumeColumnCount(CSSParserTokenRange&);

  static CSSValue* ConsumeColumnWidth(CSSParserTokenRange&);

  static bool ConsumeColumnWidthOrCount(CSSParserTokenRange&,
                                        CSSValue*&,
                                        CSSValue*&);
};

}  // namespace blink

#endif  // CSSPropertyColumnUtils_h
