// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/corewm/window_util.h"

#include "ui/aura/window.h"
#include "ui/compositor/layer.h"

namespace views {
namespace corewm {

void DeepDeleteLayers(ui::Layer* layer) {
  std::vector<ui::Layer*> children = layer->children();
  for (std::vector<ui::Layer*>::const_iterator it = children.begin();
       it != children.end();
       ++it) {
    ui::Layer* child = *it;
    DeepDeleteLayers(child);
  }
  delete layer;
}

ui::Layer* RecreateWindowLayers(aura::Window* window, bool set_bounds) {
  const gfx::Rect bounds = window->bounds();
  ui::Layer* old_layer = window->RecreateLayer();
  DCHECK(old_layer);
  for (aura::Window::Windows::const_iterator it = window->children().begin();
       it != window->children().end();
       ++it) {
    // Maintain the hierarchy of the detached layers.
    old_layer->Add(RecreateWindowLayers(*it, set_bounds));
  }
  if (set_bounds)
    window->SetBounds(bounds);
  return old_layer;
}

}  // namespace corewm
}  // namespace views
