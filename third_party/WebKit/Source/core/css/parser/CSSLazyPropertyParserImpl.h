// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSLazyPropertyParserImpl_h
#define CSSLazyPropertyParserImpl_h

#include "core/css/StylePropertySet.h"
#include "core/css/parser/CSSParserTokenRange.h"
#include "core/css/parser/CSSTokenizer.h"
#include "platform/wtf/Vector.h"

namespace blink {

class CSSLazyParsingState;

// This class is responsible for lazily parsing a single CSS declaration list.
class CSSLazyPropertyParserImpl : public CSSLazyPropertyParser {
 public:
  CSSLazyPropertyParserImpl(CSSParserTokenRange block, CSSLazyParsingState*);

  // CSSLazyPropertyParser:
  StylePropertySet* ParseProperties() override;
  void SetHasBeforeOrAfter() override { has_before_or_after_ = true; }

  DEFINE_INLINE_TRACE() {
    visitor->Trace(lazy_state_);
    CSSLazyPropertyParser::Trace(visitor);
  }

 private:
  Vector<CSSParserToken> tokens_;
  Member<CSSLazyParsingState> lazy_state_;
  bool has_before_or_after_ = false;
};

}  // namespace blink

#endif  // CSSLazyPropertyParserImpl_h
