// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSPrimitiveValueUnitTrie_h
#define CSSPrimitiveValueUnitTrie_h

#include "core/css/CSSPrimitiveValue.h"

namespace blink {

CSSPrimitiveValue::UnitType lookupCSSPrimitiveValueUnit(const LChar* characters8, unsigned length);
CSSPrimitiveValue::UnitType lookupCSSPrimitiveValueUnit(const UChar* characters16, unsigned length);

} // namespace blink

#endif
