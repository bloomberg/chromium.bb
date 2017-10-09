// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPIWebkitBorderBeforeStyle.h"

#include "core/StylePropertyShorthand.h"
#include "core/css/CSSProperty.h"

namespace blink {

const CSSPropertyAPI&
CSSPropertyAPIWebkitBorderBeforeStyle::ResolveDirectionAwareProperty(
    TextDirection direction,
    WritingMode writing_mode) const {
  return ResolveToPhysicalPropertyAPI(direction, writing_mode, kBeforeSide,
                                      borderStyleShorthand());
}
}  // namespace blink
