// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSParserLocalContext_h
#define CSSParserLocalContext_h

#include "platform/wtf/Allocator.h"

namespace blink {

// A wrapper class containing all local context when parsing a property.
// TODO(jiameng): add info for shorthand properties into this class.

class CSSParserLocalContext {
  STACK_ALLOCATED();

 public:
  CSSParserLocalContext();
  explicit CSSParserLocalContext(bool use_alias_parsing);
  bool UseAliasParsing() const;

 private:
  bool use_alias_parsing_;
};

}  // namespace blink

#endif  // CSSParserLocalContext_h
