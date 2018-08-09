// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws2/focus_handler.h"

#include "services/ui/ws2/client_change.h"
#include "services/ui/ws2/client_change_tracker.h"
#include "services/ui/ws2/server_window.h"
#include "services/ui/ws2/window_properties.h"
#include "services/ui/ws2/window_service.h"
#include "services/ui/ws2/window_service_delegate.h"
#include "services/ui/ws2/window_tree.h"
#include "ui/aura/client/focus_client.h"

namespace ui {
namespace ws2 {

FocusHandler::FocusHandler(WindowTree* window_tree)
    : window_tree_(window_tree) {
  window_tree_->window_service_->focus_client()->AddObserver(this);
}

FocusHandler::~FocusHandler() {
  window_tree_->window_service_->focus_client()->RemoveObserver(this);
}

bool FocusHandler::SetFocus(aura::Window* window) {
  if (window && !IsFocusableWindow(window)) {
    DVLOG(1) << "SetFocus failed (access denied or invalid window)";
    return false;
  }

  aura::client::FocusClient* focus_client =
      window_tree_->window_service_->focus_client();
  ServerWindow* server_window = ServerWindow::GetMayBeNull(window);
  if (window == focus_client->GetFocusedWindow()) {
    if (!window)
      return true;

    if (server_window->focus_owner() != window_tree_) {
      // The focused window didn't change, but the client that owns focus did
      // (see |ServerWindow::focus_owner_| for details on this). Notify the
      // current owner that it lost focus.
      if (server_window->focus_owner()) {
        server_window->focus_owner()->window_tree_client_->OnWindowFocused(
            kInvalidTransportId);
      }
      server_window->set_focus_owner(window_tree_);
    }
    return true;
  }

  // The client is asking to remove focus from a window. This is typically a
  // side effect of the window becoming, or about to become, an unfocusable
  // Window (for example, the Window is hiding). Windows becoming unfocusable is
  // handled locally. Assume the request is for such a scenario and return
  // true. Returning false means the client will attempt to revert to the
  // previously focused window, which may cause unexpected activation changes.
  //
  // To process null requests conflicts with top-level activation changes. For
  // example, the typical sequence when a window is hidden is to first remove
  // focus, and then hide the window. FocusController keys off window hiding to
  // move activation. If this code were to set focus to null, FocusController
  // would not see the window hiding (because the active window was set to null)
  // and not automatically activate the next window.
  //
  // Another possibility for this code is to handle null as a signal to move
  // focus to the active window (if there is one). I'm going with the simpler
  // approach for now.
  if (!window)
    return true;

  ClientChange change(window_tree_->property_change_tracker_.get(), window,
                      ClientChangeType::kFocus);
  focus_client->FocusWindow(window);
  if (focus_client->GetFocusedWindow() != window) {
    DVLOG(1) << "SetFocus failed (FocusClient::FocusWindow call failed)";
    return false;
  }

  if (server_window)
    server_window->set_focus_owner(window_tree_);
  return true;
}

void FocusHandler::SetCanFocus(aura::Window* window, bool can_focus) {
  if (window && (window_tree_->IsClientCreatedWindow(window) ||
                 window_tree_->IsClientRootWindow(window))) {
    window->SetProperty(kCanFocus, can_focus);
  } else {
    DVLOG(1) << "SetCanFocus failed (invalid or unknown window)";
  }
}

bool FocusHandler::IsFocusableWindow(aura::Window* window) const {
  if (!window)
    return true;  // Used to clear focus.

  if (!window->IsVisible() || !window->GetRootWindow())
    return false;  // The window must be drawn and attached to a root.

  return (window_tree_->IsClientCreatedWindow(window) ||
          window_tree_->IsClientRootWindow(window));
}

bool FocusHandler::IsEmbeddedClient(ServerWindow* server_window) const {
  return server_window->embedded_window_tree() == window_tree_;
}

bool FocusHandler::IsOwningClient(ServerWindow* server_window) const {
  return server_window->owning_window_tree() == window_tree_;
}

void FocusHandler::OnWindowFocused(aura::Window* gained_focus,
                                   aura::Window* lost_focus) {
  ClientChangeTracker* change_tracker =
      window_tree_->property_change_tracker_.get();
  if (change_tracker->IsProcessingChangeForWindow(lost_focus,
                                                  ClientChangeType::kFocus) ||
      change_tracker->IsProcessingChangeForWindow(gained_focus,
                                                  ClientChangeType::kFocus)) {
    // The client initiated the change, don't notify the client.
    return;
  }

  // The client did not request the focus change. Update state appropriately.
  // Prefer the embedded client over the owning client.
  bool notified_gained = false;
  if (gained_focus) {
    ServerWindow* server_window = ServerWindow::GetMayBeNull(gained_focus);
    if (server_window && (IsEmbeddedClient(server_window) ||
                          (!server_window->embedded_window_tree() &&
                           IsOwningClient(server_window)))) {
      server_window->set_focus_owner(window_tree_);
      window_tree_->window_tree_client_->OnWindowFocused(
          window_tree_->TransportIdForWindow(gained_focus));
      notified_gained = true;
    }
  }

  if (lost_focus && !notified_gained) {
    ServerWindow* server_window = ServerWindow::GetMayBeNull(lost_focus);
    if (server_window && server_window->focus_owner() == window_tree_) {
      server_window->set_focus_owner(nullptr);
      window_tree_->window_tree_client_->OnWindowFocused(kInvalidTransportId);
    }
  }
}

}  // namespace ws2
}  // namespace ui
