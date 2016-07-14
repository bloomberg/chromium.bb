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

MediaSettingsRange* PhotoCapabilities::zoom() const { return m_zoom; }

void PhotoCapabilities::setZoom(MediaSettingsRange* value) { m_zoom = value; }

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

void PhotoCapabilities::setFocusMode(media::mojom::blink::FocusMode focusMode)
{
    m_focusMode = focusMode;
}

DEFINE_TRACE(PhotoCapabilities)
{
    visitor->trace(m_zoom);
}

} // namespace blink
