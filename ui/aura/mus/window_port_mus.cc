// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/mus/window_port_mus.h"

#include "base/memory/ptr_util.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/transient_window_client.h"
#include "ui/aura/mus/client_surface_embedder.h"
#include "ui/aura/mus/property_converter.h"
#include "ui/aura/mus/window_tree_client.h"
#include "ui/aura/mus/window_tree_client_delegate.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_observer.h"
#include "ui/base/class_property.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

namespace aura {

namespace {
// Helper function to get the device_scale_factor() of the display::Display
// nearest to |window|.
float ScaleFactorForDisplay(Window* window) {
  return display::Screen::GetScreen()
      ->GetDisplayNearestWindow(window)
      .device_scale_factor();
}
}  // namespace

WindowPortMus::WindowMusChangeDataImpl::WindowMusChangeDataImpl() = default;

WindowPortMus::WindowMusChangeDataImpl::~WindowMusChangeDataImpl() = default;

// static
WindowMus* WindowMus::Get(Window* window) {
  return WindowPortMus::Get(window);
}

WindowPortMus::WindowPortMus(WindowTreeClient* client,
                             WindowMusType window_mus_type)
    : WindowMus(window_mus_type), window_tree_client_(client) {}

WindowPortMus::~WindowPortMus() {
  client_surface_embedder_.reset();

  // DESTROY is only scheduled from DestroyFromServer(), meaning if DESTROY is
  // present then the server originated the change.
  const WindowTreeClient::Origin origin =
      RemoveChangeByTypeAndData(ServerChangeType::DESTROY, ServerChangeData())
          ? WindowTreeClient::Origin::SERVER
          : WindowTreeClient::Origin::CLIENT;
  window_tree_client_->OnWindowMusDestroyed(this, origin);
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

void WindowPortMus::SetCursor(const ui::CursorData& cursor) {
  if (cursor_.IsSameAs(cursor))
    return;

  window_tree_client_->SetCursor(this, cursor_, cursor);
  cursor_ = cursor;
}

void WindowPortMus::SetEventTargetingPolicy(
    ui::mojom::EventTargetingPolicy policy) {
  window_tree_client_->SetEventTargetingPolicy(this, policy);
}

void WindowPortMus::SetCanAcceptDrops(bool can_accept_drops) {
  window_tree_client_->SetCanAcceptDrops(this, can_accept_drops);
}

void WindowPortMus::Embed(
    ui::mojom::WindowTreeClientPtr client,
    uint32_t flags,
    const ui::mojom::WindowTree::EmbedCallback& callback) {
  window_tree_client_->Embed(window_, std::move(client), flags, callback);
}

void WindowPortMus::RequestCompositorFrameSink(
    scoped_refptr<cc::ContextProvider> context_provider,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
    const CompositorFrameSinkCallback& callback) {
  DCHECK(pending_compositor_frame_sink_request_.is_null());
  // If we haven't received a FrameSinkId from the window server yet then we
  // bind the parameters to a closure that will be called once the FrameSinkId
  // is available.
  if (!frame_sink_id_.is_valid()) {
    pending_compositor_frame_sink_request_ =
        base::Bind(&WindowPortMus::RequestCompositorFrameSinkInternal,
                   base::Unretained(this), std::move(context_provider),
                   gpu_memory_buffer_manager, callback);
    return;
  }

  RequestCompositorFrameSinkInternal(std::move(context_provider),
                                     gpu_memory_buffer_manager, callback);
}

void WindowPortMus::RequestCompositorFrameSinkInternal(
    scoped_refptr<cc::ContextProvider> context_provider,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
    const CompositorFrameSinkCallback& callback) {
  DCHECK(frame_sink_id_.is_valid());
  std::unique_ptr<ui::ClientCompositorFrameSinkBinding>
      compositor_frame_sink_binding;
  std::unique_ptr<ui::ClientCompositorFrameSink> compositor_frame_sink =
      ui::ClientCompositorFrameSink::Create(
          frame_sink_id_, std::move(context_provider),
          gpu_memory_buffer_manager, &compositor_frame_sink_binding);
  AttachCompositorFrameSink(std::move(compositor_frame_sink_binding));
  callback.Run(std::move(compositor_frame_sink));
}

void WindowPortMus::AttachCompositorFrameSink(
    std::unique_ptr<ui::ClientCompositorFrameSinkBinding>
        compositor_frame_sink_binding) {
  window_tree_client_->AttachCompositorFrameSink(
      server_id(), compositor_frame_sink_binding->TakeFrameSinkRequest(),
      mojo::MakeProxy(compositor_frame_sink_binding->TakeFrameSinkClient()));
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
  auto iter = FindChangeByTypeAndData(type, data);
  if (iter == server_changes_.end())
    return false;
  server_changes_.erase(iter);
  return true;
}

WindowPortMus::ServerChanges::iterator WindowPortMus::FindChangeByTypeAndData(
    const ServerChangeType type,
    const ServerChangeData& data) {
  auto iter = server_changes_.begin();
  for (; iter != server_changes_.end(); ++iter) {
    if (iter->type != type)
      continue;

    switch (type) {
      case ServerChangeType::ADD:
      case ServerChangeType::ADD_TRANSIENT:
      case ServerChangeType::REMOVE:
      case ServerChangeType::REMOVE_TRANSIENT:
      case ServerChangeType::REORDER:
      case ServerChangeType::TRANSIENT_REORDER:
        if (iter->data.child_id == data.child_id)
          return iter;
        break;
      case ServerChangeType::BOUNDS:
        if (iter->data.bounds_in_dip == data.bounds_in_dip)
          return iter;
        break;
      case ServerChangeType::DESTROY:
        // No extra data for delete.
        return iter;
      case ServerChangeType::PROPERTY:
        if (iter->data.property_name == data.property_name)
          return iter;
        break;
      case ServerChangeType::VISIBLE:
        if (iter->data.visible == data.visible)
          return iter;
        break;
    }
  }
  return iter;
}

PropertyConverter* WindowPortMus::GetPropertyConverter() {
  return window_tree_client_->delegate_->GetPropertyConverter();
}

Window* WindowPortMus::GetWindow() {
  return window_;
}

void WindowPortMus::AddChildFromServer(WindowMus* window) {
  ServerChangeData data;
  data.child_id = window->server_id();
  ScopedServerChange change(this, ServerChangeType::ADD, data);
  window_->AddChild(window->GetWindow());
}

void WindowPortMus::RemoveChildFromServer(WindowMus* child) {
  ServerChangeData data;
  data.child_id = child->server_id();
  ScopedServerChange change(this, ServerChangeType::REMOVE, data);
  window_->RemoveChild(child->GetWindow());
}

void WindowPortMus::ReorderFromServer(WindowMus* child,
                                      WindowMus* relative,
                                      ui::mojom::OrderDirection direction) {
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

void WindowPortMus::SetBoundsFromServer(
    const gfx::Rect& bounds,
    const base::Optional<cc::LocalSurfaceId>& local_surface_id) {
  ServerChangeData data;
  data.bounds_in_dip = bounds;
  ScopedServerChange change(this, ServerChangeType::BOUNDS, data);
  last_surface_size_ = bounds.size();
  if (local_surface_id)
    local_surface_id_ = *local_surface_id;
  else
    local_surface_id_ = cc::LocalSurfaceId();
  window_->SetBounds(bounds);
}

void WindowPortMus::SetVisibleFromServer(bool visible) {
  ServerChangeData data;
  data.visible = visible;
  ScopedServerChange change(this, ServerChangeType::VISIBLE, data);
  if (visible)
    window_->Show();
  else
    window_->Hide();
}

void WindowPortMus::SetOpacityFromServer(float opacity) {
  window_->layer()->SetOpacity(opacity);
}

void WindowPortMus::SetCursorFromServer(const ui::CursorData& cursor) {
  // As this does nothing more than set the cursor we don't need to use
  // ServerChange.
  cursor_ = cursor;
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

void WindowPortMus::SetFrameSinkIdFromServer(
    const cc::FrameSinkId& frame_sink_id) {
  frame_sink_id_ = frame_sink_id;
  if (!pending_compositor_frame_sink_request_.is_null()) {
    // TOP_LEVEL_IN_WM, and EMBED_IN_OWNER windows should not be requesting
    // CompositorFrameSinks.
    DCHECK_NE(WindowMusType::TOP_LEVEL_IN_WM, window_mus_type());
    DCHECK_NE(WindowMusType::EMBED_IN_OWNER, window_mus_type());
    base::ResetAndReturn(&pending_compositor_frame_sink_request_).Run();
    return;
  }
  UpdatePrimarySurfaceInfo();
}

const cc::LocalSurfaceId& WindowPortMus::GetOrAllocateLocalSurfaceId(
    const gfx::Size& surface_size) {
  if (last_surface_size_ == surface_size && local_surface_id_.is_valid())
    return local_surface_id_;

  local_surface_id_ = local_surface_id_allocator_.GenerateId();
  last_surface_size_ = surface_size;

  // If surface synchronization is enabled and the FrameSinkId is available,
  // then immediately embed the SurfaceId. The newly generated frame by the
  // embedder will block in the display compositor until the child submits a
  // corresponding CompositorFrame or a deadline hits.
  if (window_tree_client_->enable_surface_synchronization_ &&
      frame_sink_id_.is_valid()) {
    UpdatePrimarySurfaceInfo();
  }

  return local_surface_id_;
}

void WindowPortMus::SetPrimarySurfaceInfo(const cc::SurfaceInfo& surface_info) {
  primary_surface_info_ = surface_info;
  UpdateClientSurfaceEmbedder();
  if (window_->delegate())
    window_->delegate()->OnWindowSurfaceChanged(surface_info);
}

void WindowPortMus::SetFallbackSurfaceInfo(
    const cc::SurfaceInfo& surface_info) {
  fallback_surface_info_ = surface_info;
  UpdateClientSurfaceEmbedder();
}

void WindowPortMus::DestroyFromServer() {
  std::unique_ptr<ScopedServerChange> remove_from_parent_change;
  if (window_->parent()) {
    ServerChangeData data;
    data.child_id = server_id();
    WindowPortMus* parent = Get(window_->parent());
    remove_from_parent_change = base::MakeUnique<ScopedServerChange>(
        parent, ServerChangeType::REMOVE, data);
  }
  // NOTE: this can't use ScopedServerChange as |this| is destroyed before the
  // function returns (ScopedServerChange would attempt to access |this| after
  // destruction).
  ScheduleChange(ServerChangeType::DESTROY, ServerChangeData());
  delete window_;
}

void WindowPortMus::AddTransientChildFromServer(WindowMus* child) {
  ServerChangeData data;
  data.child_id = child->server_id();
  ScopedServerChange change(this, ServerChangeType::ADD_TRANSIENT, data);
  client::GetTransientWindowClient()->AddTransientChild(window_,
                                                        child->GetWindow());
}

void WindowPortMus::RemoveTransientChildFromServer(WindowMus* child) {
  ServerChangeData data;
  data.child_id = child->server_id();
  ScopedServerChange change(this, ServerChangeType::REMOVE_TRANSIENT, data);
  client::GetTransientWindowClient()->RemoveTransientChild(window_,
                                                           child->GetWindow());
}

WindowPortMus::ChangeSource WindowPortMus::OnTransientChildAdded(
    WindowMus* child) {
  ServerChangeData change_data;
  change_data.child_id = child->server_id();
  // If there was a change it means we scheduled the change by way of
  // AddTransientChildFromServer(), which came from the server.
  return RemoveChangeByTypeAndData(ServerChangeType::ADD_TRANSIENT, change_data)
             ? ChangeSource::SERVER
             : ChangeSource::LOCAL;
}

WindowPortMus::ChangeSource WindowPortMus::OnTransientChildRemoved(
    WindowMus* child) {
  ServerChangeData change_data;
  change_data.child_id = child->server_id();
  // If there was a change it means we scheduled the change by way of
  // RemoveTransientChildFromServer(), which came from the server.
  return RemoveChangeByTypeAndData(ServerChangeType::REMOVE_TRANSIENT,
                                   change_data)
             ? ChangeSource::SERVER
             : ChangeSource::LOCAL;
}

const cc::LocalSurfaceId& WindowPortMus::GetLocalSurfaceId() {
  return local_surface_id_;
}

std::unique_ptr<WindowMusChangeData>
WindowPortMus::PrepareForServerBoundsChange(const gfx::Rect& bounds) {
  std::unique_ptr<WindowMusChangeDataImpl> data(
      base::MakeUnique<WindowMusChangeDataImpl>());
  ServerChangeData change_data;
  change_data.bounds_in_dip = bounds;
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

void WindowPortMus::PrepareForDestroy() {
  ScheduleChange(ServerChangeType::DESTROY, ServerChangeData());
}

void WindowPortMus::PrepareForTransientRestack(WindowMus* window) {
  ServerChangeData change_data;
  change_data.child_id = window->server_id();
  ScheduleChange(ServerChangeType::TRANSIENT_REORDER, change_data);
}

void WindowPortMus::OnTransientRestackDone(WindowMus* window) {
  ServerChangeData change_data;
  change_data.child_id = window->server_id();
  const bool removed = RemoveChangeByTypeAndData(
      ServerChangeType::TRANSIENT_REORDER, change_data);
  DCHECK(removed);
}

void WindowPortMus::NotifyEmbeddedAppDisconnected() {
  for (WindowObserver& observer : *GetObservers(window_))
    observer.OnEmbeddedAppDisconnected(window_);
}

void WindowPortMus::OnPreInit(Window* window) {
  window_ = window;
  window_tree_client_->OnWindowMusCreated(this);
}

void WindowPortMus::OnDeviceScaleFactorChanged(float device_scale_factor) {
  if (window_->delegate())
    window_->delegate()->OnDeviceScaleFactorChanged(device_scale_factor);
}

void WindowPortMus::OnWillAddChild(Window* child) {
  ServerChangeData change_data;
  change_data.child_id = Get(child)->server_id();
  if (!RemoveChangeByTypeAndData(ServerChangeType::ADD, change_data))
    window_tree_client_->OnWindowMusAddChild(this, Get(child));
}

void WindowPortMus::OnWillRemoveChild(Window* child) {
  ServerChangeData change_data;
  change_data.child_id = Get(child)->server_id();
  if (!RemoveChangeByTypeAndData(ServerChangeType::REMOVE, change_data))
    window_tree_client_->OnWindowMusRemoveChild(this, Get(child));
}

void WindowPortMus::OnWillMoveChild(size_t current_index, size_t dest_index) {
  ServerChangeData change_data;
  change_data.child_id = Get(window_->children()[current_index])->server_id();
  // See description of TRANSIENT_REORDER for details on why it isn't removed
  // here.
  if (!RemoveChangeByTypeAndData(ServerChangeType::REORDER, change_data) &&
      FindChangeByTypeAndData(ServerChangeType::TRANSIENT_REORDER,
                              change_data) == server_changes_.end()) {
    window_tree_client_->OnWindowMusMoveChild(this, current_index, dest_index);
  }
}

void WindowPortMus::OnVisibilityChanged(bool visible) {
  ServerChangeData change_data;
  change_data.visible = visible;
  if (!RemoveChangeByTypeAndData(ServerChangeType::VISIBLE, change_data))
    window_tree_client_->OnWindowMusSetVisible(this, visible);
}

void WindowPortMus::OnDidChangeBounds(const gfx::Rect& old_bounds,
                                      const gfx::Rect& new_bounds) {
  ServerChangeData change_data;
  change_data.bounds_in_dip = new_bounds;
  if (!RemoveChangeByTypeAndData(ServerChangeType::BOUNDS, change_data))
    window_tree_client_->OnWindowMusBoundsChanged(this, old_bounds, new_bounds);
  if (client_surface_embedder_)
    client_surface_embedder_->UpdateSizeAndGutters();
}

std::unique_ptr<ui::PropertyData> WindowPortMus::OnWillChangeProperty(
    const void* key) {
  // |window_| is null if a property is set on the aura::Window before
  // Window::Init() is called. It's safe to ignore the change in this case as
  // once Window::Init() is called the Window is queried for the current set of
  // properties.
  if (!window_)
    return nullptr;

  return window_tree_client_->OnWindowMusWillChangeProperty(this, key);
}

void WindowPortMus::OnPropertyChanged(const void* key,
                                      int64_t old_value,
                                      std::unique_ptr<ui::PropertyData> data) {
  // See comment in OnWillChangeProperty() as to why |window_| may be null.
  if (!window_)
    return;

  ServerChangeData change_data;
  change_data.property_name =
      GetPropertyConverter()->GetTransportNameForPropertyKey(key);
  // TODO(sky): investigate to see if we need to compare data. In particular do
  // we ever have a case where changing a property cascades into changing the
  // same property?
  if (!RemoveChangeByTypeAndData(ServerChangeType::PROPERTY, change_data))
    window_tree_client_->OnWindowMusPropertyChanged(this, key, old_value,
                                                    std::move(data));
}

std::unique_ptr<cc::CompositorFrameSink>
WindowPortMus::CreateCompositorFrameSink() {
  // TODO(penghuang): Implement it for Mus.
  return nullptr;
}

cc::SurfaceId WindowPortMus::GetSurfaceId() const {
  // TODO(penghuang): Implement it for Mus.
  return cc::SurfaceId();
}

void WindowPortMus::UpdatePrimarySurfaceInfo() {
  bool embeds_surface = window_mus_type() == WindowMusType::TOP_LEVEL_IN_WM ||
                        window_mus_type() == WindowMusType::EMBED_IN_OWNER;
  if (!embeds_surface || !window_tree_client_->enable_surface_synchronization_)
    return;

  if (!frame_sink_id_.is_valid() || !local_surface_id_.is_valid())
    return;

  SetPrimarySurfaceInfo(
      cc::SurfaceInfo(cc::SurfaceId(frame_sink_id_, local_surface_id_),
                      ScaleFactorForDisplay(window_), last_surface_size_));
}

void WindowPortMus::UpdateClientSurfaceEmbedder() {
  bool embeds_surface = window_mus_type() == WindowMusType::TOP_LEVEL_IN_WM ||
                        window_mus_type() == WindowMusType::EMBED_IN_OWNER;
  if (!embeds_surface)
    return;

  if (!client_surface_embedder_) {
    client_surface_embedder_ = base::MakeUnique<ClientSurfaceEmbedder>(
        window_, window_tree_client_->normal_client_area_insets_);
  }

  client_surface_embedder_->SetPrimarySurfaceInfo(primary_surface_info_);
  client_surface_embedder_->SetFallbackSurfaceInfo(fallback_surface_info_);
}

}  // namespace aura
