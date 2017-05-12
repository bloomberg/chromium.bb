// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_COMPOSITOR_UTIL_H_
#define UI_COMPOSITOR_COMPOSITOR_UTIL_H_

#include <stdint.h>

#include "ui/compositor/compositor_export.h"
#include "ui/gfx/buffer_types.h"

namespace cc {
class RendererSettings;
}

namespace ui {

COMPOSITOR_EXPORT cc::RendererSettings CreateRendererSettings(uint32_t (
    *get_texture_target)(gfx::BufferFormat format, gfx::BufferUsage usage));

}  // namespace ui

#endif  // UI_COMPOSITOR_COMPOSITOR_UTIL_H_
