// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/mus/window_port_mus.h"

#include <utility>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/no_destructor.h"
#include "cc/mojo_embedder/async_layer_tree_frame_sink.h"
#include "components/viz/client/hit_test_data_provider_draw_quad.h"
#include "components/viz/client/local_surface_id_provider.h"
#include "components/viz/common/features.h"
#include "components/viz/common/surfaces/local_surface_id_allocation.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "services/ws/public/mojom/window_tree_constants.mojom.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/drag_drop_delegate.h"
#include "ui/aura/client/transient_window_client.h"
#include "ui/aura/env.h"
#include "ui/aura/mus/mus_lsi_allocator.h"
#include "ui/aura/mus/property_converter.h"
#include "ui/aura/mus/property_utils.h"
#include "ui/aura/mus/window_tree_client.h"
#include "ui/aura/mus/window_tree_client_delegate.h"
#include "ui/aura/mus/window_tree_host_mus.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_observer.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/class_property.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/ui_base_switches_util.h"
#include "ui/compositor/compositor.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/dip_util.h"

namespace aura {

namespace {
static const char* kMus = "Mus";
}  // namespace

struct WindowPortMus::PendingLayerTreeFrameSinkArgs {
  scoped_refptr<viz::ContextProvider> context_provider;
  scoped_refptr<viz::RasterContextProvider> raster_context_provider;
  gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager;
};

WindowPortMus::WindowMusChangeDataImpl::WindowMusChangeDataImpl() = default;

WindowPortMus::WindowMusChangeDataImpl::~WindowMusChangeDataImpl() = default;

class WindowPortMus::VisibilityTracker : public WindowObserver {
 public:
  using Callback = base::RepeatingCallback<void(bool visible)>;
  VisibilityTracker(Window* target, Callback callback)
      : target_(target),
        callback_(std::move(callback)),
        last_visible_(target->IsVisible()) {
    target_->AddObserver(this);
  }

  ~VisibilityTracker() override {
    if (target_)
      target_->RemoveObserver(this);
  }

 private:
  // WindowObserver:
  void OnWindowVisibilityChanged(Window* window, bool visible) override {
    // Checking visibility change here instead of in OnVisibilityChanged to
    // capture the change from window ancestors in addition to the window
    // itself.
    bool new_visible = target_->IsVisible();
    if (new_visible == last_visible_)
      return;

    last_visible_ = new_visible;
    callback_.Run(new_visible);
  }
  void OnWindowDestroyed(Window* window) override {
    DCHECK_EQ(target_, window);
    target_ = nullptr;
  }

  Window* target_;
  Callback callback_;
  bool last_visible_;

