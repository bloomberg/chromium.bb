// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSPropertyBackgroundUtils_h
#define CSSPropertyBackgroundUtils_h

#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class CSSParserContext;
class CSSParserTokenRange;
class CSSValue;

enum class ParsingStyle { kLegacy, kNotLegacy };

class CSSPropertyBackgroundUtils {
  STATIC_ONLY(CSSPropertyBackgroundUtils);

 public:
  // TODO(jiameng): move AddBackgroundValue to anonymous namespace once all
  // background-related properties have their APIs.
  static void AddBackgroundValue(CSSValue*& list, CSSValue*);

  static CSSValue* ConsumeBackgroundAttachment(CSSParserTokenRange&);
  static CSSValue* ConsumeBackgroundBlendMode(CSSParserTokenRange&);
  static CSSValue* ConsumeBackgroundBox(CSSParserTokenRange&);
  static CSSValue* ConsumeBackgroundComposite(CSSParserTokenRange&);
  static CSSValue* ConsumeMaskSourceType(CSSParserTokenRange&);

  static CSSValue* ConsumeBackgroundSize(CSSParserTokenRange&,
                                         CSSParserMode,
                                         ParsingStyle);
  static bool ConsumeBackgroundPosition(CSSParserTokenRange&,
                                        const CSSParserContext&,
                                        CSSPropertyParserHelpers::UnitlessQuirk,
                                        CSSValue*& result_x,
                                        CSSValue*& result_y);
};

}  // namespace blink

#endif  // CSSPropertyBackgroundUtils_h
