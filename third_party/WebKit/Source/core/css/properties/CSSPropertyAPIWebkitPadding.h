// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSPropertyAPIWebkitPadding_h
#define CSSPropertyAPIWebkitPadding_h

#include "core/CSSPropertyNames.h"
#include "core/css/properties/CSSPropertyAPI.h"

namespace blink {

// TODO(aazzam): Generate API .h files

class CSSParserTokenRange;
class CSSParserContext;

class CSSPropertyAPIWebkitPadding : public CSSPropertyAPI {
 public:
  static const CSSValue* parseSingleValue(CSSParserTokenRange&,
                                          const CSSParserContext&);
};

}  // namespace blink

#endif  // CSSPropertyAPIWebkitPadding_h
