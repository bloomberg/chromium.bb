// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/imagecapture/PhotoCapabilities.h"

namespace blink {

// static
PhotoCapabilities* PhotoCapabilities::create() {
  return new PhotoCapabilities();
}

String PhotoCapabilities::fillLightMode() const {
  switch (m_fillLightMode) {
    case media::mojom::blink::FillLightMode::NONE:
      return "none";
    case media::mojom::blink::FillLightMode::OFF:
      return "off";
    case media::mojom::blink::FillLightMode::AUTO:
      return "auto";
    case media::mojom::blink::FillLightMode::FLASH:
      return "flash";
    case media::mojom::blink::FillLightMode::TORCH:
      return "torch";
    default:
      NOTREACHED();
  }
  return emptyString;
}

DEFINE_TRACE(PhotoCapabilities) {
  visitor->trace(m_imageHeight);
  visitor->trace(m_imageWidth);
}

}  // namespace blink
