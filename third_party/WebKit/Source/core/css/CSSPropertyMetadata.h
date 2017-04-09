// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSPropertyMetadata_h
#define CSSPropertyMetadata_h

#include "core/CSSPropertyNames.h"
#include "core/CoreExport.h"
#include "wtf/Allocator.h"
#include "wtf/Vector.h"

namespace blink {

class CORE_EXPORT CSSPropertyMetadata {
  STATIC_ONLY(CSSPropertyMetadata);

 public:
  static bool IsEnabledProperty(CSSPropertyID unresolved_property);
  static bool IsInterpolableProperty(CSSPropertyID unresolved_property);
  static bool IsInheritedProperty(CSSPropertyID unresolved_property);
  static bool PropertySupportsPercentage(CSSPropertyID unresolved_property);
  static bool PropertyIsRepeated(CSSPropertyID unresolved_property);
  static char RepetitionSeparator(CSSPropertyID unresolved_property);
  static bool IsDescriptor(CSSPropertyID unresolved_property);
  static bool IsProperty(CSSPropertyID unresolved_property);

  static void FilterEnabledCSSPropertiesIntoVector(const CSSPropertyID*,
                                                   size_t length,
                                                   Vector<CSSPropertyID>&);
};

}  // namespace blink

#endif  // CSSPropertyMetadata
