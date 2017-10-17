// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/mus/window_port_mus.h"

#include "base/memory/ptr_util.h"
#include "components/viz/client/local_surface_id_provider.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/transient_window_client.h"
#include "ui/aura/env.h"
#include "ui/aura/hit_test_data_provider_aura.h"
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
#include "ui/gfx/geometry/dip_util.h"

namespace aura {

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

void WindowPortMus::SetTextInputState(ui::mojom::TextInputStatePtr state) {
  window_tree_client_->SetWindowTextInputState(this, std::move(state));
}

void WindowPortMus::SetImeVisibility(bool visible,
                                     ui::mojom::TextInputStatePtr state) {
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

void WindowPortMus::SetExtendedHitRegionForChildren(
    const gfx::Insets& mouse_insets,
    const gfx::Insets& touch_insets) {
  window_tree_client_->SetExtendedHitRegionForChildren(window_, mouse_insets,
                                                       touch_insets);
}

void WindowPortMus::Embed(
    ui::mojom::WindowTreeClientPtr client,
    uint32_t flags,
    const ui::mojom::WindowTree::EmbedCallback& callback) {
  window_tree_client_->Embed(window_, std::move(client), flags, callback);
}

void WindowPortMus::ScheduleEmbed(
    ui::mojom::WindowTreeClientPtr client,
    base::OnceCallback<void(const base::UnguessableToken&)> callback) {
  window_tree_client_->ScheduleEmbed(std::move(client), std::move(callback));
}

std::unique_ptr<viz::ClientLayerTreeFrameSink>
WindowPortMus::RequestLayerTreeFrameSink(
    scoped_refptr<viz::ContextProvider> context_provider,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager) {
  viz::mojom::CompositorFrameSinkPtrInfo sink_info;
  viz::mojom::CompositorFrameSinkRequest sink_request =
      mojo::MakeRequest(&sink_info);
  viz::mojom::CompositorFrameSinkClientPtr client;
  viz::mojom::CompositorFrameSinkClientRequest client_request =
      mojo::MakeRequest(&client);

  viz::ClientLayerTreeFrameSink::InitParams params;
  params.gpu_memory_buffer_manager = gpu_memory_buffer_manager;
  params.pipes.compositor_frame_sink_info = std::move(sink_info);
  params.pipes.client_request = std::move(client_request);
  params.hit_test_data_provider =
      std::make_unique<HitTestDataProviderAura>(window_);
  params.local_surface_id_provider =
      std::make_unique<viz::DefaultLocalSurfaceIdProvider>();
  params.enable_surface_synchronization = true;

  auto layer_tree_frame_sink = std::make_unique<viz::ClientLayerTreeFrameSink>(
      std::move(context_provider), nullptr /* worker_context_provider */,
      &params);
  window_tree_client_->AttachCompositorFrameSink(
      server_id(), std::move(sink_request), std::move(client));
  return layer_tree_frame_sink;
}

viz::FrameSinkId WindowPortMus::GetFrameSinkId() const {
  if (embed_frame_sink_id_.is_valid())
    return embed_frame_sink_id_;
  return viz::FrameSinkId(0, server_id());
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
      case ServerChangeType::TRANSFORM:
        if (iter->data.transform == data.transform)
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
    const base::Optional<viz::LocalSurfaceId>& local_surface_id) {
  ServerChangeData data;
  data.bounds_in_dip = bounds;
  ScopedServerChange change(this, ServerChangeType::BOUNDS, data);
  last_surface_size_in_pixels_ =
      gfx::ConvertSizeToPixel(GetDeviceScaleFactor(), bounds.size());
  if (local_surface_id)
    local_surface_id_ = *local_surface_id;
  else
    local_surface_id_ = viz::LocalSurfaceId();
  window_->SetBounds(bounds);
}

void WindowPortMus::SetTransformFromServer(const gfx::Transform& transform) {
  ServerChangeData data;
  data.transform = transform;
  ScopedServerChange change(this, ServerChangeType::TRANSFORM, data);
  window_->SetTransform(transform);
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
    const viz::FrameSinkId& frame_sink_id) {
  DCHECK(window_mus_type() == WindowMusType::TOP_LEVEL_IN_WM ||
         window_mus_type() == WindowMusType::EMBED_IN_OWNER);
  embed_frame_sink_id_ = frame_sink_id;
  UpdatePrimarySurfaceInfo();
}

const viz::LocalSurfaceId& WindowPortMus::GetOrAllocateLocalSurfaceId(
    const gfx::Size& surface_size_in_pixels) {
  if (last_surface_size_in_pixels_ == surface_size_in_pixels &&
      local_surface_id_.is_valid()) {
    return local_surface_id_;
  }

  local_surface_id_ = local_surface_id_allocator_.GenerateId();
  last_surface_size_in_pixels_ = surface_size_in_pixels;

  // If the FrameSinkId is available, then immediately embed the SurfaceId.
  // The newly generated frame by the embedder will block in the display
  // compositor until the child submits a corresponding CompositorFrame or a
  // deadline hits.
  if (embed_frame_sink_id_.is_valid())
    UpdatePrimarySurfaceInfo();

  if (local_layer_tree_frame_sink_)
    local_layer_tree_frame_sink_->SetLocalSurfaceId(local_surface_id_);

  return local_surface_id_;
}

void WindowPortMus::SetFallbackSurfaceInfo(
    const viz::SurfaceInfo& surface_info) {
  if (!embed_frame_sink_id_.is_valid()) {
    // |primary_surface_info_| shold not be valid, since we didn't know the
    // |embed_frame_sink_id_|.
    DCHECK(!primary_surface_info_.is_valid());
    embed_frame_sink_id_ = surface_info.id().frame_sink_id();
    UpdatePrimarySurfaceInfo();
  }

  // The frame sink id should never be changed.
  DCHECK_EQ(surface_info.id().frame_sink_id(), embed_frame_sink_id_);

  fallback_surface_info_ = surface_info;
  UpdateClientSurfaceEmbedder();
  if (window_->delegate())
    window_->delegate()->OnFirstSurfaceActivation(fallback_surface_info_);
}

void WindowPortMus::DestroyFromServer() {
  std::unique_ptr<ScopedServerChange> remove_from_parent_change;
  if (window_->parent()) {
    ServerChangeData data;
    data.child_id = server_id();
    WindowPortMus* parent = Get(window_->parent());
    remove_from_parent_change = std::make_unique<ScopedServerChange>(
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

void WindowPortMus::AllocateLocalSurfaceId() {
  local_surface_id_ = local_surface_id_allocator_.GenerateId();
}

const viz::LocalSurfaceId& WindowPortMus::GetLocalSurfaceId() {
  return local_surface_id_;
}

std::unique_ptr<WindowMusChangeData>
WindowPortMus::PrepareForServerBoundsChange(const gfx::Rect& bounds) {
  std::unique_ptr<WindowMusChangeDataImpl> data(
      std::make_unique<WindowMusChangeDataImpl>());
  ServerChangeData change_data;
  change_data.bounds_in_dip = bounds;
  data->change = std::make_unique<ScopedServerChange>(
      this, ServerChangeType::BOUNDS, change_data);
  return std::move(data);
}

std::unique_ptr<WindowMusChangeData>
WindowPortMus::PrepareForServerVisibilityChange(bool value) {
  std::unique_ptr<WindowMusChangeDataImpl> data(
      std::make_unique<WindowMusChangeDataImpl>());
  ServerChangeData change_data;
  change_data.visible = value;
  data->change = std::make_unique<ScopedServerChange>(
      this, ServerChangeType::VISIBLE, change_data);
  return std::move(data);
}

void WindowPortMus::PrepareForDestroy() {
  ScheduleChange(ServerChangeType::DESTROY, ServerChangeData());
}

void WindowPortMus::NotifyEmbeddedAppDisconnected() {
  for (WindowObserver& observer : *GetObservers(window_))
    observer.OnEmbeddedAppDisconnected(window_);
}

bool WindowPortMus::HasLocalLayerTreeFrameSink() {
  return !!local_layer_tree_frame_sink_;
}

float WindowPortMus::GetDeviceScaleFactor() {
  return window_->layer()->device_scale_factor();
}

void WindowPortMus::OnPreInit(Window* window) {
  window_ = window;
  window_tree_client_->OnWindowMusCreated(this);
}

void WindowPortMus::OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                               float new_device_scale_factor) {
  if (local_surface_id_.is_valid() && local_layer_tree_frame_sink_) {
    local_surface_id_ = local_surface_id_allocator_.GenerateId();
    local_layer_tree_frame_sink_->SetLocalSurfaceId(local_surface_id_);
  }
  window_tree_client_->OnWindowMusDeviceScaleFactorChanged(
      this, old_device_scale_factor, new_device_scale_factor);

  if (window_->delegate()) {
    window_->delegate()->OnDeviceScaleFactorChanged(old_device_scale_factor,
                                                    new_device_scale_factor);
  }
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
  if (!RemoveChangeByTypeAndData(ServerChangeType::REORDER, change_data))
    window_tree_client_->OnWindowMusMoveChild(this, current_index, dest_index);
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

void WindowPortMus::OnDidChangeTransform(const gfx::Transform& old_transform,
                                         const gfx::Transform& new_transform) {
  ServerChangeData change_data;
  change_data.transform = new_transform;
  if (!RemoveChangeByTypeAndData(ServerChangeType::TRANSFORM, change_data)) {
    window_tree_client_->OnWindowMusTransformChanged(this, old_transform,
                                                     new_transform);
  }
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

std::unique_ptr<cc::LayerTreeFrameSink>
WindowPortMus::CreateLayerTreeFrameSink() {
  DCHECK_EQ(window_mus_type(), WindowMusType::LOCAL);
  DCHECK(!local_layer_tree_frame_sink_);
  auto frame_sink = RequestLayerTreeFrameSink(
      nullptr,
      aura::Env::GetInstance()->context_factory()->GetGpuMemoryBufferManager());
  local_surface_id_ = local_surface_id_allocator_.GenerateId();
  frame_sink->SetLocalSurfaceId(local_surface_id_);
  local_layer_tree_frame_sink_ = frame_sink->GetWeakPtr();
  return std::move(frame_sink);
}

viz::SurfaceId WindowPortMus::GetSurfaceId() const {
  return viz::SurfaceId(embed_frame_sink_id_, local_surface_id_);
}

void WindowPortMus::OnWindowAddedToRootWindow() {}

void WindowPortMus::OnWillRemoveWindowFromRootWindow() {}

void WindowPortMus::OnEventTargetingPolicyChanged() {
  SetEventTargetingPolicy(window_->event_targeting_policy());
}

bool WindowPortMus::ShouldRestackTransientChildren() {
  return should_restack_transient_children_;
}

void WindowPortMus::UpdatePrimarySurfaceInfo() {
  if (window_mus_type() != WindowMusType::TOP_LEVEL_IN_WM &&
      window_mus_type() != WindowMusType::EMBED_IN_OWNER &&
      window_mus_type() != WindowMusType::DISPLAY_MANUALLY_CREATED &&
      window_mus_type() != WindowMusType::LOCAL) {
    return;
  }

  if (!embed_frame_sink_id_.is_valid() || !local_surface_id_.is_valid())
    return;

  primary_surface_info_ =
      viz::SurfaceInfo(viz::SurfaceId(embed_frame_sink_id_, local_surface_id_),
                       GetDeviceScaleFactor(), last_surface_size_in_pixels_);
  UpdateClientSurfaceEmbedder();
}

void WindowPortMus::UpdateClientSurfaceEmbedder() {
  if (window_mus_type() != WindowMusType::TOP_LEVEL_IN_WM &&
      window_mus_type() != WindowMusType::EMBED_IN_OWNER &&
      window_mus_type() != WindowMusType::DISPLAY_MANUALLY_CREATED &&
      window_mus_type() != WindowMusType::LOCAL) {
    return;
  }

  if (!client_surface_embedder_) {
    client_surface_embedder_ = std::make_unique<ClientSurfaceEmbedder>(
        window_, window_mus_type() == WindowMusType::TOP_LEVEL_IN_WM,
        window_tree_client_->normal_client_area_insets_);
  }

  client_surface_embedder_->SetPrimarySurfaceInfo(primary_surface_info_);
  client_surface_embedder_->SetFallbackSurfaceInfo(fallback_surface_info_);
}

}  // namespace aura
