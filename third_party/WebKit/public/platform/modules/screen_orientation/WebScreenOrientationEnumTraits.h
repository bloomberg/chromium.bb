// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebScreenOrientationEnumTraits_h
#define WebScreenOrientationEnumTraits_h

#include "third_party/WebKit/public/platform/modules/screen_orientation/WebScreenOrientationLockType.h"
#include "third_party/WebKit/public/platform/modules/screen_orientation/screen_orientation_lock_types.mojom-shared.h"

namespace mojo {

template <>
struct EnumTraits<::blink::mojom::ScreenOrientationLockType,
                  ::blink::WebScreenOrientationLockType> {
  static ::blink::mojom::ScreenOrientationLockType ToMojom(
      ::blink::WebScreenOrientationLockType lockType) {
    switch (lockType) {
      case ::blink::WebScreenOrientationLockDefault:
        return ::blink::mojom::ScreenOrientationLockType::DEFAULT;
      case ::blink::WebScreenOrientationLockPortraitPrimary:
        return ::blink::mojom::ScreenOrientationLockType::PORTRAIT_PRIMARY;
      case ::blink::WebScreenOrientationLockPortraitSecondary:
        return ::blink::mojom::ScreenOrientationLockType::PORTRAIT_SECONDARY;
      case ::blink::WebScreenOrientationLockLandscapePrimary:
        return ::blink::mojom::ScreenOrientationLockType::LANDSCAPE_PRIMARY;
      case ::blink::WebScreenOrientationLockLandscapeSecondary:
        return ::blink::mojom::ScreenOrientationLockType::LANDSCAPE_SECONDARY;
      case ::blink::WebScreenOrientationLockAny:
        return ::blink::mojom::ScreenOrientationLockType::ANY;
      case ::blink::WebScreenOrientationLockLandscape:
        return ::blink::mojom::ScreenOrientationLockType::LANDSCAPE;
      case ::blink::WebScreenOrientationLockPortrait:
        return ::blink::mojom::ScreenOrientationLockType::PORTRAIT;
      case ::blink::WebScreenOrientationLockNatural:
        return ::blink::mojom::ScreenOrientationLockType::NATURAL;
    }
    NOTREACHED();
    return ::blink::mojom::ScreenOrientationLockType::DEFAULT;
  }

  static bool FromMojom(::blink::mojom::ScreenOrientationLockType lockType,
                        ::blink::WebScreenOrientationLockType* out) {
    switch (lockType) {
      case ::blink::mojom::ScreenOrientationLockType::DEFAULT:
        *out = ::blink::WebScreenOrientationLockDefault;
        return true;
      case ::blink::mojom::ScreenOrientationLockType::PORTRAIT_PRIMARY:
        *out = ::blink::WebScreenOrientationLockPortraitPrimary;
        return true;
      case ::blink::mojom::ScreenOrientationLockType::PORTRAIT_SECONDARY:
        *out = ::blink::WebScreenOrientationLockPortraitSecondary;
        return true;
      case ::blink::mojom::ScreenOrientationLockType::LANDSCAPE_PRIMARY:
        *out = ::blink::WebScreenOrientationLockLandscapePrimary;
        return true;
      case ::blink::mojom::ScreenOrientationLockType::LANDSCAPE_SECONDARY:
        *out = ::blink::WebScreenOrientationLockLandscapeSecondary;
        return true;
      case ::blink::mojom::ScreenOrientationLockType::ANY:
        *out = ::blink::WebScreenOrientationLockAny;
        return true;
      case ::blink::mojom::ScreenOrientationLockType::LANDSCAPE:
        *out = ::blink::WebScreenOrientationLockLandscape;
        return true;
      case ::blink::mojom::ScreenOrientationLockType::PORTRAIT:
        *out = ::blink::WebScreenOrientationLockPortrait;
        return true;
      case ::blink::mojom::ScreenOrientationLockType::NATURAL:
        *out = ::blink::WebScreenOrientationLockNatural;
        return true;
    }
    NOTREACHED();
    return false;
  }
};

}  // namespace mojo

#endif  // WebScreenOrientationEnumTraits_h
