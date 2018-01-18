// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AtRuleDescriptorParser_h
#define AtRuleDescriptorParser_h

#include "core/css/CSSPropertyValue.h"
#include "core/css/parser/AtRuleDescriptors.h"
#include "platform/wtf/Vector.h"

namespace blink {

class CSSParserContext;
class CSSParserTokenRange;
class CSSValue;

class AtRuleDescriptorParser {
  STATIC_ONLY(AtRuleDescriptorParser);

 public:
  static bool ParseAtRule(AtRuleDescriptorID,
                          CSSParserTokenRange&,
                          const CSSParserContext&,
                          HeapVector<CSSPropertyValue, 256>&);
  static CSSValue* ParseAtRule(AtRuleDescriptorID,
                               const String& value,
                               const CSSParserContext&);
  static CSSValue* ParseFontFaceDescriptor(AtRuleDescriptorID,
                                           CSSParserTokenRange&,
                                           const CSSParserContext&);
  static CSSValue* ParseFontFaceDescriptor(AtRuleDescriptorID,
                                           const String& value,
                                           const CSSParserContext&);
  static CSSValue* ParseFontFaceDeclaration(CSSParserTokenRange&,
                                            const CSSParserContext&);
};

}  // namespace blink

#endif  // AtRuleDescriptorParser_h
