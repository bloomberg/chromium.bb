// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws2/window_service_client.h"

#include "base/auto_reset.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "components/viz/common/surfaces/surface_info.h"
#include "mojo/public/cpp/bindings/map.h"
#include "services/ui/common/util.h"
#include "services/ui/ws2/client_change.h"
#include "services/ui/ws2/client_change_tracker.h"
#include "services/ui/ws2/client_root.h"
#include "services/ui/ws2/client_window.h"
#include "services/ui/ws2/embedding.h"
#include "services/ui/ws2/pointer_watcher.h"
#include "services/ui/ws2/window_delegate_impl.h"
#include "services/ui/ws2/window_service.h"
#include "services/ui/ws2/window_service_delegate.h"
#include "ui/aura/client/transient_window_client.h"
#include "ui/aura/env.h"
#include "ui/aura/mus/property_converter.h"
#include "ui/aura/mus/property_utils.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/aura/window_tree_host.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_type.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/wm/core/capture_controller.h"

namespace ui {
namespace ws2 {

WindowServiceClient::WindowServiceClient(WindowService* window_service,
                                         ClientSpecificId client_id,
                                         mojom::WindowTreeClient* client,
                                         bool intercepts_events)
    : window_service_(window_service),
      client_id_(client_id),
      window_tree_client_(client),
      intercepts_events_(intercepts_events),
      property_change_tracker_(std::make_unique<ClientChangeTracker>()) {
  wm::CaptureController::Get()->AddObserver(this);
}

void WindowServiceClient::InitForEmbed(aura::Window* root,
                                       mojom::WindowTreePtr window_tree_ptr) {
  // Force ClientWindow to be created for |root|.
  ClientWindow* client_window =
      window_service_->GetClientWindowForWindowCreateIfNecessary(root);
  const ClientWindowId client_window_id = client_window->frame_sink_id();
  AddWindowToKnownWindows(root, client_window_id);

  const bool is_top_level = false;
  CreateClientRoot(root, is_top_level, std::move(window_tree_ptr));
}

void WindowServiceClient::InitFromFactory() {
  connection_type_ = ConnectionType::kOther;
}

WindowServiceClient::~WindowServiceClient() {
  wm::CaptureController::Get()->RemoveObserver(this);

  // Delete the embeddings first, that way we don't attempt to notify the client
  // when the windows the client created are deleted.
  while (!client_roots_.empty()) {
    DeleteClientRoot(client_roots_.begin()->get(),
                     DeleteClientRootReason::kDestructor);
  }

  while (!client_created_windows_.empty()) {
    // RemoveWindowFromKnownWindows() should make it such that the Window is no
    // longer recognized as being created (owned) by this client.
    const bool delete_if_owned = true;
    RemoveWindowFromKnownWindows(client_created_windows_.begin()->first,
                                 delete_if_owned);
  }
}

void WindowServiceClient::SendEventToClient(aura::Window* window,
                                            const Event& event) {
  // TODO(sky): remove event_id.
  const uint32_t event_id = 1;
  // Events should only come to windows connected to displays.
  DCHECK(window->GetHost());
  const int64_t display_id = window->GetHost()->GetDisplayId();
  // TODO(sky): wire up |matches_pointer_watcher|.
  const bool matches_pointer_watcher = false;
  window_tree_client_->OnWindowInputEvent(
      event_id, TransportIdForWindow(window), display_id, 0u, gfx::PointF(),
      PointerWatcher::CreateEventForClient(event), matches_pointer_watcher);
}

void WindowServiceClient::SendPointerWatcherEventToClient(
    int64_t display_id,
    std::unique_ptr<ui::Event> event) {
  window_tree_client_->OnPointerEventObserved(std::move(event),
                                              kInvalidTransportId, display_id);
}

bool WindowServiceClient::IsTopLevel(aura::Window* window) {
  auto iter = FindClientRootWithRoot(window);
  return iter != client_roots_.end() && (*iter)->is_top_level();
}

ClientRoot* WindowServiceClient::CreateClientRoot(
    aura::Window* window,
    bool is_top_level,
    mojom::WindowTreePtr window_tree) {
  OnWillBecomeClientRootWindow(window);

  ClientWindow* client_window = ClientWindow::GetMayBeNull(window);
  DCHECK(client_window);
  const ClientWindowId client_window_id = client_window->frame_sink_id();

  std::unique_ptr<ClientRoot> client_root_ptr =
      std::make_unique<ClientRoot>(this, window, is_top_level);
  ClientRoot* client_root = client_root_ptr.get();
  client_roots_.push_back(std::move(client_root_ptr));

  if (!is_top_level) {
    const int64_t display_id =
        display::Screen::GetScreen()->GetDisplayNearestWindow(window).id();
    const ClientWindowId focused_window_id =
        window->HasFocus() ? window_to_client_window_id_map_[window]
                           : ClientWindowId();
    const bool drawn = window->IsVisible() && window->GetHost();
    window_tree_client_->OnEmbed(WindowToWindowData(window),
                                 std::move(window_tree), display_id,
                                 ClientWindowIdToTransportId(focused_window_id),
                                 drawn, client_root->GetLocalSurfaceId());

    // Reset the frame sink id locally (after calling OnEmbed()). This is
    // needed so that the id used by the client matches the id used locally.
    client_window->set_frame_sink_id(ClientWindowId(client_id_, 0));
  }

  client_root->RegisterVizEmbeddingSupport();

  // Requests for top-levels don't get OnFrameSinkIdAllocated().
  if (!is_top_level) {
    // TODO(sky): centralize FrameSinkId management.
    window_tree_client_->OnFrameSinkIdAllocated(
        ClientWindowIdToTransportId(client_window_id),
        client_window->frame_sink_id());
  }

  return client_root;
}

void WindowServiceClient::DeleteClientRoot(ClientRoot* client_root,
                                           DeleteClientRootReason reason) {
  aura::Window* window = client_root->window();

  ClientWindow* client_window = ClientWindow::GetMayBeNull(window);
  if (client_window->capture_owner() == this) {
    // This client will no longer know about |window|, so it should not receive
    // any events sent to the client.
    client_window->set_capture_owner(nullptr);
  }

  // Delete the ClientRoot first, so that we don't attempt to spam the
  // client with a bunch of notifications.
  auto iter = FindClientRootWithRoot(client_root->window());
  DCHECK(iter != client_roots_.end());
  client_roots_.erase(iter);
  client_root = nullptr;  // |client_root| has been deleted.

  const Id client_window_id = TransportIdForWindow(window);

  if (reason == DeleteClientRootReason::kEmbed) {
    // This case happens when another client is embedded in the window this
    // client is embedded in. A window can have at most one client embedded in
    // it. Inform the client of this by way of OnUnembed() and OnWindowDeleted()
    // because the window is no longer known to this client.
    //
    // TODO(sky): consider simplifying this case and just deleting |this|. This
    // is because at this point the client can't do anything useful (the client
    // is unable to reattach to a Window in a display at this point).
    window_tree_client_->OnUnembed(client_window_id);
    window_tree_client_->OnWindowDeleted(client_window_id);
  }

  // This client no longer knows about |window|. Unparent any windows that
  // were created by this client and parented to windows in |window|. Recursion
  // should stop at windows created by this client because the client always
  // knows about such windows, and that never changes. Only windows created by
  // other clients may be removed from the set of known windows.
  std::vector<aura::Window*> created_windows;
  RemoveWindowFromKnownWindowsRecursive(window, &created_windows);
  for (aura::Window* created_window : created_windows) {
    if (created_window != window && created_window->parent())
      created_window->parent()->RemoveChild(created_window);
  }

  if (reason == DeleteClientRootReason::kUnembed) {
    // Notify the owner of the window it no longer has a client embedded in it.
    ClientWindow* client_window = ClientWindow::GetMayBeNull(window);
    if (client_window->owning_window_service_client() &&
        client_window->owning_window_service_client() != this) {
      // ClientRoots always trigger creation of a ClientWindow, so
      // |client_window| must exist at this point.
      DCHECK(client_window);
      client_window->owning_window_service_client()
          ->window_tree_client_->OnEmbeddedAppDisconnected(
              client_window->owning_window_service_client()
                  ->TransportIdForWindow(window));
    }
  }
}

void WindowServiceClient::DeleteClientRootWithRoot(aura::Window* window) {
  auto iter = FindClientRootWithRoot(window);
  if (iter == client_roots_.end())
    return;

  DeleteClientRoot(iter->get(), DeleteClientRootReason::kEmbed);
}

aura::Window* WindowServiceClient::GetWindowByClientId(
    const ClientWindowId& id) {
  auto iter = client_window_id_to_window_map_.find(id);
  return iter == client_window_id_to_window_map_.end() ? nullptr : iter->second;
}

bool WindowServiceClient::IsClientCreatedWindow(aura::Window* window) {
  return window && client_created_windows_.count(window) > 0u;
}

bool WindowServiceClient::IsClientRootWindow(aura::Window* window) {
  return window && FindClientRootWithRoot(window) != client_roots_.end();
}

WindowServiceClient::ClientRoots::iterator
WindowServiceClient::FindClientRootWithRoot(aura::Window* window) {
  if (!window)
    return client_roots_.end();
  for (auto iter = client_roots_.begin(); iter != client_roots_.end(); ++iter) {
    if (iter->get()->window() == window)
      return iter;
  }
  return client_roots_.end();
}

bool WindowServiceClient::IsWindowKnown(aura::Window* window) const {
  return window && window_to_client_window_id_map_.count(window) > 0u;
}

bool WindowServiceClient::IsWindowRootOfAnotherClient(
    aura::Window* window) const {
  ClientWindow* client_window = ClientWindow::GetMayBeNull(window);
  return client_window &&
         client_window->embedded_window_service_client() != nullptr &&
         client_window->embedded_window_service_client() != this;
}

void WindowServiceClient::OnCaptureLost(aura::Window* lost_capture) {
  DCHECK(IsWindowKnown(lost_capture));
  window_tree_client_->OnCaptureChanged(kInvalidTransportId,
                                        TransportIdForWindow(lost_capture));
}

aura::Window* WindowServiceClient::AddClientCreatedWindow(
    const ClientWindowId& id,
    bool is_top_level,
    std::unique_ptr<aura::Window> window_ptr) {
  aura::Window* window = window_ptr.get();
  client_created_windows_[window] = std::move(window_ptr);
  ClientWindow::Create(window, this, id, is_top_level);
  AddWindowToKnownWindows(window, id);
  return window;
}

void WindowServiceClient::AddWindowToKnownWindows(aura::Window* window,
                                                  const ClientWindowId& id) {
  DCHECK_EQ(0u, window_to_client_window_id_map_.count(window));
  window_to_client_window_id_map_[window] = id;

  DCHECK(IsWindowKnown(window));
  client_window_id_to_window_map_[id] = window;
  if (IsClientCreatedWindow(window))
    window->AddObserver(this);
}

void WindowServiceClient::RemoveWindowFromKnownWindows(aura::Window* window,
                                                       bool delete_if_owned) {
  DCHECK(IsWindowKnown(window));
  auto iter = window_to_client_window_id_map_.find(window);
  client_window_id_to_window_map_.erase(iter->second);
  window_to_client_window_id_map_.erase(iter);
  auto client_iter = client_created_windows_.find(window);
  if (client_iter != client_created_windows_.end()) {
    window->RemoveObserver(this);
    if (!delete_if_owned) {
      // |window| is in the process of being deleted, release() to avoid double
      // deletion.
      client_iter->second.release();
    }
    client_created_windows_.erase(client_iter);
  }
}

bool WindowServiceClient::IsValidIdForNewWindow(
    const ClientWindowId& id) const {
  return client_window_id_to_window_map_.count(id) == 0u &&
         base::checked_cast<ClientSpecificId>(id.client_id()) == client_id_;
}

Id WindowServiceClient::ClientWindowIdToTransportId(
    const ClientWindowId& client_window_id) const {
  if (client_window_id.client_id() == client_id_)
    return client_window_id.sink_id();
  const Id client_id = client_window_id.client_id();
  return (client_id << 32) | client_window_id.sink_id();
}

Id WindowServiceClient::TransportIdForWindow(aura::Window* window) const {
  DCHECK(IsWindowKnown(window));
  auto iter = window_to_client_window_id_map_.find(window);
  DCHECK(iter != window_to_client_window_id_map_.end());
  return ClientWindowIdToTransportId(iter->second);
}

ClientWindowId WindowServiceClient::MakeClientWindowId(
    Id transport_window_id) const {
  if (!ClientIdFromTransportId(transport_window_id))
    return ClientWindowId(client_id_, transport_window_id);
  return ClientWindowId(ClientIdFromTransportId(transport_window_id),
                        ClientWindowIdFromTransportId(transport_window_id));
}

void WindowServiceClient::RemoveWindowFromKnownWindowsRecursive(
    aura::Window* window,
    std::vector<aura::Window*>* created_windows) {
  if (IsClientCreatedWindow(window)) {
    // Stop iterating at windows created by this client. We assume the client
    // will keep seeing any descendants.
    if (created_windows)
      created_windows->push_back(window);
    return;
  }

  if (IsWindowKnown(window)) {
    const bool delete_if_owned = true;
    RemoveWindowFromKnownWindows(window, delete_if_owned);
  }

  for (aura::Window* child : window->children())
    RemoveWindowFromKnownWindowsRecursive(child, created_windows);
}

std::vector<mojom::WindowDataPtr> WindowServiceClient::WindowsToWindowDatas(
    const std::vector<aura::Window*>& windows) {
  std::vector<mojom::WindowDataPtr> array(windows.size());
  for (size_t i = 0; i < windows.size(); ++i)
    array[i] = WindowToWindowData(windows[i]);
  return array;
}

mojom::WindowDataPtr WindowServiceClient::WindowToWindowData(
    aura::Window* window) {
  aura::Window* parent = window->parent();
  aura::Window* transient_parent =
      aura::client::GetTransientWindowClient()->GetTransientParent(window);

  // If a window isn't known, it means it is not known to the client and should
  // not be sent over.
  if (!IsWindowKnown(parent))
    parent = nullptr;
  if (!IsWindowKnown(transient_parent))
    transient_parent = nullptr;
  mojom::WindowDataPtr client_window(mojom::WindowData::New());
  client_window->parent_id =
      parent ? TransportIdForWindow(parent) : kInvalidTransportId;
  client_window->window_id =
      window ? TransportIdForWindow(window) : kInvalidTransportId;
  client_window->transient_parent_id =
      transient_parent ? TransportIdForWindow(transient_parent)
                       : kInvalidTransportId;
  client_window->bounds = window->bounds();
  client_window->properties =
      window_service_->property_converter()->GetTransportProperties(window);
  client_window->visible = window->TargetVisibility();
  return client_window;
}

void WindowServiceClient::OnWillBecomeClientRootWindow(aura::Window* window) {
  DCHECK(window);

  // Only one client may be embedded in a window at a time.
  ClientWindow* client_window =
      window_service_->GetClientWindowForWindowCreateIfNecessary(window);
  if (client_window->embedded_window_service_client()) {
    client_window->embedded_window_service_client()->DeleteClientRootWithRoot(
        window);
    DCHECK(!client_window->embedded_window_service_client());
  }

  // Because a new client is being embedded all existing children are removed.
  // This is because this client is no longer able to add children to |window|
  // (until the embedding is removed).
  while (!window->children().empty())
    window->RemoveChild(window->children().front());
}

base::Optional<viz::LocalSurfaceId> WindowServiceClient::GetLocalSurfaceId(
    aura::Window* window) {
  auto iter = FindClientRootWithRoot(window);
  if (iter == client_roots_.end())
    return base::nullopt;
  return iter->get()->GetLocalSurfaceId();
}

bool WindowServiceClient::NewWindowImpl(
    const ClientWindowId& client_window_id,
    const std::map<std::string, std::vector<uint8_t>>& properties) {
  DVLOG(3) << "new window client=" << client_id_
           << " window_id=" << client_window_id.ToString();
  if (!IsValidIdForNewWindow(client_window_id)) {
    DVLOG(1) << "NewWindow failed (id is not valid for client)";
    return false;
  }
  const bool is_top_level = false;
  // WindowDelegateImpl deletes itself when |window| is destroyed.
  WindowDelegateImpl* window_delegate = new WindowDelegateImpl();
  std::unique_ptr<aura::Window> window_ptr =
      std::make_unique<aura::Window>(window_delegate);
  window_delegate->set_window(window_ptr.get());
  aura::Window* window = AddClientCreatedWindow(client_window_id, is_top_level,
                                                std::move(window_ptr));

  SetWindowType(window, aura::GetWindowTypeFromProperties(properties));
  for (auto& pair : properties) {
    window_service_->property_converter()->SetPropertyFromTransportValue(
        window, pair.first, &pair.second);
  }
  window->Init(LAYER_NOT_DRAWN);
  // Windows created by the client should only be destroyed by the client.
  window->set_owned_by_parent(false);
  return true;
}

bool WindowServiceClient::DeleteWindowImpl(const ClientWindowId& window_id) {
  aura::Window* window = GetWindowByClientId(window_id);
  DVLOG(3) << "deleting window client=" << client_id_
           << " client window_id= " << window_id.ToString();
  if (!window)
    return false;

  auto iter = FindClientRootWithRoot(window);
  if (iter != client_roots_.end()) {
    DeleteClientRoot(iter->get(), DeleteClientRootReason::kUnembed);
    return true;
  }

  if (!IsClientCreatedWindow(window))
    return false;

  const bool delete_if_owned = true;
  RemoveWindowFromKnownWindows(window, delete_if_owned);
  return true;
}

bool WindowServiceClient::SetCaptureImpl(const ClientWindowId& window_id) {
  DVLOG(3) << "SetCapture window_id=" << window_id;
  aura::Window* window = GetWindowByClientId(window_id);
  if (!window) {
    DVLOG(1) << "SetCapture failed (no window)";
    return false;
  }

  if ((!IsClientCreatedWindow(window) && !IsClientRootWindow(window)) ||
      !window->IsVisible() || !window->GetRootWindow()) {
    DVLOG(1) << "SetCapture failed (access denied or invalid window)";
    return false;
  }

  ClientWindow* client_window = ClientWindow::GetMayBeNull(window);

  wm::CaptureController* capture_controller = wm::CaptureController::Get();
  DCHECK(capture_controller);

  if (capture_controller->GetCaptureWindow() == window) {
    if (client_window->capture_owner() != this) {
      // The capture window didn't change, but the client that owns capture
      // changed (see |ClientWindow::capture_owner_| for details on this).
      // Notify the current owner that it lost capture.
      if (client_window->capture_owner())
        client_window->capture_owner()->OnCaptureLost(window);
      client_window->set_capture_owner(this);
    }
    return true;
  }

  ClientChange change(property_change_tracker_.get(), window,
                      ClientChangeType::kCapture);
  client_window->set_capture_owner(this);
  capture_controller->SetCapture(window);
  return capture_controller->GetCaptureWindow() == window;
}

bool WindowServiceClient::ReleaseCaptureImpl(const ClientWindowId& window_id) {
  DVLOG(3) << "ReleaseCapture window_id=" << window_id;
  aura::Window* window = GetWindowByClientId(window_id);
  if (!window) {
    DVLOG(1) << "ReleaseCapture failed (no window)";
    return false;
  }

  if (!IsClientCreatedWindow(window) && !IsClientRootWindow(window)) {
    DVLOG(1) << "ReleaseCapture failed (access denied)";
    return false;
  }

  wm::CaptureController* capture_controller = wm::CaptureController::Get();
  DCHECK(capture_controller);

  if (!capture_controller->GetCaptureWindow())
    return true;  // Capture window is already null.

  if (capture_controller->GetCaptureWindow() != window) {
    DVLOG(1) << "ReleaseCapture failed (supplied window does not have capture)";
    return false;
  }

  ClientWindow* client_window = ClientWindow::GetMayBeNull(window);
  if (client_window->capture_owner() &&
      client_window->capture_owner() != this) {
    // This client is trying to release capture, but it doesn't own capture.
    DVLOG(1) << "ReleaseCapture failed (client did not request capture)";
    return false;
  }
  client_window->set_capture_owner(nullptr);

  ClientChange change(property_change_tracker_.get(), window,
                      ClientChangeType::kCapture);
  capture_controller->ReleaseCapture(window);
  return capture_controller->GetCaptureWindow() != window;
}

bool WindowServiceClient::AddWindowImpl(const ClientWindowId& parent_id,
                                        const ClientWindowId& child_id) {
  aura::Window* parent = GetWindowByClientId(parent_id);
  aura::Window* child = GetWindowByClientId(child_id);
  DVLOG(3) << "add window client=" << client_id_
           << " client parent window_id=" << parent_id.ToString()
           << " client child window_id= " << child_id.ToString();
  if (!parent) {
    DVLOG(1) << "AddWindow failed (no parent)";
    return false;
  }
  if (!child) {
    DVLOG(1) << "AddWindow failed (no child)";
    return false;
  }
  if (child->parent() == parent) {
    DVLOG(1) << "AddWindow failed (already has parent)";
    return false;
  }
  if (child->Contains(parent)) {
    DVLOG(1) << "AddWindow failed (child contains parent)";
    return false;
  }
  if (IsClientCreatedWindow(child) && !IsTopLevel(child) &&
      (IsClientRootWindow(parent) || (IsClientCreatedWindow(parent) &&
                                      !IsWindowRootOfAnotherClient(parent)))) {
    parent->AddChild(child);
    return true;
  }
  DVLOG(1) << "AddWindow failed (access denied)";
  return false;
}

bool WindowServiceClient::RemoveWindowFromParentImpl(
    const ClientWindowId& client_window_id) {
  aura::Window* window = GetWindowByClientId(client_window_id);
  DVLOG(3) << "removing window from parent client=" << client_id_
           << " client window_id= " << client_window_id;
  if (!window) {
    DVLOG(1) << "RemoveWindowFromParent failed (invalid window id="
             << client_window_id.ToString() << ")";
    return false;
  }
  if (!window->parent()) {
    DVLOG(1) << "RemoveWindowFromParent failed (no parent id="
             << client_window_id.ToString() << ")";
    return false;
  }
  if (IsClientCreatedWindow(window) && !IsClientRootWindow(window)) {
    window->parent()->RemoveChild(window);
    return true;
  }
  DVLOG(1) << "RemoveWindowFromParent failed (access policy disallowed id="
           << client_window_id.ToString() << ")";
  return false;
}

bool WindowServiceClient::SetWindowVisibilityImpl(
    const ClientWindowId& window_id,
    bool visible) {
  aura::Window* window = GetWindowByClientId(window_id);
  DVLOG(3) << "SetWindowVisibility client=" << client_id_
           << " client window_id= " << window_id.ToString();
  if (!window) {
    DVLOG(1) << "SetWindowVisibility failed (no window)";
    return false;
  }
  if (IsClientCreatedWindow(window) ||
      (IsClientRootWindow(window) && can_change_root_window_visibility_)) {
    if (window->TargetVisibility() == visible)
      return true;
    if (visible)
      window->Show();
    else
      window->Hide();
    return true;
  }
  DVLOG(1) << "SetWindowVisibility failed (access policy denied change)";
  return false;
}

bool WindowServiceClient::SetWindowPropertyImpl(
    const ClientWindowId& window_id,
    const std::string& name,
    const base::Optional<std::vector<uint8_t>>& value) {
  aura::Window* window = GetWindowByClientId(window_id);
  DVLOG(3) << "SetWindowProperty client=" << client_id_
           << " client window_id= " << window_id.ToString();
  if (!window) {
    DVLOG(1) << "SetWindowProperty failed (no window)";
    return false;
  }
  DCHECK(window_service_->property_converter()->IsTransportNameRegistered(name))
      << "Attempting to set an unregistered property; this is not implemented. "
      << "property name=" << name;
  if (!IsClientCreatedWindow(window) && !IsClientRootWindow(window)) {
    DVLOG(1) << "SetWindowProperty failed (access policy denied change)";
    return false;
  }

  ClientChange change(property_change_tracker_.get(), window,
                      ClientChangeType::kProperty);
  std::unique_ptr<std::vector<uint8_t>> data;
  if (value.has_value())
    data = std::make_unique<std::vector<uint8_t>>(value.value());
  window_service_->property_converter()->SetPropertyFromTransportValue(
      window, name, data.get());
  return true;
}

bool WindowServiceClient::SetWindowOpacityImpl(const ClientWindowId& window_id,
                                               float opacity) {
  aura::Window* window = GetWindowByClientId(window_id);
  DVLOG(3) << "SetWindowOpacity client=" << client_id_
           << " client window_id= " << window_id.ToString();
  if (IsClientCreatedWindow(window) ||
      (IsClientRootWindow(window) && can_change_root_window_visibility_)) {
    if (window->layer()->opacity() == opacity)
      return true;
    window->layer()->SetOpacity(opacity);
    return true;
  }
  DVLOG(1) << "SetWindowOpacity failed (invalid window or access denied)";
  return false;
}

bool WindowServiceClient::SetWindowBoundsImpl(
    const ClientWindowId& window_id,
    const gfx::Rect& bounds,
    const base::Optional<viz::LocalSurfaceId>& local_surface_id) {
  // TODO: |local_surface_id| needs to be routed correctly.
  // https://crbug.com/844789.
  aura::Window* window = GetWindowByClientId(window_id);

  DVLOG(3) << "SetWindowBounds window_id=" << window_id
           << " bounds=" << bounds.ToString() << " local_surface_id="
           << (local_surface_id ? local_surface_id->ToString() : "null");

  if (!window) {
    DVLOG(1) << "SetWindowBounds failed (invalid window id)";
    return false;
  }

  // Only the owner of the window can change the bounds.
  if (!IsClientCreatedWindow(window)) {
    DVLOG(1) << "SetWindowBounds failed (access denied)";
    return false;
  }

  if (window->bounds() == bounds &&
      GetLocalSurfaceId(window) == local_surface_id) {
    return true;
  }

  ClientChange change(property_change_tracker_.get(), window,
                      ClientChangeType::kBounds);
  const gfx::Rect original_bounds = window->bounds();
  window->SetBounds(bounds);
  if (!change.window())
    return true;  // Return value doesn't matter if window destroyed.

  if (IsClientRootWindow(window)) {
    // ClientRoot handles notification in this case. Note that this
    // unconditionally returns false, because the LocalSurfaceId changes with
    // the bounds. Returning false ensures the client applies the LocalSurfaceId
    // assigned by ClientRoot and sent to the client in
    // ClientRoot::OnWindowBoundsChanged().
    return false;
  }

  if (window->bounds() == original_bounds)
    return false;

  // The window's bounds changed, but not to the value the client requested.
  // Tell the client the new value, and return false, which triggers the client
  // to use the value supplied to OnWindowBoundsChanged().
  window_tree_client_->OnWindowBoundsChanged(TransportIdForWindow(window),
                                             original_bounds, window->bounds(),
                                             local_surface_id);
  return false;
}

bool WindowServiceClient::EmbedImpl(
    const ClientWindowId& window_id,
    mojom::WindowTreeClientPtr window_tree_client_ptr,
    mojom::WindowTreeClient* window_tree_client,
    uint32_t flags) {
  DVLOG(3) << "Embed window_id=" << window_id;

  // mojom::kEmbedFlagEmbedderInterceptsEvents is inherited, otherwise an
  // embedder could effectively circumvent it by embedding itself.
  if (intercepts_events_)
    flags = mojom::kEmbedFlagEmbedderInterceptsEvents;

  aura::Window* window = GetWindowByClientId(window_id);
  if (!window) {
    DVLOG(1) << "Embed failed (no window)";
    return false;
  }
  if (!IsClientCreatedWindow(window) || IsTopLevel(window)) {
    DVLOG(1) << "Embed failed (access denied)";
    return false;
  }

  const bool intercept_events =
      (flags & mojom::kEmbedFlagEmbedderInterceptsEvents) != 0;
  std::unique_ptr<Embedding> embedding =
      std::make_unique<Embedding>(this, window);
  embedding->Init(
      window_service_, std::move(window_tree_client_ptr), window_tree_client,
      intercept_events,
      base::BindOnce(&WindowServiceClient::OnEmbeddedClientConnectionLost,
                     base::Unretained(this), embedding.get()));
  if (flags & mojom::kEmbedFlagEmbedderControlsVisibility)
    embedding->embedded_client()->can_change_root_window_visibility_ = false;
  ClientWindow::GetMayBeNull(window)->SetEmbedding(std::move(embedding));
  return true;
}

std::vector<aura::Window*> WindowServiceClient::GetWindowTreeImpl(
    const ClientWindowId& window_id) {
  aura::Window* window = GetWindowByClientId(window_id);
  std::vector<aura::Window*> results;
  GetWindowTreeRecursive(window, &results);
  return results;
}

bool WindowServiceClient::SetFocusImpl(const ClientWindowId& window_id) {
  DVLOG(3) << "SetFocus window_id=" << window_id;
  // FocusHandler deals with a null window.
  return focus_handler_.SetFocus(GetWindowByClientId(window_id));
}

void WindowServiceClient::GetWindowTreeRecursive(
    aura::Window* window,
    std::vector<aura::Window*>* windows) {
  if (!IsWindowKnown(window))
    return;

  windows->push_back(window);
  for (aura::Window* child : window->children())
    GetWindowTreeRecursive(child, windows);
}

void WindowServiceClient::OnEmbeddedClientConnectionLost(Embedding* embedding) {
  ClientWindow::GetMayBeNull(embedding->window())->SetEmbedding(nullptr);
}

void WindowServiceClient::OnWindowDestroyed(aura::Window* window) {
  DCHECK(IsWindowKnown(window));

  // WARNING: this function is not necessarily called. In particular it isn't
  // called when the client requests the window to be deleted, or from the
  // destructor.

  auto iter = FindClientRootWithRoot(window);
  if (iter != client_roots_.end()) {
    // TODO(sky): add test coverage of this. I'm pretty sure this isn't wired
    // up correctly.
    DeleteClientRoot(iter->get(),
                     WindowServiceClient::DeleteClientRootReason::kDeleted);
  }

  DCHECK_NE(0u, window_to_client_window_id_map_.count(window));
  const ClientWindowId client_window_id =
      window_to_client_window_id_map_[window];
  window_tree_client_->OnWindowDeleted(
      ClientWindowIdToTransportId(client_window_id));

  const bool delete_if_owned = false;
  RemoveWindowFromKnownWindows(window, delete_if_owned);
}

void WindowServiceClient::OnCaptureChanged(aura::Window* lost_capture,
                                           aura::Window* gained_capture) {
  if (property_change_tracker_->IsProcessingChangeForWindow(
          lost_capture, ClientChangeType::kCapture) ||
      property_change_tracker_->IsProcessingChangeForWindow(
          gained_capture, ClientChangeType::kCapture)) {
    // The client initiated the change, don't notify the client.
    return;
  }

  // Assume the environment the WindowService is running in is not requesting
  // capture on windows created by clients. With this assumption, the only time
  // the client needs to be notified is if the client had set capture on one of
  // its windows, and capture changed. This might happen if the window is no
  // longer valid for capture, or the local environment requests capture on
  // another window.
  if (lost_capture && (IsClientCreatedWindow(lost_capture) ||
                       IsClientRootWindow(lost_capture))) {
    ClientWindow* client_window = ClientWindow::GetMayBeNull(lost_capture);
    if (client_window->capture_owner() == this) {
      // One of the windows known to this client had capture. Notify the client
      // of the change. If the client does not know about the window that gained
      // capture, an invalid window id is used.
      client_window->set_capture_owner(nullptr);
      const Id gained_capture_id = gained_capture &&
                                           IsWindowKnown(gained_capture) &&
                                           !IsClientRootWindow(gained_capture)
                                       ? TransportIdForWindow(gained_capture)
                                       : kInvalidTransportId;
      window_tree_client_->OnCaptureChanged(gained_capture_id,
                                            TransportIdForWindow(lost_capture));
    }
  } else {
    DCHECK(!gained_capture || !IsClientCreatedWindow(gained_capture) ||
           IsTopLevel(gained_capture));
  }
}

void WindowServiceClient::NewWindow(
    uint32_t change_id,
    Id transport_window_id,
    const base::Optional<base::flat_map<std::string, std::vector<uint8_t>>>&
        transport_properties) {
  std::map<std::string, std::vector<uint8_t>> properties;
  if (transport_properties.has_value())
    properties = mojo::FlatMapToMap(transport_properties.value());
  window_tree_client_->OnChangeCompleted(
      change_id,
      NewWindowImpl(MakeClientWindowId(transport_window_id), properties));
}

void WindowServiceClient::NewTopLevelWindow(
    uint32_t change_id,
    Id transport_window_id,
    const base::flat_map<std::string, std::vector<uint8_t>>& properties) {
  const ClientWindowId client_window_id =
      MakeClientWindowId(transport_window_id);
  DVLOG(3) << "NewTopLevelWindow client_window_id="
           << client_window_id.ToString();
  if (!IsValidIdForNewWindow(client_window_id)) {
    DVLOG(1) << "NewTopLevelWindow failed (invalid window id)";
    window_tree_client_->OnChangeCompleted(change_id, false);
    return;
  }
  if (connection_type_ == ConnectionType::kEmbedding) {
    // This is done to disallow clients such as renderers from creating
    // top-level windows.
    DVLOG(1) << "NewTopLevelWindow failed (access denied)";
    window_tree_client_->OnChangeCompleted(change_id, false);
    return;
  }
  std::unique_ptr<aura::Window> top_level_ptr =
      window_service_->delegate()->NewTopLevel(
          window_service_->property_converter(), properties);
  if (!top_level_ptr) {
    DVLOG(1) << "NewTopLevelWindow failed (delegate window creation failed)";
    window_tree_client_->OnChangeCompleted(change_id, false);
    return;
  }
  top_level_ptr->set_owned_by_parent(false);
  const bool is_top_level = true;
  aura::Window* top_level = AddClientCreatedWindow(
      client_window_id, is_top_level, std::move(top_level_ptr));
  ClientWindow::GetMayBeNull(top_level)->set_frame_sink_id(client_window_id);
  const int64_t display_id =
      display::Screen::GetScreen()->GetDisplayNearestWindow(top_level).id();
  // This passes null for the mojom::WindowTreePtr because the client has
  // already been given the mojom::WindowTreePtr that is backed by this
  // WindowServiceClient.
  ClientRoot* client_root = CreateClientRoot(top_level, is_top_level, nullptr);
  window_tree_client_->OnTopLevelCreated(
      change_id, WindowToWindowData(top_level), display_id,
      top_level->IsVisible(), client_root->GetLocalSurfaceId());
}

void WindowServiceClient::DeleteWindow(uint32_t change_id,
                                       Id transport_window_id) {
  window_tree_client_->OnChangeCompleted(
      change_id, DeleteWindowImpl(MakeClientWindowId(transport_window_id)));
}

void WindowServiceClient::SetCapture(uint32_t change_id,
                                     Id transport_window_id) {
  window_tree_client_->OnChangeCompleted(
      change_id, SetCaptureImpl(MakeClientWindowId(transport_window_id)));
}

void WindowServiceClient::ReleaseCapture(uint32_t change_id,
                                         Id transport_window_id) {
  window_tree_client_->OnChangeCompleted(
      change_id, ReleaseCaptureImpl(MakeClientWindowId(transport_window_id)));
}

void WindowServiceClient::StartPointerWatcher(bool want_moves) {
  if (!pointer_watcher_)
    pointer_watcher_ = std::make_unique<PointerWatcher>(this);
  pointer_watcher_->set_types_to_watch(
      want_moves ? PointerWatcher::TypesToWatch::kUpDownMoveWheel
                 : PointerWatcher::TypesToWatch::kUpDown);
}

void WindowServiceClient::StopPointerWatcher() {
  pointer_watcher_.reset();
}

void WindowServiceClient::SetWindowBounds(
    uint32_t change_id,
    Id window_id,
    const gfx::Rect& bounds,
    const base::Optional<viz::LocalSurfaceId>& local_surface_id) {
  window_tree_client_->OnChangeCompleted(
      change_id, SetWindowBoundsImpl(MakeClientWindowId(window_id), bounds,
                                     local_surface_id));
}

void WindowServiceClient::SetWindowTransform(uint32_t change_id,
                                             Id window_id,
                                             const gfx::Transform& transform) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void WindowServiceClient::SetClientArea(
    Id transport_window_id,
    const gfx::Insets& insets,
    const base::Optional<std::vector<gfx::Rect>>& additional_client_areas) {
  const ClientWindowId window_id = MakeClientWindowId(transport_window_id);
  aura::Window* window = GetWindowByClientId(window_id);
  DVLOG(3) << "SetClientArea client window_id=" << window_id.ToString()
           << " insets=" << insets.ToString();
  if (!window) {
    DVLOG(1) << "SetClientArea failed (invalid window id)";
    return;
  }
  if (!IsClientRootWindow(window) || !IsTopLevel(window)) {
    DVLOG(1) << "SetClientArea failed (access denied)";
    return;
  }

  ClientWindow* client_window = ClientWindow::GetMayBeNull(window);
  DCHECK(client_window);  // Must exist because of preceeding conditionals.
  client_window->SetClientArea(
      insets, additional_client_areas.value_or(std::vector<gfx::Rect>()));
}

void WindowServiceClient::SetHitTestMask(
    Id window_id,
    const base::Optional<gfx::Rect>& mask) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void WindowServiceClient::SetCanAcceptDrops(Id window_id, bool accepts_drops) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void WindowServiceClient::SetWindowVisibility(uint32_t change_id,
                                              Id transport_window_id,
                                              bool visible) {
  window_tree_client_->OnChangeCompleted(
      change_id, SetWindowVisibilityImpl(
                     MakeClientWindowId(transport_window_id), visible));
}

void WindowServiceClient::SetWindowProperty(
    uint32_t change_id,
    Id window_id,
    const std::string& name,
    const base::Optional<std::vector<uint8_t>>& value) {
  window_tree_client_->OnChangeCompleted(
      change_id,
      SetWindowPropertyImpl(MakeClientWindowId(window_id), name, value));
}

void WindowServiceClient::SetWindowOpacity(uint32_t change_id,
                                           Id transport_window_id,
                                           float opacity) {
  window_tree_client_->OnChangeCompleted(
      change_id,
      SetWindowOpacityImpl(MakeClientWindowId(transport_window_id), opacity));
}

void WindowServiceClient::AttachCompositorFrameSink(
    Id transport_window_id,
    viz::mojom::CompositorFrameSinkRequest compositor_frame_sink,
    viz::mojom::CompositorFrameSinkClientPtr client) {
  DVLOG(3) << "AttachCompositorFrameSink id="
           << MakeClientWindowId(transport_window_id).ToString();
  aura::Window* window =
      GetWindowByClientId(MakeClientWindowId(transport_window_id));
  if (!window) {
    DVLOG(1) << "AttachCompositorFrameSink failed (invalid window id)";
    return;
  }
  ClientWindow* client_window = ClientWindow::GetMayBeNull(window);
  // If this isn't called on the root, then only allow it if there is not
  // another client embedded in the window.
  const bool allow =
      IsClientRootWindow(window) ||
      (IsClientCreatedWindow(window) &&
       client_window->embedded_window_service_client() == nullptr);
  if (!allow) {
    DVLOG(1) << "AttachCompositorFrameSink failed (policy disallowed)";
    return;
  }

  client_window->AttachCompositorFrameSink(std::move(compositor_frame_sink),
                                           std::move(client));
}

void WindowServiceClient::AddWindow(uint32_t change_id,
                                    Id parent_id,
                                    Id child_id) {
  window_tree_client_->OnChangeCompleted(
      change_id, AddWindowImpl(MakeClientWindowId(parent_id),
                               MakeClientWindowId(child_id)));
}

void WindowServiceClient::RemoveWindowFromParent(uint32_t change_id,
                                                 Id window_id) {
  window_tree_client_->OnChangeCompleted(
      change_id, RemoveWindowFromParentImpl(MakeClientWindowId(window_id)));
}

void WindowServiceClient::AddTransientWindow(uint32_t change_id,
                                             Id window_id,
                                             Id transient_window_id) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void WindowServiceClient::RemoveTransientWindowFromParent(
    uint32_t change_id,
    Id transient_window_id) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void WindowServiceClient::SetModalType(uint32_t change_id,
                                       Id window_id,
                                       ui::ModalType type) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void WindowServiceClient::SetChildModalParent(uint32_t change_id,
                                              Id window_id,
                                              Id parent_window_id) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void WindowServiceClient::ReorderWindow(uint32_t change_id,
                                        Id window_id,
                                        Id relative_window_id,
                                        ::ui::mojom::OrderDirection direction) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void WindowServiceClient::GetWindowTree(Id window_id,
                                        GetWindowTreeCallback callback) {
  std::vector<aura::Window*> windows =
      GetWindowTreeImpl(MakeClientWindowId(window_id));
  std::move(callback).Run(WindowsToWindowDatas(windows));
}

void WindowServiceClient::Embed(Id transport_window_id,
                                mojom::WindowTreeClientPtr client_ptr,
                                uint32_t embed_flags,
                                EmbedCallback callback) {
  mojom::WindowTreeClient* client = client_ptr.get();
  std::move(callback).Run(EmbedImpl(MakeClientWindowId(transport_window_id),
                                    std::move(client_ptr), client,
                                    embed_flags));
}

void WindowServiceClient::ScheduleEmbed(mojom::WindowTreeClientPtr client,
                                        ScheduleEmbedCallback callback) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void WindowServiceClient::ScheduleEmbedForExistingClient(
    uint32_t window_id,
    ScheduleEmbedForExistingClientCallback callback) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void WindowServiceClient::EmbedUsingToken(Id window_id,
                                          const base::UnguessableToken& token,
                                          uint32_t embed_flags,
                                          EmbedUsingTokenCallback callback) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void WindowServiceClient::SetFocus(uint32_t change_id, Id transport_window_id) {
  window_tree_client_->OnChangeCompleted(
      change_id, SetFocusImpl(MakeClientWindowId(transport_window_id)));
}

void WindowServiceClient::SetCanFocus(Id transport_window_id, bool can_focus) {
  focus_handler_.SetCanFocus(
      GetWindowByClientId(MakeClientWindowId(transport_window_id)), can_focus);
}

void WindowServiceClient::SetCursor(uint32_t change_id,
                                    Id window_id,
                                    ui::CursorData cursor) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void WindowServiceClient::SetWindowTextInputState(
    Id window_id,
    ::ui::mojom::TextInputStatePtr state) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void WindowServiceClient::SetImeVisibility(
    Id window_id,
    bool visible,
    ::ui::mojom::TextInputStatePtr state) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void WindowServiceClient::SetEventTargetingPolicy(
    Id transport_window_id,
    ::ui::mojom::EventTargetingPolicy policy) {
  aura::Window* window =
      GetWindowByClientId(MakeClientWindowId(transport_window_id));
  if (IsClientCreatedWindow(window) || IsClientRootWindow(window))
    window->SetEventTargetingPolicy(policy);
}

void WindowServiceClient::OnWindowInputEventAck(uint32_t event_id,
                                                mojom::EventResult result) {
  // TODO(sky): this is no longer needed, remove.
}

void WindowServiceClient::DeactivateWindow(Id window_id) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void WindowServiceClient::StackAbove(uint32_t change_id,
                                     Id above_id,
                                     Id below_id) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void WindowServiceClient::StackAtTop(uint32_t change_id, Id window_id) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void WindowServiceClient::PerformWmAction(Id window_id,
                                          const std::string& action) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void WindowServiceClient::GetWindowManagerClient(
    ::ui::mojom::WindowManagerClientAssociatedRequest internal) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void WindowServiceClient::GetCursorLocationMemory(
    GetCursorLocationMemoryCallback callback) {
  auto shared_buffer_handle =
      aura::Env::GetInstance()->GetLastMouseLocationMemory();
  DCHECK(shared_buffer_handle.is_valid());
  std::move(callback).Run(std::move(shared_buffer_handle));
}

void WindowServiceClient::PerformWindowMove(uint32_t change_id,
                                            Id window_id,
                                            ::ui::mojom::MoveLoopSource source,
                                            const gfx::Point& cursor) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void WindowServiceClient::CancelWindowMove(Id window_id) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void WindowServiceClient::PerformDragDrop(
    uint32_t change_id,
    Id source_window_id,
    const gfx::Point& screen_location,
    const base::flat_map<std::string, std::vector<uint8_t>>& drag_data,
    const SkBitmap& drag_image,
    const gfx::Vector2d& drag_image_offset,
    uint32_t drag_operation,
    ::ui::mojom::PointerKind source) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void WindowServiceClient::CancelDragDrop(Id window_id) {
  NOTIMPLEMENTED_LOG_ONCE();
}

}  // namespace ws2
}  // namespace ui
