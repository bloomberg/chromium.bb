// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_COMPOSITOR_UTIL_H_
#define UI_COMPOSITOR_COMPOSITOR_UTIL_H_

#include <stdint.h>

#include "components/viz/common/resources/buffer_to_texture_target_map.h"
#include "ui/compositor/compositor_export.h"

namespace viz {
class RendererSettings;
class ResourceSettings;
}

namespace ui {

// |image_targets| is a map from every supported pair of GPU memory buffer
// usage/format to its GL texture target.
COMPOSITOR_EXPORT viz::ResourceSettings CreateResourceSettings(
    const viz::BufferToTextureTargetMap& image_targets);

COMPOSITOR_EXPORT viz::RendererSettings CreateRendererSettings(
    const viz::BufferToTextureTargetMap& image_targets);

}  // namespace ui

#endif  // UI_COMPOSITOR_COMPOSITOR_UTIL_H_
