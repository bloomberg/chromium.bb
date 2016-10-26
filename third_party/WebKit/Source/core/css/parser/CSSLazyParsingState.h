// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSLazyParsingState_h
#define CSSLazyParsingState_h

#include "core/css/CSSSelectorList.h"
#include "core/css/parser/CSSParserMode.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"

namespace blink {

// This class helps lazy parsing by retaining necessary state. It should not
// outlive the StyleSheetContents that initiated the parse, as it retains a raw
// reference to the UseCounter associated with the style sheet.
class CSSLazyParsingState
    : public GarbageCollectedFinalized<CSSLazyParsingState> {
 public:
  CSSLazyParsingState(const CSSParserContext&,
                      Vector<String> escapedStrings,
                      const String& sheetText);

  // This should be a copy of the context used in CSSParser::parseSheet.
  const CSSParserContext& context() { return m_context; }

  bool shouldLazilyParseProperties(const CSSSelectorList&);

  DEFINE_INLINE_TRACE() {}

 private:
  CSSParserContext m_context;
  Vector<String> m_escapedStrings;
  // Also referenced on the css resource.
  String m_sheetText;
};

}  // namespace blink

#endif  // CSSLazyParsingState_h
