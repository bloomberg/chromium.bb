// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSSupportsParser_h
#define CSSSupportsParser_h

#include "platform/heap/Handle.h"

namespace blink {

class CSSParserImpl;
class CSSParserTokenRange;

class CSSSupportsParser {
  STACK_ALLOCATED();

 public:
  enum SupportsResult { kUnsupported = false, kSupported = true, kInvalid };
  enum SupportsParsingMode { kForAtRule, kForWindowCSS };

  static SupportsResult SupportsCondition(CSSParserTokenRange,
                                          CSSParserImpl&,
                                          SupportsParsingMode);

 private:
  CSSSupportsParser(CSSParserImpl& parser) : parser_(parser) {}

  SupportsResult ConsumeCondition(CSSParserTokenRange);
  SupportsResult ConsumeNegation(CSSParserTokenRange);
  SupportsResult ConsumeDeclarationCondition(CSSParserTokenRange&);

  SupportsResult ConsumeConditionInParenthesis(CSSParserTokenRange&);

  CSSParserImpl& parser_;
};

}  // namespace blink

#endif  // CSSSupportsParser_h
