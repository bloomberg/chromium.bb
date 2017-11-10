// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSLazyPropertyParserImpl_h
#define CSSLazyPropertyParserImpl_h

#include "core/css/CSSPropertyValueSet.h"
#include "core/css/parser/CSSParserTokenRange.h"
#include "core/css/parser/CSSTokenizer.h"

namespace blink {

class CSSLazyParsingState;

// This class is responsible for lazily parsing a single CSS declaration list.
class CSSLazyPropertyParserImpl : public CSSLazyPropertyParser {
 public:
  CSSLazyPropertyParserImpl(size_t offset, CSSLazyParsingState*);

  // CSSLazyPropertyParser:
  CSSPropertyValueSet* ParseProperties() override;

  void Trace(blink::Visitor* visitor) {
    visitor->Trace(lazy_state_);
    CSSLazyPropertyParser::Trace(visitor);
  }

 private:
  size_t offset_;
  Member<CSSLazyParsingState> lazy_state_;
};

}  // namespace blink

#endif  // CSSLazyPropertyParserImpl_h
