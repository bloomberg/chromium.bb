// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSParserFastPaths_h
#define CSSParserFastPaths_h

#include "core/CSSPropertyNames.h"
#include "core/CSSValueKeywords.h"
#include "platform/graphics/Color.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Forward.h"

namespace blink {

class CSSValue;

class CORE_EXPORT CSSParserFastPaths {
  STATIC_ONLY(CSSParserFastPaths);

 public:
  // Parses simple values like '10px' or 'green', but makes no guarantees
  // about handling any property completely.
  static CSSValue* MaybeParseValue(CSSPropertyID, const String&, CSSParserMode);

  // Properties handled here shouldn't be explicitly handled in
  // CSSPropertyParser
  static bool IsKeywordPropertyID(CSSPropertyID);

  // Returns if a property should be handled by the fast path, but have other
  // non-keyword values which should be handled by the CSSPropertyParser.
  static bool IsPartialKeywordPropertyID(CSSPropertyID);

  static bool IsValidKeywordPropertyAndValue(CSSPropertyID,
                                             CSSValueID,
                                             CSSParserMode);

  static CSSValue* ParseColor(const String&, CSSParserMode);
};

}  // namespace blink

#endif  // CSSParserFastPaths_h
