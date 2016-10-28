// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/mus/window_port_mus.h"

#include "ui/aura/client/aura_constants.h"
#include "ui/aura/mus/property_converter.h"
#include "ui/aura/mus/surface_id_handler.h"
#include "ui/aura/mus/window_tree_client.h"
#include "ui/aura/mus/window_tree_client_delegate.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/aura/window_property.h"

namespace aura {

WindowPortMus::WindowMusChangeDataImpl::WindowMusChangeDataImpl() = default;

WindowPortMus::WindowMusChangeDataImpl::~WindowMusChangeDataImpl() = default;

// static
WindowMus* WindowMus::Get(Window* window) {
  return WindowPortMus::Get(window);
}

WindowPortMus::WindowPortMus(WindowTreeClient* client,
                             bool create_remote_window)
    : WindowMus(create_remote_window), window_tree_client_(client) {}

WindowPortMus::~WindowPortMus() {
  if (surface_info_)
    SetSurfaceIdFromServer(nullptr);

  window_tree_client_->OnWindowMusDestroyed(this);
}

// static
WindowPortMus* WindowPortMus::Get(Window* window) {
  return static_cast<WindowPortMus*>(WindowPort::Get(window));
}

void WindowPortMus::SetTextInputState(mojo::TextInputStatePtr state) {
  window_tree_client_->SetWindowTextInputState(this, std::move(state));
}

void WindowPortMus::SetImeVisibility(bool visible,
                                     mojo::TextInputStatePtr state) {
  window_tree_client_->SetImeVisibility(this, visible, std::move(state));
}

void WindowPortMus::SetPredefinedCursor(ui::mojom::Cursor cursor_id) {
  window_tree_client_->SetPredefinedCursor(this, predefined_cursor_, cursor_id);
  predefined_cursor_ = cursor_id;
}

WindowPortMus::ServerChangeIdType WindowPortMus::ScheduleChange(
    const ServerChangeType type,
    const ServerChangeData& data) {
  ServerChange change;
  change.type = type;
  change.server_change_id = next_server_change_id_++;
  change.data = data;
  server_changes_.push_back(change);
  return change.server_change_id;
}

void WindowPortMus::RemoveChangeById(ServerChangeIdType change_id) {
  for (auto iter = server_changes_.rbegin(); iter != server_changes_.rend();
       ++iter) {
    if (iter->server_change_id == change_id) {
      server_changes_.erase(--(iter.base()));
      return;
    }
  }
}

bool WindowPortMus::RemoveChangeByTypeAndData(const ServerChangeType type,
                                              const ServerChangeData& data) {
  for (auto iter = server_changes_.begin(); iter != server_changes_.end();
       ++iter) {
    if (iter->type != type)
      continue;

    switch (type) {
      case ServerChangeType::ADD:
      case ServerChangeType::REMOVE:
      case ServerChangeType::REORDER:
        if (iter->data.child_id == data.child_id)
          break;
        continue;
      case ServerChangeType::BOUNDS:
        if (iter->data.bounds == data.bounds)
          break;
        continue;
      case ServerChangeType::PROPERTY:
        if (iter->data.property_name == data.property_name)
          break;
        continue;
      case ServerChangeType::VISIBLE:
        if (iter->data.visible == data.visible)
          break;
        continue;
    }
    server_changes_.erase(iter);
    return true;
  }
  return false;
}

PropertyConverter* WindowPortMus::GetPropertyConverter() {
  return window_tree_client_->delegate_->GetPropertyConverter();
}

Window* WindowPortMus::GetWindow() {
  return window_;
}

void WindowPortMus::AddChildFromServer(WindowMus* window) {
  DCHECK(has_server_window());
  ServerChangeData data;
  data.child_id = window->server_id();
  ScopedServerChange change(this, ServerChangeType::ADD, data);
  window_->AddChild(window->GetWindow());
}

void WindowPortMus::RemoveChildFromServer(WindowMus* child) {
  DCHECK(has_server_window());
  ServerChangeData data;
  data.child_id = child->server_id();
  ScopedServerChange change(this, ServerChangeType::REMOVE, data);
  window_->RemoveChild(child->GetWindow());
}

void WindowPortMus::ReorderFromServer(WindowMus* child,
                                      WindowMus* relative,
                                      ui::mojom::OrderDirection direction) {
  DCHECK(has_server_window());
  // Keying off solely the id isn't entirely accurate, in so far as if Window
  // does some other reordering then the server and client are out of sync.
  // But we assume only one client can make changes to a particular window at
  // a time, so this should be ok.
  ServerChangeData data;
  data.child_id = child->server_id();
  ScopedServerChange change(this, ServerChangeType::REORDER, data);
  if (direction == ui::mojom::OrderDirection::BELOW)
    window_->StackChildBelow(child->GetWindow(), relative->GetWindow());
  else
    window_->StackChildAbove(child->GetWindow(), relative->GetWindow());
}

void WindowPortMus::SetBoundsFromServer(const gfx::Rect& bounds) {
  DCHECK(has_server_window());
  ServerChangeData data;
  data.bounds = bounds;
  ScopedServerChange change(this, ServerChangeType::BOUNDS, data);
  window_->SetBounds(bounds);
}

void WindowPortMus::SetVisibleFromServer(bool visible) {
  DCHECK(has_server_window());
  ServerChangeData data;
  data.visible = visible;
  ScopedServerChange change(this, ServerChangeType::VISIBLE, data);
  if (visible)
    window_->Show();
  else
    window_->Hide();
}

void WindowPortMus::SetOpacityFromServer(float opacity) {
  // TODO(sky): this may not be necessary anymore.
  DCHECK(has_server_window());
  // Changes to opacity don't make it back to the server.
  window_->layer()->SetOpacity(opacity);
}

void WindowPortMus::SetPredefinedCursorFromServer(ui::mojom::Cursor cursor) {
  // As this does nothing more than set the cursor we don't need to use
  // ServerChange.
  predefined_cursor_ = cursor;
}

void WindowPortMus::SetPropertyFromServer(
    const std::string& property_name,
    const std::vector<uint8_t>* property_data) {
  ServerChangeData data;
  data.property_name = property_name;
  ScopedServerChange change(this, ServerChangeType::PROPERTY, data);
  GetPropertyConverter()->SetPropertyFromTransportValue(window_, property_name,
                                                        property_data);
}

void WindowPortMus::SetSurfaceIdFromServer(
    std::unique_ptr<SurfaceInfo> surface_info) {
  if (surface_info_) {
    const cc::SurfaceId& existing_surface_id = surface_info_->surface_id;
    cc::SurfaceId new_surface_id =
        surface_info ? surface_info->surface_id : cc::SurfaceId();
    if (!existing_surface_id.is_null() &&
        existing_surface_id != new_surface_id) {
      // Return the existing surface sequence.
      window_tree_client_->OnWindowMusSurfaceDetached(
          this, surface_info_->surface_sequence);
    }
  }
  WindowPortMus* parent = Get(window_->parent());
  if (parent && parent->surface_id_handler_) {
    parent->surface_id_handler_->OnChildWindowSurfaceChanged(window_,
                                                             &surface_info);
  }
  surface_info_ = std::move(surface_info);
}

std::unique_ptr<WindowMusChangeData>
WindowPortMus::PrepareForServerBoundsChange(const gfx::Rect& bounds) {
  std::unique_ptr<WindowMusChangeDataImpl> data(
      base::MakeUnique<WindowMusChangeDataImpl>());
  ServerChangeData change_data;
  change_data.bounds = bounds;
  data->change = base::MakeUnique<ScopedServerChange>(
      this, ServerChangeType::BOUNDS, change_data);
  return std::move(data);
}

std::unique_ptr<WindowMusChangeData>
WindowPortMus::PrepareForServerVisibilityChange(bool value) {
  std::unique_ptr<WindowMusChangeDataImpl> data(
      base::MakeUnique<WindowMusChangeDataImpl>());
  ServerChangeData change_data;
  change_data.visible = value;
  data->change = base::MakeUnique<ScopedServerChange>(
      this, ServerChangeType::VISIBLE, change_data);
  return std::move(data);
}

void WindowPortMus::NotifyEmbeddedAppDisconnected() {
  for (WindowObserver& observer : *GetObservers(window_))
    observer.OnEmbeddedAppDisconnected(window_);
}

std::unique_ptr<WindowPortInitData> WindowPortMus::OnPreInit(Window* window) {
  window_ = window;
  return window_tree_client_->OnWindowMusCreated(this);
}

void WindowPortMus::OnPostInit(std::unique_ptr<WindowPortInitData> init_data) {
  window_tree_client_->OnWindowMusInitDone(this, std::move(init_data));
}

void WindowPortMus::OnDeviceScaleFactorChanged(float device_scale_factor) {}

void WindowPortMus::OnWillAddChild(Window* child) {
  if (!has_server_window())
    return;

  ServerChangeData change_data;
  change_data.child_id = Get(child)->server_id();
  if (!RemoveChangeByTypeAndData(ServerChangeType::ADD, change_data))
    window_tree_client_->OnWindowMusAddChild(this, Get(child));
}

void WindowPortMus::OnWillRemoveChild(Window* child) {
  if (!has_server_window())
    return;

  ServerChangeData change_data;
  change_data.child_id = Get(child)->server_id();
  if (!RemoveChangeByTypeAndData(ServerChangeType::REMOVE, change_data))
    window_tree_client_->OnWindowMusRemoveChild(this, Get(child));
}

void WindowPortMus::OnWillMoveChild(size_t current_index, size_t dest_index) {
  if (!has_server_window())
    return;

  ServerChangeData change_data;
  change_data.child_id = Get(window_->children()[current_index])->server_id();
  if (!RemoveChangeByTypeAndData(ServerChangeType::REORDER, change_data))
    window_tree_client_->OnWindowMusMoveChild(this, current_index, dest_index);
}

void WindowPortMus::OnVisibilityChanged(bool visible) {
  if (!has_server_window())
    return;

  ServerChangeData change_data;
  change_data.visible = visible;
  if (!RemoveChangeByTypeAndData(ServerChangeType::VISIBLE, change_data))
    window_tree_client_->OnWindowMusSetVisible(this, visible);
}

void WindowPortMus::OnDidChangeBounds(const gfx::Rect& old_bounds,
                                      const gfx::Rect& new_bounds) {
  if (!has_server_window())
    return;

  ServerChangeData change_data;
  change_data.bounds = new_bounds;
  if (!RemoveChangeByTypeAndData(ServerChangeType::BOUNDS, change_data))
    window_tree_client_->OnWindowMusBoundsChanged(this, old_bounds, new_bounds);
}

std::unique_ptr<WindowPortPropertyData> WindowPortMus::OnWillChangeProperty(
    const void* key) {
  if (!has_server_window())
    return nullptr;

  return window_tree_client_->OnWindowMusWillChangeProperty(this, key);
}

void WindowPortMus::OnPropertyChanged(
    const void* key,
    std::unique_ptr<WindowPortPropertyData> data) {
  if (!has_server_window())
    return;

  ServerChangeData change_data;
  change_data.property_name =
      GetPropertyConverter()->GetTransportNameForPropertyKey(key);
  // TODO(sky): investigate to see if we need to compare data. In particular do
  // we ever have a case where changing a property cascades into changing the
  // same property?
  if (!RemoveChangeByTypeAndData(ServerChangeType::PROPERTY, change_data))
    window_tree_client_->OnWindowMusPropertyChanged(this, key, std::move(data));
}

}  // namespace aura
