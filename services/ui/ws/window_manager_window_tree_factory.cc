// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws/window_manager_window_tree_factory.h"

#include "base/bind.h"
#include "services/ui/ws/display_creation_config.h"
#include "services/ui/ws/window_manager_window_tree_factory_set.h"
#include "services/ui/ws/window_server.h"
#include "services/ui/ws/window_server_delegate.h"
#include "services/ui/ws/window_tree.h"

namespace ui {
namespace ws {

WindowManagerWindowTreeFactory::WindowManagerWindowTreeFactory(
    WindowManagerWindowTreeFactorySet* window_manager_window_tree_factory_set,
    const UserId& user_id,
    mojo::InterfaceRequest<mojom::WindowManagerWindowTreeFactory> request)
    : WindowManagerWindowTreeFactory(window_manager_window_tree_factory_set,
                                     user_id) {
  if (request.is_pending())
    binding_.Bind(std::move(request));
}

WindowManagerWindowTreeFactory::~WindowManagerWindowTreeFactory() {}

void WindowManagerWindowTreeFactory::CreateWindowTree(
    mojom::WindowTreeRequest window_tree_request,
    mojom::WindowTreeClientPtr window_tree_client,
    bool automatically_create_display_roots) {
  if (window_tree_) {
    DVLOG(1) << "CreateWindowTree() called more than once.";
    return;
  }

  // CreateWindowTree() can only be called once, so there is no reason to keep
  // the binding around.
  if (binding_.is_bound())
    binding_.Close();

  // If the config is MANUAL, then all WindowManagers must connect as MANUAL.
  if (!automatically_create_display_roots &&
      GetWindowServer()->display_creation_config() ==
          DisplayCreationConfig::AUTOMATIC) {
    DVLOG(1) << "CreateWindowTree() called with manual and automatic.";
    return;
  }

  SetWindowTree(GetWindowServer()->CreateTreeForWindowManager(
      user_id_, std::move(window_tree_request), std::move(window_tree_client),
      automatically_create_display_roots));
}

WindowManagerWindowTreeFactory::WindowManagerWindowTreeFactory(
    WindowManagerWindowTreeFactorySet* window_manager_window_tree_factory_set,
    const UserId& user_id)
    : window_manager_window_tree_factory_set_(
          window_manager_window_tree_factory_set),
      user_id_(user_id),
      binding_(this) {}

WindowServer* WindowManagerWindowTreeFactory::GetWindowServer() {
  return window_manager_window_tree_factory_set_->window_server();
}

void WindowManagerWindowTreeFactory::SetWindowTree(WindowTree* window_tree) {
  DCHECK(!window_tree_);
  window_tree_ = window_tree;

  window_manager_window_tree_factory_set_
      ->OnWindowManagerWindowTreeFactoryReady(this);
}

}  // namespace ws
}  // namespace ui
