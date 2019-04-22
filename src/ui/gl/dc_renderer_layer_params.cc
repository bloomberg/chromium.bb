// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/dc_renderer_layer_params.h"

#include "ui/gl/gl_image.h"

namespace ui {

DCRendererLayerParams::DCRendererLayerParams() = default;
DCRendererLayerParams::DCRendererLayerParams(
    const DCRendererLayerParams& other) = default;
DCRendererLayerParams& DCRendererLayerParams::operator=(
    const DCRendererLayerParams& other) = default;
DCRendererLayerParams::~DCRendererLayerParams() = default;

}  // namespace ui
