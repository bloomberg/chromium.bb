// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSOMTypes_h
#define CSSOMTypes_h

#include "core/CSSPropertyNames.h"
#include "core/css/cssom/CSSStyleValue.h"
#include "platform/wtf/Allocator.h"

namespace blink {

// This class provides utility functions for determining whether a property
// can accept a CSSStyleValue type or instance. Its implementation is generated
// using input from CSSProperties.json5 and the script
// build/scripts/make_cssom_types.py.
class CSSOMTypes {
  STATIC_ONLY(CSSOMTypes);

 public:
  static bool PropertyCanTake(CSSPropertyID, const CSSStyleValue&);
  static bool PropertyCanTakeType(CSSPropertyID, CSSStyleValue::StyleValueType);
};

}  // namespace blink

#endif
