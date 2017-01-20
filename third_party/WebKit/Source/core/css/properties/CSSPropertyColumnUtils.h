// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSPropertyColumnUtils_h
#define CSSPropertyColumnUtils_h

#include "wtf/Allocator.h"

namespace blink {

class CSSParserTokenRange;
class CSSValue;

class CSSPropertyColumnUtils {
  STATIC_ONLY(CSSPropertyColumnUtils);

  static CSSValue* consumeColumnCount(CSSParserTokenRange&);

  static CSSValue* consumeColumnWidth(CSSParserTokenRange&);

  static bool consumeColumnWidthOrCount(CSSParserTokenRange&,
                                        CSSValue*&,
                                        CSSValue*&);
};

}  // namespace blink

#endif  // CSSPropertyColumnUtils_h
