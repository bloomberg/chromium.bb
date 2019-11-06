// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_FRAME_METADATA_UTIL_H_
#define CONTENT_BROWSER_RENDERER_HOST_FRAME_METADATA_UTIL_H_

#include "content/common/content_export.h"

namespace gfx {
class SizeF;
}

namespace content {

// Decides whether frame metadata corresponds to mobile-optimized content.
// By default returns |false|, except for the following cases:
// - page that has a width=device-width or narrower viewport
//   (indicating that this is a mobile-optimized or responsive web design);
// - page that prevents zooming in or out.
CONTENT_EXPORT bool IsMobileOptimizedFrame(
    float page_scale_factor,
    float min_page_scale_factor,
    float max_page_scale_factor,
    const gfx::SizeF& scrollable_viewport_size,
    const gfx::SizeF& root_layer_size);

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_FRAME_METADATA_UTIL_H_
