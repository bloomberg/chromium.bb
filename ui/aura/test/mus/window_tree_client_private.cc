// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/test/mus/window_tree_client_private.h"

#include "ui/aura/mus/window_port_mus.h"
#include "ui/aura/mus/window_tree_client.h"
#include "ui/aura/window.h"
#include "ui/display/display.h"

namespace aura {

WindowTreeClientPrivate::WindowTreeClientPrivate(
    WindowTreeClient* tree_client_impl)
    : tree_client_impl_(tree_client_impl) {}

WindowTreeClientPrivate::WindowTreeClientPrivate(Window* window)
    : WindowTreeClientPrivate(WindowPortMus::Get(window)->window_tree_client_) {
}

WindowTreeClientPrivate::~WindowTreeClientPrivate() {}

void WindowTreeClientPrivate::OnEmbed(ui::mojom::WindowTree* window_tree) {
  ui::mojom::WindowDataPtr root_data(ui::mojom::WindowData::New());
  root_data->parent_id = 0;
  root_data->window_id = next_window_id_++;
  root_data->visible = true;
  const int64_t display_id = 1;
  const Id focused_window_id = 0;
  tree_client_impl_->OnEmbedImpl(window_tree, 1, std::move(root_data),
                                 display_id, focused_window_id, true);
}

WindowTreeHostMus* WindowTreeClientPrivate::CallWmNewDisplayAdded(
    const display::Display& display) {
  ui::mojom::WindowDataPtr root_data(ui::mojom::WindowData::New());
  root_data->parent_id = 0;
  root_data->window_id = next_window_id_++;
  root_data->visible = true;
  root_data->bounds = gfx::Rect(display.bounds().size());
  const bool parent_drawn = true;
  return CallWmNewDisplayAdded(display, std::move(root_data), parent_drawn);
}

WindowTreeHostMus* WindowTreeClientPrivate::CallWmNewDisplayAdded(
    const display::Display& display,
    ui::mojom::WindowDataPtr root_data,
    bool parent_drawn) {
  return tree_client_impl_->WmNewDisplayAddedImpl(display, std::move(root_data),
                                                  parent_drawn);
}

void WindowTreeClientPrivate::CallOnWindowInputEvent(
    Window* window,
    std::unique_ptr<ui::Event> event) {
  const uint32_t event_id = 0u;
  const uint32_t observer_id = 0u;
  tree_client_impl_->OnWindowInputEvent(event_id,
                                        WindowPortMus::Get(window)->server_id(),
                                        std::move(event), observer_id);
}

void WindowTreeClientPrivate::CallOnCaptureChanged(Window* new_capture,
                                                   Window* old_capture) {
  tree_client_impl_->OnCaptureChanged(
      new_capture ? WindowPortMus::Get(new_capture)->server_id() : 0,
      old_capture ? WindowPortMus::Get(old_capture)->server_id() : 0);
}

void WindowTreeClientPrivate::SetTreeAndClientId(
    ui::mojom::WindowTree* window_tree,
    ClientSpecificId client_id) {
  tree_client_impl_->WindowTreeConnectionEstablished(window_tree);
  tree_client_impl_->client_id_ = client_id;
}

bool WindowTreeClientPrivate::HasPointerWatcher() {
  return tree_client_impl_->has_pointer_watcher_;
}

}  // namespace aura
