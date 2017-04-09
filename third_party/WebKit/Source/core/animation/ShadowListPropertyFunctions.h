// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ShadowListPropertyFunctions_h
#define ShadowListPropertyFunctions_h

#include "core/CSSPropertyNames.h"
#include "core/style/ComputedStyle.h"

namespace blink {

class ShadowListPropertyFunctions {
 public:
  static const ShadowList* GetInitialShadowList(CSSPropertyID) {
    return nullptr;
  }
  static const ShadowList* GetShadowList(CSSPropertyID property,
                                         const ComputedStyle& style) {
    switch (property) {
      case CSSPropertyBoxShadow:
        return style.BoxShadow();
      case CSSPropertyTextShadow:
        return style.TextShadow();
      default:
        NOTREACHED();
        return nullptr;
    }
  }
  static void SetShadowList(CSSPropertyID property,
                            ComputedStyle& style,
                            PassRefPtr<ShadowList> shadow_list) {
    switch (property) {
      case CSSPropertyBoxShadow:
        style.SetBoxShadow(std::move(shadow_list));
        return;
      case CSSPropertyTextShadow:
        style.SetTextShadow(std::move(shadow_list));
        return;
      default:
        NOTREACHED();
    }
  }
};

}  // namespace blink

#endif  // ShadowListPropertyFunctions_h
