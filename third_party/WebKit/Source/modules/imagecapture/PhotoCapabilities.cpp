// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/imagecapture/PhotoCapabilities.h"

namespace blink {

// static
PhotoCapabilities* PhotoCapabilities::create() {
  return new PhotoCapabilities();
}

Vector<String> PhotoCapabilities::fillLightMode() const {
  Vector<String> fillLightModes;
  for (const auto& mode : m_fillLightModes) {
    switch (mode) {
      case media::mojom::blink::FillLightMode::OFF:
        fillLightModes.push_back("off");
        break;
      case media::mojom::blink::FillLightMode::AUTO:
        fillLightModes.push_back("auto");
        break;
      case media::mojom::blink::FillLightMode::FLASH:
        fillLightModes.push_back("flash");
        break;
      default:
        NOTREACHED();
    }
  }
  return fillLightModes;
}

String PhotoCapabilities::redEyeReduction() const {
  return m_redEyeReduction ? "controllable" : "never";
}

DEFINE_TRACE(PhotoCapabilities) {
  visitor->trace(m_imageHeight);
  visitor->trace(m_imageWidth);
}

}  // namespace blink
