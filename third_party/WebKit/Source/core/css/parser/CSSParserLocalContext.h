// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSParserLocalContext_h
#define CSSParserLocalContext_h

#include "core/CSSPropertyNames.h"
#include "platform/wtf/Allocator.h"

namespace blink {

// A wrapper class containing all local context when parsing a property.

class CSSParserLocalContext {
  STACK_ALLOCATED();

 public:
  CSSParserLocalContext();
  CSSParserLocalContext(bool use_alias_parsing,
                        CSSPropertyID current_shorthand);
  bool UseAliasParsing() const;
  CSSPropertyID CurrentShorthand() const;

 private:
  bool use_alias_parsing_;
  CSSPropertyID current_shorthand_;
};

}  // namespace blink

#endif  // CSSParserLocalContext_h