  DISALLOW_COPY_AND_ASSIGN(VisibilityTracker);
};

// static
WindowMus* WindowMus::Get(Window* window) {
  return WindowPortMus::Get(window);
}

WindowPortMus::WindowPortMus(WindowTreeClient* client,
                             WindowMusType window_mus_type)
    : WindowPort(WindowPort::Type::kMus),
      WindowMus(window_mus_type),
      window_tree_client_(client),
      weak_ptr_factory_(this) {}

WindowPortMus::~WindowPortMus() {
  allocator_.reset();

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
  WindowPort* port = WindowPort::Get(window);
  return port && port->type() == WindowPort::Type::kMus
             ? static_cast<WindowPortMus*>(port)
             : nullptr;
}

void WindowPortMus::SetTextInputState(ui::mojom::TextInputStatePtr state) {
  window_tree_client_->SetWindowTextInputState(this, std::move(state));
}

void WindowPortMus::SetImeVisibility(bool visible,
                                     ui::mojom::TextInputStatePtr state) {
  window_tree_client_->SetImeVisibility(this, visible, std::move(state));
}

void WindowPortMus::SetCursor(const ui::Cursor& cursor) {
  if (cursor == cursor_)
    return;

  window_tree_client_->SetCursor(this, cursor_, cursor);
  cursor_ = cursor;
}

void WindowPortMus::SetEventTargetingPolicy(
    ws::mojom::EventTargetingPolicy policy) {
  window_tree_client_->SetEventTargetingPolicy(this, policy);
}

void WindowPortMus::SetCanAcceptDrops(bool can_accept_drops) {
  window_tree_client_->SetCanAcceptDrops(this, can_accept_drops);
}

void WindowPortMus::SetHitTestInsets(const gfx::Insets& mouse,
                                     const gfx::Insets& touch) {
  window_tree_client_->SetHitTestInsets(this, mouse, touch);
}

void WindowPortMus::Embed(ws::mojom::WindowTreeClientPtr client,
                          uint32_t flags,
                          ws::mojom::WindowTree::EmbedCallback callback) {
  if (!PrepareForEmbed()) {
    std::move(callback).Run(false);
    return;
  }
  window_tree_client_->tree_->Embed(
      server_id(), std::move(client), flags,
      base::BindOnce(&WindowPortMus::OnEmbedAck, weak_ptr_factory_.GetWeakPtr(),
                     std::move(callback)));
}

void WindowPortMus::EmbedUsingToken(
    const base::UnguessableToken& token,
    uint32_t flags,
    ws::mojom::WindowTree::EmbedCallback callback) {
  if (!PrepareForEmbed()) {
    std::move(callback).Run(false);
    return;
  }
  window_tree_client_->tree_->EmbedUsingToken(
      server_id(), token, flags,
      base::BindOnce(&WindowPortMus::OnEmbedAck, weak_ptr_factory_.GetWeakPtr(),
                     std::move(callback)));
}

void WindowPortMus::CreateLayerTreeFrameSink(
    scoped_refptr<viz::ContextProvider> context_provider,
    scoped_refptr<viz::RasterContextProvider> raster_context_provider,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager) {
  // Ensures MusLsiAllocator has been created.
  GetLocalSurfaceIdAllocation();

  if (did_set_frame_sink_) {
    pending_layer_tree_frame_sink_args_ =
        std::make_unique<PendingLayerTreeFrameSinkArgs>();
    pending_layer_tree_frame_sink_args_->context_provider =
        std::move(context_provider);
    pending_layer_tree_frame_sink_args_->raster_context_provider =
        std::move(raster_context_provider);
    pending_layer_tree_frame_sink_args_->gpu_memory_buffer_manager =
        gpu_memory_buffer_manager;
    window_tree_client_->RequestNewLocalSurfaceId(this);
    return;
  }
  // The second time a LayerTreeFrameSink is set LayerTreeHostImpl expects a
  // new LocalSurfaceId. Request one from the server, and when received create
  // the LayerTreeFrameSink.
  // TODO(sky): this is temporary and can be removed once
  // https://crbug.com/921129 is fixed.
  did_set_frame_sink_ = true;
  CreateLayerTreeFrameSinkImpl(std::move(context_provider),
                               std::move(raster_context_provider),
                               gpu_memory_buffer_manager);
}

viz::FrameSinkId WindowPortMus::GenerateFrameSinkIdFromServerId() const {
  // With mus, the client does not know its own client id. So it uses a constant
  // value of 0. This gets replaced in the server side with the correct value
  // where appropriate.
  constexpr int kClientSelfId = 0;
  return viz::FrameSinkId(kClientSelfId, server_id());
}

gfx::Size WindowPortMus::GetSizeInPixels() {
  return GetSizeInPixels(window_->bounds().size());
}

gfx::Size WindowPortMus::GetSizeInPixels(const gfx::Size& size) {
  return gfx::ScaleToCeiledSize(size, GetDeviceScaleFactor());
}

void WindowPortMus::SetAllocator(std::unique_ptr<MusLsiAllocator> allocator) {
  DCHECK(!allocator_);
  allocator_ = std::move(allocator);
  allocator_->OnInstalled();
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

bool WindowPortMus::PrepareForEmbed() {
  // Window::Init() must be called before Embed() (otherwise the server hasn't
  // been told about the window).
  DCHECK(window_->layer());

  // The window server removes all children before embedding. In other words,
  // it's generally an error to Embed() with existing children. So, fail early.
  if (!window_->children().empty())
    return false;

  // Can only embed in windows created by this client.
  if (window_mus_type() != WindowMusType::LOCAL)
    return false;

  // Don't allow an embed when one exists. This could be handled, if the
  // callback was converted to OnChangeCompleted(). To attempt to handle it
  // without routing the callback over the WindowTreeClient pipe would result
  // in problemcs because of ordering. The ordering problem is because there is
  // the Embed() request, the callback, and OnEmbeddedAppDisconnected() (which
  // originates from the server side).
  if (has_embedding_)
    return false;

  has_embedding_ = true;
  DCHECK(!allocator_.get());
  SetAllocator(std::make_unique<ParentAllocator>(this, window_tree_client_,
                                                 /* is_embedder */ true));
  return true;
}

// static
void WindowPortMus::OnEmbedAck(
    base::WeakPtr<WindowPortMus> window,
    ws::mojom::WindowTree::EmbedCallback real_callback,
    bool result) {
  if (window && !result) {
    window->has_embedding_ = false;
    window->allocator_.reset();
  }
  std::move(real_callback).Run(window && result);
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
                                      ws::mojom::OrderDirection direction) {
  // Keying off solely the id isn't entirely accurate, in so far as if Window
  // does some other reordering then the server and client are out of sync.
  // But we assume only one client can make changes to a particular window at
  // a time, so this should be ok.
  ServerChangeData data;
  data.child_id = child->server_id();
  ScopedServerChange change(this, ServerChangeType::REORDER, data);
  if (direction == ws::mojom::OrderDirection::BELOW)
    window_->StackChildBelow(child->GetWindow(), relative->GetWindow());
  else
    window_->StackChildAbove(child->GetWindow(), relative->GetWindow());
}

void WindowPortMus::SetBoundsFromServer(const gfx::Rect& bounds) {
  // Changes to TOP_LEVEL and EMBED are routed through WindowTreeHostMus.
  DCHECK(window_mus_type() != WindowMusType::TOP_LEVEL &&
         window_mus_type() != WindowMusType::EMBED);
  ServerChangeData data;
  data.bounds_in_dip = bounds;
  ScopedServerChange change(this, ServerChangeType::BOUNDS, data);
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

void WindowPortMus::SetCursorFromServer(const ui::Cursor& cursor) {
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
  // Only called if this window is embedding another window.
  DCHECK(has_embedding_);
  embed_frame_sink_id_ = frame_sink_id;
  window_->SetEmbedFrameSinkId(embed_frame_sink_id_);
  DCHECK(allocator_->AsParentAllocator());
  allocator_->AsParentAllocator()->OnFrameSinkIdChanged();
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
  // This API does not make sense for EMBED.
  DCHECK_NE(window_mus_type(), WindowMusType::EMBED);
  if (!allocator_ && (window_mus_type() == WindowMusType::TOP_LEVEL &&
                      static_cast<WindowTreeHostMus*>(window_->GetHost())
                          ->has_pending_local_surface_id_from_server())) {
    SetAllocator(
        std::make_unique<TopLevelAllocator>(this, window_tree_client_));
  }
  if (allocator_) {
    allocator_->AllocateLocalSurfaceId();
  } else if (window_mus_type() == WindowMusType::LOCAL) {
    SetAllocator(std::make_unique<ParentAllocator>(this, window_tree_client_,
                                                   /* is_embedder */ false));
  }
}

viz::ScopedSurfaceIdAllocator WindowPortMus::GetSurfaceIdAllocator(
    base::OnceCallback<void()> allocation_task) {
  // This API does not make sense for EMBED.
  DCHECK_NE(window_mus_type(), WindowMusType::EMBED);
  return allocator_
             ? allocator_->GetSurfaceIdAllocator(std::move(allocation_task))
             : viz::ScopedSurfaceIdAllocator(std::move(allocation_task));
}

void WindowPortMus::InvalidateLocalSurfaceId() {
  // This API does not make sense for EMBED.
  DCHECK_NE(window_mus_type(), WindowMusType::EMBED);
  if (allocator_)
    allocator_->InvalidateLocalSurfaceId();
}

void WindowPortMus::UpdateLocalSurfaceIdFromParent(
    const viz::LocalSurfaceIdAllocation& local_surface_id_allocation) {
  switch (window_mus_type()) {
    case WindowMusType::EMBED:
      window_->GetHost()->compositor()->UpdateLocalSurfaceIdFromParent(
          local_surface_id_allocation);
      break;
    case WindowMusType::TOP_LEVEL:
      if (!allocator_) {
        SetAllocator(
            std::make_unique<TopLevelAllocator>(this, window_tree_client_));
      }
      DCHECK(allocator_->AsTopLevelAllocator());
      allocator_->AsTopLevelAllocator()->UpdateLocalSurfaceIdFromParent(
          local_surface_id_allocation);
      break;
    case WindowMusType::LOCAL:
    case WindowMusType::OTHER:
      NOTREACHED();
      break;
  }
}

void WindowPortMus::UpdateLocalSurfaceIdFromEmbeddedClient(
    const viz::LocalSurfaceIdAllocation&
        embedded_client_local_surface_id_allocation) {
  // This API does not make sense for EMBED and TOP_LEVEL.
  DCHECK_NE(window_mus_type(), WindowMusType::EMBED);
  DCHECK_NE(window_mus_type(), WindowMusType::TOP_LEVEL);
  if (allocator_) {
    DCHECK(allocator_->AsParentAllocator());
    allocator_->AsParentAllocator()->UpdateLocalSurfaceIdFromEmbeddedClient(
        embedded_client_local_surface_id_allocation);
  }
}

const viz::LocalSurfaceIdAllocation&
WindowPortMus::GetLocalSurfaceIdAllocation() {
  static base::NoDestructor<viz::LocalSurfaceIdAllocation> empty_allocation;
  if (window_mus_type() == WindowMusType::EMBED && !allocator_) {
    // Ideally EmbeddedAllocator would be created in the constructor, but that
    // isn't possible because of timing dependencies (EmbeddedAllocator expects
    // GetHost()->compositor() to return non-null).
    SetAllocator(
        std::make_unique<EmbeddedAllocator>(this, window_tree_client_));
  }
  return allocator_ ? allocator_->GetLocalSurfaceIdAllocation()
                    : *empty_allocation;
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
  has_embedding_ = false;
  allocator_ = nullptr;
  for (WindowObserver& observer : *GetObservers(window_))
    observer.OnEmbeddedAppDisconnected(window_);
}

float WindowPortMus::GetDeviceScaleFactor() {
  return window_->layer()->device_scale_factor();
}

void WindowPortMus::DidSetWindowTreeHostBoundsFromServer() {
  if (!pending_layer_tree_frame_sink_args_)
    return;

  auto args = std::move(pending_layer_tree_frame_sink_args_);
  CreateLayerTreeFrameSinkImpl(std::move(args->context_provider),
                               std::move(args->raster_context_provider),
                               args->gpu_memory_buffer_manager);
}

void WindowPortMus::OnPreInit(Window* window) {
  window_ = window;
  window_tree_client_->OnWindowMusCreated(this);
}

void WindowPortMus::OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                               float new_device_scale_factor) {
  if (allocator_)
    allocator_->OnDeviceScaleFactorChanged();

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
  const bool from_server =
      RemoveChangeByTypeAndData(ServerChangeType::BOUNDS, change_data);
  if (allocator_)
    allocator_->OnDidChangeBounds(GetSizeInPixels(), from_server);
  if (!from_server)
    window_tree_client_->OnWindowMusBoundsChanged(this, old_bounds, new_bounds);
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

  if (key == client::kDragDropDelegateKey) {
    SetCanAcceptDrops(window_->GetProperty(client::kDragDropDelegateKey) !=
                      nullptr);
  }

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
  // This function is currently not used when aura is configured with MUS.
  NOTIMPLEMENTED();
  return nullptr;
}

void WindowPortMus::OnEventTargetingPolicyChanged() {
  SetEventTargetingPolicy(window_->event_targeting_policy());
}

bool WindowPortMus::ShouldRestackTransientChildren() {
  return should_restack_transient_children_;
}

void WindowPortMus::RegisterFrameSinkId(const viz::FrameSinkId& frame_sink_id) {
  if (frame_sink_id == embed_frame_sink_id_)
    return;

  window_tree_client_->RegisterFrameSinkId(this, frame_sink_id);
  // This api only makes sense for local windows.
  DCHECK_EQ(window_mus_type(), WindowMusType::LOCAL);
  if (!allocator_) {
    SetAllocator(std::make_unique<ParentAllocator>(this, window_tree_client_,
                                                   /* is_embedder */ false));
  }
}

void WindowPortMus::UnregisterFrameSinkId(
    const viz::FrameSinkId& frame_sink_id) {
  if (frame_sink_id == embed_frame_sink_id_)
    return;

  window_tree_client_->UnregisterFrameSinkId(this);
}

void WindowPortMus::TrackOcclusionState() {
  // base::Unretained because |this| owns |visibility_tracker_|.
  visibility_tracker_ = std::make_unique<VisibilityTracker>(
      window_, base::BindRepeating(
                   &WindowPortMus::UpdateOcclusionStateAfterVisiblityChange,
                   base::Unretained(this)));
  window_tree_client_->TrackOcclusionState(this);
}

void WindowPortMus::SetOcclusionStateFromServer(
    ws::mojom::OcclusionState occlusion_state) {
  const Window::OcclusionState new_state =
      WindowOcclusionStateFromMojom(occlusion_state);
  const Window::OcclusionState old_state = window_->occlusion_state();

  if (old_state == new_state)
    return;

  // Filter HIDDEN/VISIBLE state that does not match window target visibility.
  // This happens when the client makes visibility changes without waiting for
  // server's occlusion state update. The stale occlusion state is still
  // received and should be dropped to avoid unnecessary state change.
  // e.g.
  //   CLIENT: Hide()
  //   CLIENT: Show()
  //   SERVER: Receives Hide() and sends back HIDDEN
  //   SERVER: Receives Show() and sends back VISIBLE
  //   CLIENT: Receives HIDDEN and drops it because local state is visible.
  //   CLIENT: Receives VISIBLE and accepts it.
  const bool visible = window_->IsVisible();
  if ((visible && new_state == Window::OcclusionState::HIDDEN) ||
      (!visible && new_state == Window::OcclusionState::VISIBLE)) {
    return;
  }

  UpdateOcclusionState(new_state);
}

void WindowPortMus::UpdateOcclusionState(Window::OcclusionState new_state) {
  const Window::OcclusionState old_state = window_->occlusion_state();

  if (new_state == Window::OcclusionState::HIDDEN &&
      old_state != Window::OcclusionState::UNKNOWN) {
    occlusion_state_before_hidden_ = old_state;
  } else {
    occlusion_state_before_hidden_.reset();
  }

  // TODO: Support occlusion region tracking. See crbug.com/900568.
  window_->SetOcclusionInfo(new_state, SkRegion());
}

void WindowPortMus::UpdateOcclusionStateAfterVisiblityChange(bool visible) {
  DCHECK_EQ(visible, window_->IsVisible());

  // No occlusion state update if |window_| is not added a root window.
  if (!window_->GetRootWindow())
    return;

  if (!visible) {
    // Set HIDDEN early when |window_| becomes hidden.
    UpdateOcclusionState(Window::OcclusionState::HIDDEN);
  } else {
    // Restore to before-hidden state or VISIBLE when |window_| becomes visible.
    UpdateOcclusionState(occlusion_state_before_hidden_
                             ? occlusion_state_before_hidden_.value()
                             : Window::OcclusionState::VISIBLE);
  }
}

void WindowPortMus::CreateLayerTreeFrameSinkImpl(
    scoped_refptr<viz::ContextProvider> context_provider,
    scoped_refptr<viz::RasterContextProvider> raster_context_provider,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager) {
  viz::mojom::CompositorFrameSinkPtrInfo sink_info;
  viz::mojom::CompositorFrameSinkRequest sink_request =
      mojo::MakeRequest(&sink_info);
  viz::mojom::CompositorFrameSinkClientPtr client;
  viz::mojom::CompositorFrameSinkClientRequest client_request =
      mojo::MakeRequest(&client);

  cc::mojo_embedder::AsyncLayerTreeFrameSink::InitParams params;
  ui::Compositor* compositor = window_->layer()->GetCompositor();
  DCHECK(compositor);
  params.compositor_task_runner = compositor->task_runner();
  params.gpu_memory_buffer_manager = gpu_memory_buffer_manager;
  params.pipes.compositor_frame_sink_info = std::move(sink_info);
  params.pipes.client_request = std::move(client_request);
  bool root_accepts_events =
      (window_->event_targeting_policy() ==
       ws::mojom::EventTargetingPolicy::TARGET_ONLY) ||
      (window_->event_targeting_policy() ==
       ws::mojom::EventTargetingPolicy::TARGET_AND_DESCENDANTS);
  if (features::IsVizHitTestingDrawQuadEnabled()) {
    params.hit_test_data_provider =
        std::make_unique<viz::HitTestDataProviderDrawQuad>(
            /* should_ask_for_child_regions */ false, root_accepts_events);
  }
  params.enable_surface_synchronization = true;
  params.client_name = kMus;

  auto layer_tree_frame_sink =
      std::make_unique<cc::mojo_embedder::AsyncLayerTreeFrameSink>(
          std::move(context_provider), std::move(raster_context_provider),
          &params);
  window_tree_client_->AttachCompositorFrameSink(
      server_id(), std::move(sink_request), std::move(client));
  window_->GetHost()->compositor()->SetLayerTreeFrameSink(
      std::move(layer_tree_frame_sink));
}

}  // namespace aura
