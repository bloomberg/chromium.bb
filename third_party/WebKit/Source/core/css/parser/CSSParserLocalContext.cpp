// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/parser/CSSParserLocalContext.h"

namespace blink {

CSSParserLocalContext::CSSParserLocalContext() : use_alias_parsing_(false) {}

CSSParserLocalContext::CSSParserLocalContext(bool use_alias_parsing)
    : use_alias_parsing_(use_alias_parsing) {}

bool CSSParserLocalContext::UseAliasParsing() const {
  return use_alias_parsing_;
}

}  // namespace blink
