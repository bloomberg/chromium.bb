// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/clone_layer.h"

#include "ui/compositor/layer.h"
#include "ui/compositor/layer_delegate.h"
#include "ui/compositor/layer_owner.h"

namespace ui {

scoped_ptr<ui::Layer> CloneLayer(LayerOwner* layer_owner) {
  scoped_ptr<ui::Layer> old_layer(layer_owner->AcquireLayer());
  if (!old_layer)
    return old_layer.Pass();

  LayerDelegate* old_delegate = old_layer->delegate();
  old_layer->set_delegate(NULL);

  const gfx::Rect layer_bounds(old_layer->bounds());
  Layer* new_layer = new ui::Layer(old_layer->type());
  layer_owner->SetLayer(new_layer);
  new_layer->SetVisible(old_layer->visible());
  new_layer->set_scale_content(old_layer->scale_content());
  new_layer->SetBounds(layer_bounds);
  new_layer->SetMasksToBounds(old_layer->GetMasksToBounds());
  new_layer->set_name(old_layer->name());
  new_layer->SetFillsBoundsOpaquely(old_layer->fills_bounds_opaquely());

  // Install new layer as a sibling of the old layer, stacked below it.
  if (old_layer->parent()) {
    old_layer->parent()->Add(new_layer);
    old_layer->parent()->StackBelow(new_layer, old_layer.get());
  }

  // Migrate all the child layers over to the new layer. Copy the list because
  // the items are removed during iteration.
  std::vector<ui::Layer*> children_copy = old_layer->children();
  for (std::vector<ui::Layer*>::const_iterator it = children_copy.begin();
       it != children_copy.end();
       ++it) {
    ui::Layer* child = *it;
    new_layer->Add(child);
  }

  // Install the delegate last so that the delegate isn't notified as we copy
  // state to the new layer.
  new_layer->set_delegate(old_delegate);

  return old_layer.Pass();
}

}  // namespace ui
