// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/corewm/window_util.h"

#include "ui/aura/client/activation_client.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace {

// Helper method for RecreateWindowLayers() which adds all the existing layers
// for |view| and its descendants to |parent_layer|. New layers are created for
// |view| (if it previously had a layer) and any descendants which previously
// had layers. The new layers are blank, so nothing has been painted to them
// yet. Returns true if this method added at least one layer to |parent_layer|.
bool RecreateViewLayers(ui::Layer* parent_layer, views::View* view) {
  bool recreated_layer = false;
  if (view->layer()) {
    ui::Layer* layer = view->RecreateLayer();
    if (layer) {
      layer->SuppressPaint();
      parent_layer->Add(layer);
      parent_layer = layer;
      recreated_layer = true;
    }
  }
  for (int i = 0; i < view->child_count(); ++i)
    recreated_layer |= RecreateViewLayers(parent_layer, view->child_at(i));

  return recreated_layer;
}

}  // namespace

namespace views {
namespace corewm {

void ActivateWindow(aura::Window* window) {
  DCHECK(window);
  DCHECK(window->GetRootWindow());
  aura::client::GetActivationClient(window->GetRootWindow())->ActivateWindow(
      window);
}

void DeactivateWindow(aura::Window* window) {
  DCHECK(window);
  DCHECK(window->GetRootWindow());
  aura::client::GetActivationClient(window->GetRootWindow())->DeactivateWindow(
      window);
}

bool IsActiveWindow(aura::Window* window) {
  DCHECK(window);
  if (!window->GetRootWindow())
    return false;
  aura::client::ActivationClient* client =
      aura::client::GetActivationClient(window->GetRootWindow());
  return client && client->GetActiveWindow() == window;
}

bool CanActivateWindow(aura::Window* window) {
  DCHECK(window);
  if (!window->GetRootWindow())
    return false;
  aura::client::ActivationClient* client =
      aura::client::GetActivationClient(window->GetRootWindow());
  return client && client->CanActivateWindow(window);
}

aura::Window* GetActivatableWindow(aura::Window* window) {
  aura::client::ActivationClient* client =
      aura::client::GetActivationClient(window->GetRootWindow());
  return client ? client->GetActivatableWindow(window) : NULL;
}

aura::Window* GetToplevelWindow(aura::Window* window) {
  aura::client::ActivationClient* client =
      aura::client::GetActivationClient(window->GetRootWindow());
  return client ? client->GetToplevelWindow(window) : NULL;
}

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

  // Cache the order of |window|'s child layers. If |window| belongs to a widget
  // and the widget has both child windows and child views with layers,
  // |initial_layer_order| is used to determine the relative order.
  std::vector<ui::Layer*> initial_layer_order = window->layer()->children();

  // Recreate the layers for any child windows of |window|.
  for (aura::Window::Windows::const_iterator it = window->children().begin();
       it != window->children().end();
       ++it) {
    old_layer->Add(RecreateWindowLayers(*it, set_bounds));
  }

  // Recreate the layers for any child views of |widget|.
  bool has_view_layers = false;
  views::Widget* widget = views::Widget::GetWidgetForNativeView(window);
  if (widget && widget->GetRootView())
    has_view_layers = RecreateViewLayers(old_layer, widget->GetRootView());

  if (has_view_layers && !window->children().empty()) {
    // RecreateViewLayers() added the view layers above the window layers in
    // z-order. The window layers and the view layers may have been originally
    // intermingled. Reorder |old_layer|'s children based on the initial
    // order.
    for (size_t i = 0; i < initial_layer_order.size(); ++i) {
      ui::Layer* layer = initial_layer_order[i];
      std::vector<ui::Layer*>::const_iterator it = std::find(
          old_layer->children().begin(), old_layer->children().end(), layer);
      if (it != old_layer->children().end())
        old_layer->StackAtTop(layer);
    }
  }

  if (set_bounds)
    window->SetBounds(bounds);
  return old_layer;
}

}  // namespace corewm
}  // namespace views
