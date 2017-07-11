// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSPropertyTextDecorationLineUtils_h
#define CSSPropertyTextDecorationLineUtils_h

#include "platform/wtf/Allocator.h"

namespace blink {

class CSSParserTokenRange;
class CSSValue;

class CSSPropertyTextDecorationLineUtils {
  STATIC_ONLY(CSSPropertyTextDecorationLineUtils);

 public:
  static CSSValue* ConsumeTextDecorationLine(CSSParserTokenRange&);
};

}  // namespace blink

#endif  // CSSPropertyTextDecorationLineUtils_h
