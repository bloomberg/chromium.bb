// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSOMKeywords_h
#define CSSOMKeywords_h

#include "core/CSSPropertyNames.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class CSSKeywordValue;

// CSSOMKeywords provides utility methods for determining whether a given
// CSSKeywordValue is valid for a given CSS Property.
//
// The implementation for this class is generated using input from
// CSSProperties.json5 and build/scripts/make_cssom_types.py.
class CSSOMKeywords {
  STATIC_ONLY(CSSOMKeywords);

 public:
  static bool ValidKeywordForProperty(CSSPropertyID, const CSSKeywordValue&);
};

}  // namespace blink

#endif
