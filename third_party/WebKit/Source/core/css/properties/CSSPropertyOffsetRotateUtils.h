// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSPropertyOffsetRotateUtils_h
#define CSSPropertyOffsetRotateUtils_h

#include "platform/wtf/Allocator.h"

namespace blink {

class CSSParserContext;
class CSSParserTokenRange;
class CSSValue;

class CSSPropertyOffsetRotateUtils {
  STATIC_ONLY(CSSPropertyOffsetRotateUtils);

 public:
  static CSSValue* ConsumeOffsetRotate(CSSParserTokenRange&,
                                       const CSSParserContext&);
};

}  // namespace blink

#endif  // CSSPropertyOffsetRotateUtils_h
