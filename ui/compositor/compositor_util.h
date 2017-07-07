// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_COMPOSITOR_UTIL_H_
#define UI_COMPOSITOR_COMPOSITOR_UTIL_H_

#include <stdint.h>

#include "cc/output/buffer_to_texture_target_map.h"
#include "ui/compositor/compositor_export.h"

namespace cc {
class RendererSettings;
}

namespace ui {

COMPOSITOR_EXPORT cc::RendererSettings CreateRendererSettings(
    const cc::BufferToTextureTargetMap& image_targets);

}  // namespace ui

#endif  // UI_COMPOSITOR_COMPOSITOR_UTIL_H_
