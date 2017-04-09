// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FilterListPropertyFunctions_h
#define FilterListPropertyFunctions_h

#include "core/CSSPropertyNames.h"
#include "core/style/ComputedStyle.h"
#include "wtf/Allocator.h"

namespace blink {

class FilterListPropertyFunctions {
  STATIC_ONLY(FilterListPropertyFunctions);

 public:
  static const FilterOperations& GetInitialFilterList(CSSPropertyID property) {
    return GetFilterList(property, ComputedStyle::InitialStyle());
  }

  static const FilterOperations& GetFilterList(CSSPropertyID property,
                                               const ComputedStyle& style) {
    switch (property) {
      default:
        NOTREACHED();
      // Fall through.
      case CSSPropertyBackdropFilter:
        return style.BackdropFilter();
      case CSSPropertyFilter:
        return style.Filter();
    }
  }

  static void SetFilterList(CSSPropertyID property,
                            ComputedStyle& style,
                            const FilterOperations& filter_operations) {
    switch (property) {
      case CSSPropertyBackdropFilter:
        style.SetBackdropFilter(filter_operations);
        break;
      case CSSPropertyFilter:
        style.SetFilter(filter_operations);
        break;
      default:
        NOTREACHED();
        break;
    }
  }
};

}  // namespace blink

#endif  // FilterListPropertyFunctions_h
