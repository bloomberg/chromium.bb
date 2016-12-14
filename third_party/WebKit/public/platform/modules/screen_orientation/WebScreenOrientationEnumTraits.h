// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebScreenOrientationEnumTraits_h
#define WebScreenOrientationEnumTraits_h

#include "device/screen_orientation/public/interfaces/screen_orientation_lock_types.mojom-shared.h"
#include "third_party/WebKit/public/platform/modules/screen_orientation/WebScreenOrientationLockType.h"

namespace mojo {

template <>
struct EnumTraits<::device::mojom::ScreenOrientationLockType,
                  ::blink::WebScreenOrientationLockType> {
  static ::device::mojom::ScreenOrientationLockType ToMojom(
      ::blink::WebScreenOrientationLockType lockType) {
    switch (lockType) {
      case ::blink::WebScreenOrientationLockDefault:
        return ::device::mojom::ScreenOrientationLockType::DEFAULT;
      case ::blink::WebScreenOrientationLockPortraitPrimary:
        return ::device::mojom::ScreenOrientationLockType::PORTRAIT_PRIMARY;
      case ::blink::WebScreenOrientationLockPortraitSecondary:
        return ::device::mojom::ScreenOrientationLockType::PORTRAIT_SECONDARY;
      case ::blink::WebScreenOrientationLockLandscapePrimary:
        return ::device::mojom::ScreenOrientationLockType::LANDSCAPE_PRIMARY;
      case ::blink::WebScreenOrientationLockLandscapeSecondary:
        return ::device::mojom::ScreenOrientationLockType::LANDSCAPE_SECONDARY;
      case ::blink::WebScreenOrientationLockAny:
        return ::device::mojom::ScreenOrientationLockType::ANY;
      case ::blink::WebScreenOrientationLockLandscape:
        return ::device::mojom::ScreenOrientationLockType::LANDSCAPE;
      case ::blink::WebScreenOrientationLockPortrait:
        return ::device::mojom::ScreenOrientationLockType::PORTRAIT;
      case ::blink::WebScreenOrientationLockNatural:
        return ::device::mojom::ScreenOrientationLockType::NATURAL;
    }
    NOTREACHED();
    return ::device::mojom::ScreenOrientationLockType::DEFAULT;
  }

  static bool FromMojom(::device::mojom::ScreenOrientationLockType lockType,
                        ::blink::WebScreenOrientationLockType* out) {
    switch (lockType) {
      case ::device::mojom::ScreenOrientationLockType::DEFAULT:
        *out = ::blink::WebScreenOrientationLockDefault;
        return true;
      case ::device::mojom::ScreenOrientationLockType::PORTRAIT_PRIMARY:
        *out = ::blink::WebScreenOrientationLockPortraitPrimary;
        return true;
      case ::device::mojom::ScreenOrientationLockType::PORTRAIT_SECONDARY:
        *out = ::blink::WebScreenOrientationLockPortraitSecondary;
        return true;
      case ::device::mojom::ScreenOrientationLockType::LANDSCAPE_PRIMARY:
        *out = ::blink::WebScreenOrientationLockLandscapePrimary;
        return true;
      case ::device::mojom::ScreenOrientationLockType::LANDSCAPE_SECONDARY:
        *out = ::blink::WebScreenOrientationLockLandscapeSecondary;
        return true;
      case ::device::mojom::ScreenOrientationLockType::ANY:
        *out = ::blink::WebScreenOrientationLockAny;
        return true;
      case ::device::mojom::ScreenOrientationLockType::LANDSCAPE:
        *out = ::blink::WebScreenOrientationLockLandscape;
        return true;
      case ::device::mojom::ScreenOrientationLockType::PORTRAIT:
        *out = ::blink::WebScreenOrientationLockPortrait;
        return true;
      case ::device::mojom::ScreenOrientationLockType::NATURAL:
        *out = ::blink::WebScreenOrientationLockNatural;
        return true;
    }
    NOTREACHED();
    return false;
  }
};

}  // namespace mojo

#endif  // WebScreenOrientationEnumTraits_h
