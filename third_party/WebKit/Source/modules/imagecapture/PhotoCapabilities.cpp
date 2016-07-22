// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/imagecapture/PhotoCapabilities.h"

namespace blink {

// static
PhotoCapabilities* PhotoCapabilities::create()
{
    return new PhotoCapabilities();
}

String PhotoCapabilities::focusMode() const
{
    switch (m_focusMode) {
    case media::mojom::blink::FocusMode::UNAVAILABLE:
        return "unavailable";
    case media::mojom::blink::FocusMode::AUTO:
        return "auto";
    case media::mojom::blink::FocusMode::MANUAL:
        return "manual";
    default:
        NOTREACHED();
    }
    return emptyString();
}

DEFINE_TRACE(PhotoCapabilities)
{
    visitor->trace(m_iso);
    visitor->trace(m_imageHeight);
    visitor->trace(m_imageWidth);
    visitor->trace(m_zoom);
}

} // namespace blink
