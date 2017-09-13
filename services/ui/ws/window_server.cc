// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws/window_server.h"

#include <set>
#include <string>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "services/ui/ws/display.h"
#include "services/ui/ws/display_creation_config.h"
#include "services/ui/ws/display_manager.h"
#include "services/ui/ws/frame_generator.h"
#include "services/ui/ws/gpu_host.h"
#include "services/ui/ws/operation.h"
#include "services/ui/ws/server_window.h"
#include "services/ui/ws/user_activity_monitor.h"
#include "services/ui/ws/window_manager_access_policy.h"
#include "services/ui/ws/window_manager_display_root.h"
#include "services/ui/ws/window_manager_state.h"
#include "services/ui/ws/window_manager_window_tree_factory.h"
#include "services/ui/ws/window_server_delegate.h"
#include "services/ui/ws/window_tree.h"
#include "services/ui/ws/window_tree_binding.h"
#include "ui/gfx/geometry/size_conversions.h"

namespace ui {
namespace ws {

namespace {

// Returns true if |window| is considered the active window manager for
// |display|.
bool IsWindowConsideredWindowManagerRoot(const Display* display,
                                         const ServerWindow* window) {
  if (!display)
    return false;

  const WindowManagerDisplayRoot* display_root =
      display->GetActiveWindowManagerDisplayRoot();
  return display_root && display_root->GetClientVisibleRoot() == window;
}

}  // namespace

struct WindowServer::CurrentMoveLoopState {
  uint32_t change_id;
  ServerWindow* window;
  WindowTree* initiator;
  gfx::Rect revert_bounds;
};

struct WindowServer::CurrentDragLoopState {
  uint32_t change_id;
  ServerWindow* window;
  WindowTree* initiator;
};

WindowServer::WindowServer(WindowServerDelegate* delegate)
    : delegate_(delegate),
      next_client_id_(kWindowServerClientId + 1),
      display_manager_(new DisplayManager(this, &user_id_tracker_)),
      current_operation_(nullptr),
      in_destructor_(false),
      next_wm_change_id_(0),
      window_manager_window_tree_factory_set_(this, &user_id_tracker_),
      host_frame_sink_manager_(base::MakeUnique<viz::HostFrameSinkManager>()),
      display_creation_config_(DisplayCreationConfig::UNKNOWN) {
  user_id_tracker_.AddObserver(this);
  OnUserIdAdded(user_id_tracker_.active_id());
}

WindowServer::~WindowServer() {
  in_destructor_ = true;

  for (auto& pair : tree_map_)
    pair.second->PrepareForWindowServerShutdown();

  // Shutdown GPU before destroying PlatformWindows for displays so that
  // GLSurfaces corresponding to a windows AcceleratedWidget gets destroyed
  // first.
  gpu_host_.reset();

  // Destroys the window trees results in querying for the display. Tear down
  // the displays first so that the trees are notified of the display going
  // away while the display is still valid.
  display_manager_->DestroyAllDisplays();

  while (!tree_map_.empty())
    DestroyTree(tree_map_.begin()->second.get());

  display_manager_.reset();
}

void WindowServer::SetDisplayCreationConfig(DisplayCreationConfig config) {
  DCHECK(tree_map_.empty());
  DCHECK_EQ(DisplayCreationConfig::UNKNOWN, display_creation_config_);
  display_creation_config_ = config;
  display_manager_->OnDisplayCreationConfigSet();
}

void WindowServer::SetGpuHost(std::unique_ptr<GpuHost> gpu_host) {
  gpu_host_ = std::move(gpu_host);
  CreateFrameSinkManager();
}

ThreadedImageCursorsFactory* WindowServer::GetThreadedImageCursorsFactory() {
  return delegate()->GetThreadedImageCursorsFactory();
}

ServerWindow* WindowServer::CreateServerWindow(
    const WindowId& id,
    const viz::FrameSinkId& frame_sink_id,
    const std::map<std::string, std::vector<uint8_t>>& properties) {
  ServerWindow* window = new ServerWindow(this, id, frame_sink_id, properties);
  window->AddObserver(this);
  return window;
}

ClientSpecificId WindowServer::GetAndAdvanceNextClientId() {
  const ClientSpecificId id = next_client_id_++;
  DCHECK_LT(id, next_client_id_);
  return id;
}

WindowTree* WindowServer::EmbedAtWindow(
    ServerWindow* root,
    const UserId& user_id,
    mojom::WindowTreeClientPtr client,
    uint32_t flags,
    std::unique_ptr<AccessPolicy> access_policy) {
  std::unique_ptr<WindowTree> tree_ptr(
      new WindowTree(this, user_id, root, std::move(access_policy)));
  WindowTree* tree = tree_ptr.get();
  if (flags & mojom::kEmbedFlagEmbedderInterceptsEvents)
    tree->set_embedder_intercepts_events();

  mojom::WindowTreePtr window_tree_ptr;
  auto window_tree_request = mojo::MakeRequest(&window_tree_ptr);
  std::unique_ptr<WindowTreeBinding> binding =
      delegate_->CreateWindowTreeBinding(
          WindowServerDelegate::BindingType::EMBED, this, tree,
          &window_tree_request, &client);
  if (!binding) {
    binding = base::MakeUnique<ws::DefaultWindowTreeBinding>(
        tree, this, std::move(window_tree_request), std::move(client));
  }

  AddTree(std::move(tree_ptr), std::move(binding), std::move(window_tree_ptr));
  OnTreeMessagedClient(tree->id());
  return tree;
}

void WindowServer::AddTree(std::unique_ptr<WindowTree> tree_impl_ptr,
                           std::unique_ptr<WindowTreeBinding> binding,
                           mojom::WindowTreePtr tree_ptr) {
  CHECK_EQ(0u, tree_map_.count(tree_impl_ptr->id()));
  WindowTree* tree = tree_impl_ptr.get();
  tree_map_[tree->id()] = std::move(tree_impl_ptr);
  tree->Init(std::move(binding), std::move(tree_ptr));
}

WindowTree* WindowServer::CreateTreeForWindowManager(
    const UserId& user_id,
    mojom::WindowTreeRequest window_tree_request,
    mojom::WindowTreeClientPtr window_tree_client,
    bool automatically_create_display_roots) {
  delegate_->OnWillCreateTreeForWindowManager(
      automatically_create_display_roots);

  std::unique_ptr<WindowTree> window_tree(new WindowTree(
      this, user_id, nullptr, base::WrapUnique(new WindowManagerAccessPolicy)));
  std::unique_ptr<WindowTreeBinding> window_tree_binding =
      delegate_->CreateWindowTreeBinding(
          WindowServerDelegate::BindingType::WINDOW_MANAGER, this,
          window_tree.get(), &window_tree_request, &window_tree_client);
  if (!window_tree_binding) {
    window_tree_binding = base::MakeUnique<DefaultWindowTreeBinding>(
        window_tree.get(), this, std::move(window_tree_request),
        std::move(window_tree_client));
  }
  WindowTree* window_tree_ptr = window_tree.get();
  AddTree(std::move(window_tree), std::move(window_tree_binding), nullptr);
  window_tree_ptr->ConfigureWindowManager(automatically_create_display_roots);
  return window_tree_ptr;
}

void WindowServer::DestroyTree(WindowTree* tree) {
  std::unique_ptr<WindowTree> tree_ptr;
  {
    auto iter = tree_map_.find(tree->id());
    DCHECK(iter != tree_map_.end());
    tree_ptr = std::move(iter->second);
    tree_map_.erase(iter);
  }

  // Notify remaining connections so that they can cleanup.
  for (auto& pair : tree_map_)
    pair.second->OnWindowDestroyingTreeImpl(tree);

  window_manager_window_tree_factory_set_.DeleteFactoryAssociatedWithTree(tree);

  // Remove any requests from the client that resulted in a call to the window
  // manager and we haven't gotten a response back yet.
  std::set<uint32_t> to_remove;
  for (auto& pair : in_flight_wm_change_map_) {
    if (pair.second.client_id == tree->id())
      to_remove.insert(pair.first);
  }
  for (uint32_t id : to_remove)
    in_flight_wm_change_map_.erase(id);
}

WindowTree* WindowServer::GetTreeWithId(ClientSpecificId client_id) {
  auto iter = tree_map_.find(client_id);
  return iter == tree_map_.end() ? nullptr : iter->second.get();
}

WindowTree* WindowServer::GetTreeWithClientName(
    const std::string& client_name) {
  for (const auto& entry : tree_map_) {
    if (entry.second->name() == client_name)
      return entry.second.get();
  }
  return nullptr;
}

ServerWindow* WindowServer::GetWindow(const WindowId& id) {
  // kWindowServerClientId is used for Display and WindowManager nodes.
  if (id.client_id == kWindowServerClientId) {
    for (Display* display : display_manager_->displays()) {
      ServerWindow* window = display->GetRootWithId(id);
      if (window)
        return window;
    }
    // WindowManagerDisplayRoots are destroyed by the client and not held by
    // the Display.
    for (auto& pair : tree_map_) {
      if (pair.second->window_manager_state()) {
        ServerWindow* window =
            pair.second->window_manager_state()->GetOrphanedRootWithId(id);
        if (window)
          return window;
      }
    }
  }
  WindowTree* tree = GetTreeWithId(id.client_id);
  return tree ? tree->GetWindow(id) : nullptr;
}

void WindowServer::OnTreeMessagedClient(ClientSpecificId id) {
  if (current_operation_)
    current_operation_->MarkTreeAsMessaged(id);
}

bool WindowServer::DidTreeMessageClient(ClientSpecificId id) const {
  return current_operation_ && current_operation_->DidMessageTree(id);
}

const WindowTree* WindowServer::GetTreeWithRoot(
    const ServerWindow* window) const {
  if (!window)
    return nullptr;
  for (auto& pair : tree_map_) {
    if (pair.second->HasRoot(window))
      return pair.second.get();
  }
  return nullptr;
}

UserActivityMonitor* WindowServer::GetUserActivityMonitorForUser(
    const UserId& user_id) {
  DCHECK_GT(activity_monitor_map_.count(user_id), 0u);
  return activity_monitor_map_[user_id].get();
}

bool WindowServer::SetFocusedWindow(ServerWindow* window) {
  // TODO(sky): this should fail if there is modal dialog active and |window|
  // is outside that.
  ServerWindow* currently_focused = GetFocusedWindow();
  Display* focused_display =
      currently_focused
          ? display_manager_->GetDisplayContaining(currently_focused)
          : nullptr;
  if (!window)
    return focused_display ? focused_display->SetFocusedWindow(nullptr) : true;

  Display* display = display_manager_->GetDisplayContaining(window);
  DCHECK(display);  // It's assumed callers do validation before calling this.
  const bool result = display->SetFocusedWindow(window);
  // If the focus actually changed, and focus was in another display, then we
  // need to notify the previously focused display so that it cleans up state
  // and notifies appropriately.
  if (result && display->GetFocusedWindow() && display != focused_display &&
      focused_display) {
    const bool cleared_focus = focused_display->SetFocusedWindow(nullptr);
    DCHECK(cleared_focus);
  }
  return result;
}

ServerWindow* WindowServer::GetFocusedWindow() {
  for (Display* display : display_manager_->displays()) {
    ServerWindow* focused_window = display->GetFocusedWindow();
    if (focused_window)
      return focused_window;
  }
  return nullptr;
}

void WindowServer::SetHighContrastMode(const UserId& user, bool enabled) {
  // TODO(fsamuel): This doesn't really seem like it's a window server concept?
  if (IsUserInHighContrastMode(user) == enabled)
    return;
  high_contrast_mode_[user] = enabled;

  if (user == user_id_tracker_.active_id()) {
    // Propagate the change to all Displays so that FrameGenerators start
    // requesting BeginFrames.
    display_manager_->SetHighContrastMode(enabled);
  }
}

uint32_t WindowServer::GenerateWindowManagerChangeId(
    WindowTree* source,
    uint32_t client_change_id) {
  const uint32_t wm_change_id = next_wm_change_id_++;
  in_flight_wm_change_map_[wm_change_id] = {source->id(), client_change_id};
  return wm_change_id;
}

void WindowServer::WindowManagerChangeCompleted(
    uint32_t window_manager_change_id,
    bool success) {
  InFlightWindowManagerChange change;
  if (!GetAndClearInFlightWindowManagerChange(window_manager_change_id,
                                              &change)) {
    return;
  }

  WindowTree* tree = GetTreeWithId(change.client_id);
  tree->OnChangeCompleted(change.client_change_id, success);
}

void WindowServer::WindowManagerCreatedTopLevelWindow(
    WindowTree* wm_tree,
    uint32_t window_manager_change_id,
    const ServerWindow* window) {
  InFlightWindowManagerChange change;
  if (!GetAndClearInFlightWindowManagerChange(window_manager_change_id,
                                              &change)) {
    DVLOG(1) << "WindowManager responded with invalid change id; most "
             << "likely bug in WindowManager processing WmCreateTopLevelWindow "
             << "change_id=" << window_manager_change_id;
    return;
  }

  WindowTree* tree = GetTreeWithId(change.client_id);
  // The window manager should have created the window already, and it should
  // be ready for embedding.
  if (!tree->IsWaitingForNewTopLevelWindow(window_manager_change_id)) {
    DVLOG(1) << "WindowManager responded with valid change id, but client "
             << "is not waiting for top-level; possible bug in mus, change_id="
             << window_manager_change_id;
    WindowManagerSentBogusMessage();
    return;
  }
  if (window && (window->id().client_id != wm_tree->id() ||
                 !window->children().empty() || GetTreeWithRoot(window))) {
    DVLOG(1)
        << "WindowManager responded with invalid window; window should "
        << "not have any children, not be the root of a client and should be "
        << "created by the WindowManager, window_manager_change_id="
        << window_manager_change_id;
    WindowManagerSentBogusMessage();
    return;
  }

  tree->OnWindowManagerCreatedTopLevelWindow(window_manager_change_id,
                                             change.client_change_id, window);
}

void WindowServer::ProcessWindowBoundsChanged(
    const ServerWindow* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds,
    const base::Optional<viz::LocalSurfaceId>& local_surface_id) {
  for (auto& pair : tree_map_) {
    pair.second->ProcessWindowBoundsChanged(window, old_bounds, new_bounds,
                                            IsOperationSource(pair.first),
                                            local_surface_id);
  }
}

void WindowServer::ProcessWindowTransformChanged(
    const ServerWindow* window,
    const gfx::Transform& old_transform,
    const gfx::Transform& new_transform) {
  for (auto& pair : tree_map_) {
    pair.second->ProcessWindowTransformChanged(
        window, old_transform, new_transform, IsOperationSource(pair.first));
  }
}

void WindowServer::ProcessClientAreaChanged(
    const ServerWindow* window,
    const gfx::Insets& new_client_area,
    const std::vector<gfx::Rect>& new_additional_client_areas) {
  for (auto& pair : tree_map_) {
    pair.second->ProcessClientAreaChanged(window, new_client_area,
                                          new_additional_client_areas,
                                          IsOperationSource(pair.first));
  }
}

void WindowServer::ProcessCaptureChanged(const ServerWindow* new_capture,
                                         const ServerWindow* old_capture) {
  for (auto& pair : tree_map_) {
    pair.second->ProcessCaptureChanged(new_capture, old_capture,
                                       IsOperationSource(pair.first));
  }
}

void WindowServer::ProcessWillChangeWindowHierarchy(
    const ServerWindow* window,
    const ServerWindow* new_parent,
    const ServerWindow* old_parent) {
  for (auto& pair : tree_map_) {
    pair.second->ProcessWillChangeWindowHierarchy(
        window, new_parent, old_parent, IsOperationSource(pair.first));
  }
}

void WindowServer::ProcessWindowHierarchyChanged(
    const ServerWindow* window,
    const ServerWindow* new_parent,
    const ServerWindow* old_parent) {
  for (auto& pair : tree_map_) {
    pair.second->ProcessWindowHierarchyChanged(window, new_parent, old_parent,
                                               IsOperationSource(pair.first));
  }
}

void WindowServer::ProcessWindowReorder(const ServerWindow* window,
                                        const ServerWindow* relative_window,
                                        const mojom::OrderDirection direction) {
  // We'll probably do a bit of reshuffling when we add a transient window.
  if ((current_operation_type() == OperationType::ADD_TRANSIENT_WINDOW) ||
      (current_operation_type() ==
       OperationType::REMOVE_TRANSIENT_WINDOW_FROM_PARENT)) {
    return;
  }
  for (auto& pair : tree_map_) {
    pair.second->ProcessWindowReorder(window, relative_window, direction,
                                      IsOperationSource(pair.first));
  }
}

void WindowServer::ProcessWindowDeleted(ServerWindow* window) {
  for (auto& pair : tree_map_)
    pair.second->ProcessWindowDeleted(window, IsOperationSource(pair.first));
}

void WindowServer::ProcessWillChangeWindowCursor(ServerWindow* window,
                                                 const ui::CursorData& cursor) {
  for (auto& pair : tree_map_) {
    pair.second->ProcessCursorChanged(window, cursor,
                                      IsOperationSource(pair.first));
  }
}

void WindowServer::SendToPointerWatchers(const ui::Event& event,
                                         const UserId& user_id,
                                         ServerWindow* target_window,
                                         WindowTree* ignore_tree,
                                         int64_t display_id) {
  for (auto& pair : tree_map_) {
    WindowTree* tree = pair.second.get();
    if (tree->user_id() == user_id && tree != ignore_tree)
      tree->SendToPointerWatcher(event, target_window, display_id);
  }
}

void WindowServer::SetPaintCallback(
    const base::Callback<void(ServerWindow*)>& callback) {
  DCHECK(delegate_->IsTestConfig()) << "Paint callbacks are expensive, and "
                                    << "allowed only in tests.";
  DCHECK(window_paint_callback_.is_null() || callback.is_null());
  window_paint_callback_ = callback;
}

void WindowServer::StartMoveLoop(uint32_t change_id,
                                 ServerWindow* window,
                                 WindowTree* initiator,
                                 const gfx::Rect& revert_bounds) {
  current_move_loop_.reset(
      new CurrentMoveLoopState{change_id, window, initiator, revert_bounds});
}

void WindowServer::EndMoveLoop() {
  current_move_loop_.reset();
}

uint32_t WindowServer::GetCurrentMoveLoopChangeId() {
  if (current_move_loop_)
    return current_move_loop_->change_id;
  return 0;
}

ServerWindow* WindowServer::GetCurrentMoveLoopWindow() {
  if (current_move_loop_)
    return current_move_loop_->window;
  return nullptr;
}

WindowTree* WindowServer::GetCurrentMoveLoopInitiator() {
  if (current_move_loop_)
    return current_move_loop_->initiator;
  return nullptr;
}

gfx::Rect WindowServer::GetCurrentMoveLoopRevertBounds() {
  if (current_move_loop_)
    return current_move_loop_->revert_bounds;
  return gfx::Rect();
}

void WindowServer::StartDragLoop(uint32_t change_id,
                                 ServerWindow* window,
                                 WindowTree* initiator) {
  current_drag_loop_.reset(
      new CurrentDragLoopState{change_id, window, initiator});
}

void WindowServer::EndDragLoop() {
  current_drag_loop_.reset();
}

uint32_t WindowServer::GetCurrentDragLoopChangeId() {
  if (current_drag_loop_)
    return current_drag_loop_->change_id;
  return 0u;
}

ServerWindow* WindowServer::GetCurrentDragLoopWindow() {
  if (current_drag_loop_)
    return current_drag_loop_->window;
  return nullptr;
}

WindowTree* WindowServer::GetCurrentDragLoopInitiator() {
  if (current_drag_loop_)
    return current_drag_loop_->initiator;
  return nullptr;
}

void WindowServer::OnDisplayReady(Display* display, bool is_first) {
  if (is_first)
    delegate_->OnFirstDisplayReady();
  gpu_host_->OnAcceleratedWidgetAvailable(
      display->platform_display()->GetAcceleratedWidget());
}

void WindowServer::OnDisplayDestroyed(Display* display) {
  if (gpu_host_) {
    gpu_host_->OnAcceleratedWidgetDestroyed(
        display->platform_display()->GetAcceleratedWidget());
  }
}

void WindowServer::OnNoMoreDisplays() {
  delegate_->OnNoMoreDisplays();
}

WindowManagerState* WindowServer::GetWindowManagerStateForUser(
    const UserId& user_id) {
  return window_manager_window_tree_factory_set_.GetWindowManagerStateForUser(
      user_id);
}

viz::HostFrameSinkManager* WindowServer::GetHostFrameSinkManager() {
  return host_frame_sink_manager_.get();
}

void WindowServer::OnFirstSurfaceActivation(
    const viz::SurfaceInfo& surface_info,
    ServerWindow* window) {
  // This is only used for testing to observe that a window has a
  // CompositorFrame.
  if (!window_paint_callback_.is_null())
    window_paint_callback_.Run(window);

  Display* display = display_manager_->GetDisplayContaining(window);
  if (IsWindowConsideredWindowManagerRoot(display, window)) {
    // A new surface for a WindowManager root has been created. This is a
    // special case because ServerWindows created by the WindowServer are not
    // part of a WindowTree. Send the SurfaceId directly to FrameGenerator and
    // claim the temporary reference for the display root.
    display->platform_display()->GetFrameGenerator()->OnFirstSurfaceActivation(
        surface_info);
    host_frame_sink_manager_->AssignTemporaryReference(
        surface_info.id(), display->root_window()->frame_sink_id());
    return;
  }

  HandleTemporaryReferenceForNewSurface(surface_info.id(), window);

  // We always use the owner of the window's id (even for an embedded window),
  // because an embedded window's id is allocated by the parent's window tree.
  WindowTree* window_tree = GetTreeWithId(window->id().client_id);
  if (window_tree)
    window_tree->ProcessWindowSurfaceChanged(window, surface_info);
}

bool WindowServer::GetFrameDecorationsForUser(
    const UserId& user_id,
    mojom::FrameDecorationValuesPtr* values) {
  WindowManagerState* window_manager_state =
      window_manager_window_tree_factory_set_.GetWindowManagerStateForUser(
          user_id);
  if (!window_manager_state)
    return false;
  if (values && window_manager_state->got_frame_decoration_values())
    *values = window_manager_state->frame_decoration_values().Clone();
  return window_manager_state->got_frame_decoration_values();
}

int64_t WindowServer::GetInternalDisplayId() {
  return display_manager_->GetInternalDisplayId();
}

bool WindowServer::GetAndClearInFlightWindowManagerChange(
    uint32_t window_manager_change_id,
    InFlightWindowManagerChange* change) {
  // There are valid reasons as to why we wouldn't know about the id. The
  // most likely is the client disconnected before the response from the window
  // manager came back.
  auto iter = in_flight_wm_change_map_.find(window_manager_change_id);
  if (iter == in_flight_wm_change_map_.end())
    return false;

  *change = iter->second;
  in_flight_wm_change_map_.erase(iter);
  return true;
}

void WindowServer::PrepareForOperation(Operation* op) {
  // Should only ever have one change in flight.
  CHECK(!current_operation_);
  current_operation_ = op;
}

void WindowServer::FinishOperation() {
  // PrepareForOperation/FinishOperation should be balanced.
  CHECK(current_operation_);
  current_operation_ = nullptr;
}

void WindowServer::UpdateNativeCursorFromMouseLocation(ServerWindow* window) {
  WindowManagerDisplayRoot* display_root =
      display_manager_->GetWindowManagerDisplayRoot(window);
  if (display_root) {
    EventDispatcher* event_dispatcher =
        display_root->window_manager_state()->event_dispatcher();
    event_dispatcher->UpdateCursorProviderByLastKnownLocation();
  }
}

void WindowServer::UpdateNativeCursorIfOver(ServerWindow* window) {
  WindowManagerDisplayRoot* display_root =
      display_manager_->GetWindowManagerDisplayRoot(window);
  if (!display_root)
    return;

  EventDispatcher* event_dispatcher =
      display_root->window_manager_state()->event_dispatcher();
  if (window != event_dispatcher->GetWindowForMouseCursor())
    return;

  event_dispatcher->UpdateNonClientAreaForCurrentWindow();
}

bool WindowServer::IsUserInHighContrastMode(const UserId& user) const {
  const auto iter = high_contrast_mode_.find(user);
  return (iter == high_contrast_mode_.end()) ? false : iter->second;
}

void WindowServer::HandleTemporaryReferenceForNewSurface(
    const viz::SurfaceId& surface_id,
    ServerWindow* window) {
  // TODO(kylechar): Investigate adding tests for this.
  const ClientSpecificId window_client_id = window->id().client_id;

  // Find the root ServerWindow for the client that embeds |window|, which is
  // the root of the client that embeds |surface_id|. The client that embeds
  // |surface_id| created |window|, so |window| will have the client id of the
  // embedder. The root window of the embedder will have been created by it's
  // embedder, so the first ServerWindow with a different client id will be the
  // root of the embedder.
  ServerWindow* current = window->parent();
  while (current && current->id().client_id == window_client_id)
    current = current->parent();

  // The client that embeds |window| is expected to submit a CompositorFrame
  // that references |surface_id|. Have the parent claim ownership of the
  // temporary reference to |surface_id|. If the parent client crashes before it
  // adds a surface reference then the GPU can cleanup temporary references. If
  // no parent client embeds |window| then tell the GPU to drop the temporary
  // reference immediately.
  if (current) {
    host_frame_sink_manager_->AssignTemporaryReference(
        surface_id, current->frame_sink_id());
  } else {
    host_frame_sink_manager_->DropTemporaryReference(surface_id);
  }
}

void WindowServer::CreateFrameSinkManager() {
  viz::mojom::FrameSinkManagerPtr frame_sink_manager;
  viz::mojom::FrameSinkManagerRequest frame_sink_manager_request =
      mojo::MakeRequest(&frame_sink_manager);
  viz::mojom::FrameSinkManagerClientPtr frame_sink_manager_client;
  viz::mojom::FrameSinkManagerClientRequest frame_sink_manager_client_request =
      mojo::MakeRequest(&frame_sink_manager_client);
  gpu_host_->CreateFrameSinkManager(std::move(frame_sink_manager_request),
                                    std::move(frame_sink_manager_client));
  host_frame_sink_manager_->BindAndSetManager(
      std::move(frame_sink_manager_client_request), nullptr /* task_runner */,
      std::move(frame_sink_manager));
}

ServerWindow* WindowServer::GetRootWindowForDrawn(const ServerWindow* window) {
  Display* display = display_manager_->GetDisplayContaining(window);
  return display ? display->root_window() : nullptr;
}

void WindowServer::OnWindowDestroyed(ServerWindow* window) {
  ProcessWindowDeleted(window);
}

void WindowServer::OnWillChangeWindowHierarchy(ServerWindow* window,
                                               ServerWindow* new_parent,
                                               ServerWindow* old_parent) {
  if (in_destructor_)
    return;

  ProcessWillChangeWindowHierarchy(window, new_parent, old_parent);
}

void WindowServer::OnWindowHierarchyChanged(ServerWindow* window,
                                            ServerWindow* new_parent,
                                            ServerWindow* old_parent) {
  if (in_destructor_)
    return;

  WindowManagerDisplayRoot* display_root =
      display_manager_->GetWindowManagerDisplayRoot(window);
  if (display_root) {
    display_root->window_manager_state()
        ->ReleaseCaptureBlockedByAnyModalWindow();
  }

  ProcessWindowHierarchyChanged(window, new_parent, old_parent);

  if (old_parent) {
    host_frame_sink_manager_->UnregisterFrameSinkHierarchy(
        old_parent->frame_sink_id(), window->frame_sink_id());
  }
  if (new_parent) {
    host_frame_sink_manager_->RegisterFrameSinkHierarchy(
        new_parent->frame_sink_id(), window->frame_sink_id());
  }

  if (!pending_system_modal_windows_.windows().empty()) {
    // Windows that are now in a display are put here, then removed. We do this
    // in two passes to avoid removing from a list we're iterating over.
    std::set<ServerWindow*> no_longer_pending;
    for (ServerWindow* system_modal_window :
         pending_system_modal_windows_.windows()) {
      DCHECK_EQ(MODAL_TYPE_SYSTEM, system_modal_window->modal_type());
      WindowManagerDisplayRoot* display_root =
          display_manager_->GetWindowManagerDisplayRoot(system_modal_window);
      if (display_root) {
        no_longer_pending.insert(system_modal_window);
        display_root->window_manager_state()->AddSystemModalWindow(window);
      }
    }

    for (ServerWindow* system_modal_window : no_longer_pending)
      pending_system_modal_windows_.Remove(system_modal_window);
  }

  UpdateNativeCursorFromMouseLocation(window);
}

void WindowServer::OnWindowBoundsChanged(ServerWindow* window,
                                         const gfx::Rect& old_bounds,
                                         const gfx::Rect& new_bounds) {
  if (in_destructor_)
    return;

  ProcessWindowBoundsChanged(window, old_bounds, new_bounds,
                             window->current_local_surface_id());
  UpdateNativeCursorFromMouseLocation(window);
}

void WindowServer::OnWindowTransformChanged(
    ServerWindow* window,
    const gfx::Transform& old_transform,
    const gfx::Transform& new_transform) {
  if (in_destructor_)
    return;

  ProcessWindowTransformChanged(window, old_transform, new_transform);
  UpdateNativeCursorFromMouseLocation(window);
}

void WindowServer::OnWindowClientAreaChanged(
    ServerWindow* window,
    const gfx::Insets& new_client_area,
    const std::vector<gfx::Rect>& new_additional_client_areas) {
  if (in_destructor_)
    return;

  ProcessClientAreaChanged(window, new_client_area,
                           new_additional_client_areas);

  UpdateNativeCursorIfOver(window);
}

void WindowServer::OnWindowReordered(ServerWindow* window,
                                     ServerWindow* relative,
                                     mojom::OrderDirection direction) {
  ProcessWindowReorder(window, relative, direction);
  UpdateNativeCursorFromMouseLocation(window);
}

void WindowServer::OnWillChangeWindowVisibility(ServerWindow* window) {
  if (in_destructor_)
    return;

  for (auto& pair : tree_map_) {
    pair.second->ProcessWillChangeWindowVisibility(
        window, IsOperationSource(pair.first));
  }
}

void WindowServer::OnWindowOpacityChanged(ServerWindow* window,
                                          float old_opacity,
                                          float new_opacity) {
  DCHECK(!in_destructor_);

  for (auto& pair : tree_map_) {
    pair.second->ProcessWindowOpacityChanged(window, old_opacity, new_opacity,
                                             IsOperationSource(pair.first));
  }
}

void WindowServer::OnWindowVisibilityChanged(ServerWindow* window) {
  if (in_destructor_)
    return;

  WindowManagerDisplayRoot* display_root =
      display_manager_->GetWindowManagerDisplayRoot(window);
  if (display_root) {
    display_root->window_manager_state()
        ->ReleaseCaptureBlockedByAnyModalWindow();
  }
}

void WindowServer::OnWindowCursorChanged(ServerWindow* window,
                                         const ui::CursorData& cursor) {
  if (in_destructor_)
    return;

  ProcessWillChangeWindowCursor(window, cursor);

  UpdateNativeCursorIfOver(window);
}

void WindowServer::OnWindowNonClientCursorChanged(
    ServerWindow* window,
    const ui::CursorData& cursor) {
  if (in_destructor_)
    return;

  UpdateNativeCursorIfOver(window);
}

void WindowServer::OnWindowSharedPropertyChanged(
    ServerWindow* window,
    const std::string& name,
    const std::vector<uint8_t>* new_data) {
  for (auto& pair : tree_map_) {
    pair.second->ProcessWindowPropertyChanged(window, name, new_data,
                                              IsOperationSource(pair.first));
  }
}

void WindowServer::OnWindowTextInputStateChanged(
    ServerWindow* window,
    const ui::TextInputState& state) {
  Display* display = display_manager_->GetDisplayContaining(window);
  display->UpdateTextInputState(window, state);
}

void WindowServer::OnTransientWindowAdded(ServerWindow* window,
                                          ServerWindow* transient_child) {
  for (auto& pair : tree_map_) {
    pair.second->ProcessTransientWindowAdded(window, transient_child,
                                             IsOperationSource(pair.first));
  }
}

void WindowServer::OnTransientWindowRemoved(ServerWindow* window,
                                            ServerWindow* transient_child) {
  // If we're deleting a window, then this is a superfluous message.
  if (current_operation_type() == OperationType::DELETE_WINDOW)
    return;
  for (auto& pair : tree_map_) {
    pair.second->ProcessTransientWindowRemoved(window, transient_child,
                                               IsOperationSource(pair.first));
  }
}

void WindowServer::OnWindowModalTypeChanged(ServerWindow* window,
                                            ModalType old_modal_type) {
  WindowManagerDisplayRoot* display_root =
      display_manager_->GetWindowManagerDisplayRoot(window);
  if (window->modal_type() == MODAL_TYPE_SYSTEM) {
    if (display_root)
      display_root->window_manager_state()->AddSystemModalWindow(window);
    else
      pending_system_modal_windows_.Add(window);
  } else {
    pending_system_modal_windows_.Remove(window);
  }

  if (display_root && window->modal_type() != MODAL_TYPE_NONE) {
    display_root->window_manager_state()
        ->ReleaseCaptureBlockedByAnyModalWindow();
  }
}

void WindowServer::OnGpuServiceInitialized() {
  delegate_->StartDisplayInit();
}

void WindowServer::OnActiveUserIdChanged(const UserId& previously_active_id,
                                         const UserId& active_id) {
}

void WindowServer::OnUserIdAdded(const UserId& id) {
  activity_monitor_map_[id] = base::MakeUnique<UserActivityMonitor>(nullptr);
}

void WindowServer::OnUserIdRemoved(const UserId& id) {
  activity_monitor_map_.erase(id);
}

}  // namespace ws
}  // namespace ui
