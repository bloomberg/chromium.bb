// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_CLONE_LAYER_H_
#define UI_COMPOSITOR_CLONE_LAYER_H_

#include "base/memory/scoped_ptr.h"
#include "ui/compositor/compositor_export.h"

namespace ui {

class Layer;
class LayerOwner;

// Creates a new Layer for |layer_owner| returning the old. This does NOT
// recurse. All the existing children of the layer are moved to the new layer.
// This does nothing if |layer_owner| currently has no layer.
COMPOSITOR_EXPORT scoped_ptr<ui::Layer> CloneLayer(LayerOwner* layer_owner);

}  // namespace ui

#endif  // UI_COMPOSITOR_CLONE_LAYER_H_
