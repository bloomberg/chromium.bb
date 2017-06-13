// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebDisplayModeStructTraits_h
#define WebDisplayModeStructTraits_h

#include "mojo/public/cpp/bindings/enum_traits.h"
#include "third_party/WebKit/public/platform/WebDisplayMode.h"
#include "third_party/WebKit/public/platform/display_mode.mojom-shared.h"

namespace mojo {

template <>
struct EnumTraits<blink::mojom::DisplayMode, blink::WebDisplayMode> {
  static blink::mojom::DisplayMode ToMojom(blink::WebDisplayMode input) {
    switch (input) {
      case blink::kWebDisplayModeUndefined:
        return blink::mojom::DisplayMode::UNDEFINED;
      case blink::kWebDisplayModeBrowser:
        return blink::mojom::DisplayMode::BROWSER;
      case blink::kWebDisplayModeMinimalUi:
        return blink::mojom::DisplayMode::MINIMAL_UI;
      case blink::kWebDisplayModeStandalone:
        return blink::mojom::DisplayMode::STANDALONE;
      case blink::kWebDisplayModeFullscreen:
        return blink::mojom::DisplayMode::FULLSCREEN;
    }
    NOTREACHED();
    return blink::mojom::DisplayMode::UNDEFINED;
  }

  static bool FromMojom(blink::mojom::DisplayMode input,
                        blink::WebDisplayMode* output) {
    switch (input) {
      case blink::mojom::DisplayMode::UNDEFINED:
        *output = blink::kWebDisplayModeUndefined;
        return true;
      case blink::mojom::DisplayMode::BROWSER:
        *output = blink::kWebDisplayModeBrowser;
        return true;
      case blink::mojom::DisplayMode::MINIMAL_UI:
        *output = blink::kWebDisplayModeMinimalUi;
        return true;
      case blink::mojom::DisplayMode::STANDALONE:
        *output = blink::kWebDisplayModeStandalone;
        return true;
      case blink::mojom::DisplayMode::FULLSCREEN:
        *output = blink::kWebDisplayModeFullscreen;
        return true;
    }
    return false;
  }
};

}  // namespace mojo

#endif  // WebDisplayModeStructTraits_h
