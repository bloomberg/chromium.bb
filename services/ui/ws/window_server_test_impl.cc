// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws/window_server_test_impl.h"

#include "services/ui/public/interfaces/window_tree.mojom.h"
#include "services/ui/ws/display.h"
#include "services/ui/ws/display_manager.h"
#include "services/ui/ws/server_window.h"
#include "services/ui/ws/window_server.h"
#include "services/ui/ws/window_tree.h"

namespace ui {
namespace ws {

WindowServerTestImpl::WindowServerTestImpl(WindowServer* window_server)
    : window_server_(window_server) {}

WindowServerTestImpl::~WindowServerTestImpl() {}

void WindowServerTestImpl::OnWindowPaint(
    const std::string& name,
    const EnsureClientHasDrawnWindowCallback& cb,
    ServerWindow* window) {
  WindowTree* tree = window_server_->GetTreeWithClientName(name);
  if (!tree)
    return;
  if (tree->HasRoot(window) && window->has_created_compositor_frame_sink()) {
    cb.Run(true);
    window_server_->SetPaintCallback(base::Callback<void(ServerWindow*)>());
  }
}

void WindowServerTestImpl::EnsureClientHasDrawnWindow(
    const std::string& client_name,
    const EnsureClientHasDrawnWindowCallback& callback) {
  WindowTree* tree = window_server_->GetTreeWithClientName(client_name);
  if (tree) {
    for (const ServerWindow* window : tree->roots()) {
      if (window->has_created_compositor_frame_sink()) {
        callback.Run(true);
        return;
      }
    }
  }

  window_server_->SetPaintCallback(
      base::Bind(&WindowServerTestImpl::OnWindowPaint, base::Unretained(this),
                 client_name, std::move(callback)));
}

void WindowServerTestImpl::DispatchEvent(int64_t display_id,
                                         std::unique_ptr<ui::Event> event,
                                         const DispatchEventCallback& cb) {
  DisplayManager* manager = window_server_->display_manager();
  if (!manager) {
    DVLOG(1) << "No display manager in DispatchEvent.";
    cb.Run(false);
    return;
  }

  Display* display = manager->GetDisplayById(display_id);
  if (!display) {
    DVLOG(1) << "Invalid display_id in DispatchEvent.";
    cb.Run(false);
    return;
  }

  ignore_result(static_cast<PlatformDisplayDelegate*>(display)
                    ->GetEventSink()
                    ->OnEventFromSource(event.get()));
  cb.Run(true);
}

}  // namespace ws
}  // namespace ui
