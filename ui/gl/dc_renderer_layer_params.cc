// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/dc_renderer_layer_params.h"

namespace ui {

DCRendererLayerParams::DCRendererLayerParams(bool is_clipped,
                                             const gfx::Rect clip_rect,
                                             unsigned sorting_context_id,
                                             const gfx::Transform& transform,
                                             gl::GLImage* image,
                                             const gfx::RectF& contents_rect,
                                             const gfx::Rect& rect,
                                             unsigned background_color,
                                             unsigned edge_aa_mask,
                                             float opacity,
                                             unsigned filter)
    : is_clipped(is_clipped),
      clip_rect(clip_rect),
      sorting_context_id(sorting_context_id),
      transform(transform),
      image(image),
      contents_rect(contents_rect),
      rect(rect),
      background_color(background_color),
      edge_aa_mask(edge_aa_mask),
      opacity(opacity),
      filter(filter) {}

DCRendererLayerParams::DCRendererLayerParams(
    const DCRendererLayerParams& other) = default;
DCRendererLayerParams::~DCRendererLayerParams() = default;

}  // namespace ui
