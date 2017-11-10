// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FontStyleResolver_h
#define FontStyleResolver_h

#include "core/css/CSSPrimitiveValueMappings.h"
#include "core/css/CSSPropertyValueSet.h"
#include "core/css/CSSValueIDMappings.h"
#include "platform/fonts/FontDescription.h"
#include "platform/fonts/FontSelector.h"

namespace blink {

// FontStyleResolver is a simpler version of Font-related parts of
// StyleResolver. This is needed because ComputedStyle/StyleResolver can't run
// outside the main thread or without a document. This is a way of minimizing
// duplicate code for when font parsing is needed.
class CORE_EXPORT FontStyleResolver {
  STATIC_ONLY(FontStyleResolver);

 public:
  static FontDescription ComputeFont(const CSSPropertyValueSet&, FontSelector*);
};

}  // namespace blink

#endif
