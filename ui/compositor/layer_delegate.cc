// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/layer_delegate.h"

namespace ui {

void LayerDelegate::OnLayerBoundsChanged(const gfx::Rect& old_bounds) {}

void LayerDelegate::OnLayerOpacityChanged(float old_opacity,
                                          float new_opacity) {}

}  // namespace ui
