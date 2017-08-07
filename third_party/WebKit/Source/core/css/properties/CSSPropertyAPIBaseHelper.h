// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSPropertyAPIBaseHelper_h
#define CSSPropertyAPIBaseHelper_h

#include "core/CSSPropertyNames.h"
#include "core/css/CSSProperty.h"
#include "core/css/parser/CSSParserMode.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class CSSParserTokenRange;
class CSSParserContext;
class CSSValue;
class StylePropertyShorthand;

// This file contains a bunch of helper methods that are used in CSSPropertyAPI
namespace CSSPropertyAPIBaseHelper {

bool ConsumeBackgroundShorthand(const StylePropertyShorthand&,
                                bool important,
                                CSSParserTokenRange&,
                                const CSSParserContext&,
                                HeapVector<CSSProperty, 256>& properties);
bool ConsumeBorder(bool important,
                   CSSParserTokenRange&,
                   const CSSParserContext&,
                   HeapVector<CSSProperty, 256>& properties);
bool ConsumeRepeatStyleComponent(CSSParserTokenRange&,
                                 CSSValue*& value1,
                                 CSSValue*& value2,
                                 bool& implicit);
CSSValue* ConsumeBackgroundSize(CSSParserTokenRange&,
                                CSSParserMode,
                                bool use_legacy_parsing);
CSSValue* ConsumePrefixedBackgroundBox(CSSParserTokenRange&,
                                       const CSSParserContext*,
                                       bool allow_text_value);

}  // namespace CSSPropertyAPIBaseHelper

}  // namespace blink

#endif  // CSSPropertyAPIBaseHelper_h
