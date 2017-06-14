// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSPropertyOffsetPathUtils_h
#define CSSPropertyOffsetPathUtils_h

#include "platform/wtf/Allocator.h"

namespace blink {

class CSSParserContext;
class CSSParserTokenRange;
class CSSValue;

class CSSPropertyOffsetPathUtils {
  STATIC_ONLY(CSSPropertyOffsetPathUtils);

 public:
  static CSSValue* ConsumeOffsetPath(CSSParserTokenRange&,
                                     const CSSParserContext&);
  static CSSValue* ConsumePathOrNone(CSSParserTokenRange&);
};

}  // namespace blink

#endif  // CSSPropertyOffsetPathUtils_h
