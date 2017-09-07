// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws/server_window.h"

#include <inttypes.h>
#include <stddef.h>

#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "components/viz/host/renderer_settings_creation.h"
#include "services/ui/common/transient_window_utils.h"
#include "services/ui/public/interfaces/window_manager.mojom.h"
#include "services/ui/ws/server_window_delegate.h"
#include "services/ui/ws/server_window_observer.h"
#include "services/ui/ws/server_window_tracker.h"
#include "ui/base/cursor/cursor.h"

namespace ui {
namespace ws {

ServerWindow::ServerWindow(ServerWindowDelegate* delegate, const WindowId& id)
    : ServerWindow(delegate, id, Properties()) {}

ServerWindow::ServerWindow(ServerWindowDelegate* delegate,
                           const WindowId& id,
                           const Properties& properties)
    : delegate_(delegate),
      id_(id),
      frame_sink_id_((id_.client_id << 16) | id_.window_id, 0),
      parent_(nullptr),
      stacking_target_(nullptr),
      transient_parent_(nullptr),
      modal_type_(MODAL_TYPE_NONE),
      visible_(false),
      // Default to kPointer as kNull doesn't change the cursor, it leaves
      // the last non-null cursor.
      cursor_(ui::CursorType::kPointer),
      non_client_cursor_(ui::CursorType::kPointer),
      opacity_(1),
      can_focus_(true),
      properties_(properties),
      // Don't notify newly added observers during notification. This causes
      // problems for code that adds an observer as part of an observer
      // notification (such as ServerWindowDrawTracker).
      observers_(
          base::ObserverList<ServerWindowObserver>::NOTIFY_EXISTING_ONLY) {
  DCHECK(delegate);  // Must provide a delegate.
  // TODO(kylechar): Add method to reregister |frame_sink_id_| when viz service
  // has crashed.
  auto* host_frame_sink_manager = delegate_->GetHostFrameSinkManager();
  if (host_frame_sink_manager) {
    host_frame_sink_manager->RegisterFrameSinkId(frame_sink_id_, this);
#if DCHECK_IS_ON()
    host_frame_sink_manager->SetFrameSinkDebugLabel(frame_sink_id_, GetName());
#endif
  }
}

ServerWindow::~ServerWindow() {
  for (auto& observer : observers_)
    observer.OnWindowDestroying(this);

  if (transient_parent_)
    transient_parent_->RemoveTransientWindow(this);

  // Destroy transient children, only after we've removed ourselves from our
  // parent, as destroying an active transient child may otherwise attempt to
  // refocus us.
  Windows transient_children(transient_children_);
  for (auto* window : transient_children)
    delete window;
  DCHECK(transient_children_.empty());

  while (!children_.empty())
    children_.front()->parent()->Remove(children_.front());

  if (parent_)
    parent_->Remove(this);

  for (auto& observer : observers_)
    observer.OnWindowDestroyed(this);

  auto* host_frame_sink_manager = delegate_->GetHostFrameSinkManager();
  if (host_frame_sink_manager)
    host_frame_sink_manager->InvalidateFrameSinkId(frame_sink_id_);
}

void ServerWindow::AddObserver(ServerWindowObserver* observer) {
  observers_.AddObserver(observer);
}

void ServerWindow::RemoveObserver(ServerWindowObserver* observer) {
  DCHECK(observers_.HasObserver(observer));
  observers_.RemoveObserver(observer);
}

bool ServerWindow::HasObserver(ServerWindowObserver* observer) {
  return observers_.HasObserver(observer);
}

void ServerWindow::CreateRootCompositorFrameSink(
    gfx::AcceleratedWidget widget,
    viz::mojom::CompositorFrameSinkAssociatedRequest sink_request,
    viz::mojom::CompositorFrameSinkClientPtr client,
    viz::mojom::DisplayPrivateAssociatedRequest display_request) {
  has_created_compositor_frame_sink_ = true;
  // TODO(fsamuel): AcceleratedWidget cannot be transported over IPC for Mac
  // or Android. We should instead use GpuSurfaceTracker here on those
  // platforms.
  delegate_->GetHostFrameSinkManager()->CreateRootCompositorFrameSink(
      frame_sink_id_, widget,
      viz::CreateRendererSettings(viz::BufferToTextureTargetMap()),
      std::move(sink_request), std::move(client), std::move(display_request));
}

void ServerWindow::CreateCompositorFrameSink(
    viz::mojom::CompositorFrameSinkRequest request,
    viz::mojom::CompositorFrameSinkClientPtr client) {
  has_created_compositor_frame_sink_ = true;
  delegate_->GetHostFrameSinkManager()->CreateCompositorFrameSink(
      frame_sink_id_, std::move(request), std::move(client));
}

void ServerWindow::Add(ServerWindow* child) {
  // We assume validation checks happened already.
  DCHECK(child);
  DCHECK(child != this);
  DCHECK(!child->Contains(this));
  if (child->parent() == this) {
    if (children_.size() == 1)
      return;  // Already in the right position.
    child->Reorder(children_.back(), mojom::OrderDirection::ABOVE);
    return;
  }

  ServerWindow* old_parent = child->parent();
  for (auto& observer : child->observers_)
    observer.OnWillChangeWindowHierarchy(child, this, old_parent);

  if (child->parent())
    child->parent()->RemoveImpl(child);

  child->parent_ = this;
  children_.push_back(child);

  // Stack the child properly if it is a transient child of a sibling.
  if (child->transient_parent_ && child->transient_parent_->parent() == this)
    RestackTransientDescendants(child->transient_parent_, &GetStackingTarget,
                                &ReorderImpl);

  for (auto& observer : child->observers_)
    observer.OnWindowHierarchyChanged(child, this, old_parent);
}

void ServerWindow::Remove(ServerWindow* child) {
  // We assume validation checks happened else where.
  DCHECK(child);
  DCHECK(child != this);
  DCHECK(child->parent() == this);

  for (auto& observer : child->observers_)
    observer.OnWillChangeWindowHierarchy(child, nullptr, this);

  RemoveImpl(child);

  // Stack the child properly if it is a transient child of a sibling.
  if (child->transient_parent_ && child->transient_parent_->parent() == this)
    RestackTransientDescendants(child->transient_parent_, &GetStackingTarget,
                                &ReorderImpl);

  for (auto& observer : child->observers_)
    observer.OnWindowHierarchyChanged(child, nullptr, this);
}

void ServerWindow::Reorder(ServerWindow* relative,
                           mojom::OrderDirection direction) {
  ReorderImpl(this, relative, direction);
}

void ServerWindow::StackChildAtBottom(ServerWindow* child) {
  // There's nothing to do if the child is already at the bottom.
  if (children_.size() <= 1 || child == children_.front())
    return;
  child->Reorder(children_.front(), mojom::OrderDirection::BELOW);
}

void ServerWindow::StackChildAtTop(ServerWindow* child) {
  // There's nothing to do if the child is already at the top.
  if (children_.size() <= 1 || child == children_.back())
    return;
  child->Reorder(children_.back(), mojom::OrderDirection::ABOVE);
}

void ServerWindow::SetBounds(
    const gfx::Rect& bounds,
    const base::Optional<viz::LocalSurfaceId>& local_surface_id) {
  if (bounds_ == bounds && current_local_surface_id_ == local_surface_id)
    return;

  const gfx::Rect old_bounds = bounds_;

  current_local_surface_id_ = local_surface_id;

  bounds_ = bounds;
  for (auto& observer : observers_)
    observer.OnWindowBoundsChanged(this, old_bounds, bounds);
}

void ServerWindow::SetClientArea(
    const gfx::Insets& insets,
    const std::vector<gfx::Rect>& additional_client_areas) {
  if (client_area_ == insets &&
      additional_client_areas == additional_client_areas_) {
    return;
  }

  additional_client_areas_ = additional_client_areas;
  client_area_ = insets;
  for (auto& observer : observers_)
    observer.OnWindowClientAreaChanged(this, insets, additional_client_areas);
}

void ServerWindow::SetHitTestMask(const gfx::Rect& mask) {
  hit_test_mask_ = base::MakeUnique<gfx::Rect>(mask);
}

void ServerWindow::ClearHitTestMask() {
  hit_test_mask_.reset();
}

void ServerWindow::SetCanAcceptDrops(bool accepts_drops) {
  accepts_drops_ = accepts_drops;
}

const ServerWindow* ServerWindow::GetRootForDrawn() const {
  return delegate_->GetRootWindowForDrawn(this);
}

ServerWindow* ServerWindow::GetChildWindow(const WindowId& window_id) {
  if (id_ == window_id)
    return this;

  for (ServerWindow* child : children_) {
    ServerWindow* window = child->GetChildWindow(window_id);
    if (window)
      return window;
  }

  return nullptr;
}

bool ServerWindow::AddTransientWindow(ServerWindow* child) {
  if (child->transient_parent())
    child->transient_parent()->RemoveTransientWindow(child);

  DCHECK(std::find(transient_children_.begin(), transient_children_.end(),
                   child) == transient_children_.end());
  transient_children_.push_back(child);
  child->transient_parent_ = this;

  // Restack |child| properly above its transient parent, if they share the same
  // parent.
  if (child->parent() == parent())
    RestackTransientDescendants(this, &GetStackingTarget, &ReorderImpl);

  for (auto& observer : observers_)
    observer.OnTransientWindowAdded(this, child);
  return true;
}

void ServerWindow::RemoveTransientWindow(ServerWindow* child) {
  Windows::iterator i =
      std::find(transient_children_.begin(), transient_children_.end(), child);
  DCHECK(i != transient_children_.end());
  transient_children_.erase(i);
  DCHECK_EQ(this, child->transient_parent());
  child->transient_parent_ = nullptr;

  // If |child| and its former transient parent share the same parent, |child|
  // should be restacked properly so it is not among transient children of its
  // former parent, anymore.
  if (parent() == child->parent())
    RestackTransientDescendants(this, &GetStackingTarget, &ReorderImpl);

  for (auto& observer : observers_)
    observer.OnTransientWindowRemoved(this, child);
}

bool ServerWindow::HasTransientAncestor(const ServerWindow* window) const {
  const ServerWindow* transient_ancestor = this;
  while (transient_ancestor && transient_ancestor != window)
    transient_ancestor = transient_ancestor->transient_parent_;
  return transient_ancestor == window;
}

void ServerWindow::SetModalType(ModalType modal_type) {
  if (modal_type_ == modal_type)
    return;

  const ModalType old_modal_type = modal_type_;
  modal_type_ = modal_type;
  for (auto& observer : observers_)
    observer.OnWindowModalTypeChanged(this, old_modal_type);
}

void ServerWindow::SetChildModalParent(ServerWindow* modal_parent) {
  if (modal_parent) {
    child_modal_parent_tracker_ = base::MakeUnique<ServerWindowTracker>();
    child_modal_parent_tracker_->Add(modal_parent);
  } else {
    child_modal_parent_tracker_.reset();
  }
}

const ServerWindow* ServerWindow::GetChildModalParent() const {
  return child_modal_parent_tracker_ &&
                 !child_modal_parent_tracker_->windows().empty()
             ? *child_modal_parent_tracker_->windows().begin()
             : nullptr;
}

bool ServerWindow::Contains(const ServerWindow* window) const {
  for (const ServerWindow* parent = window; parent; parent = parent->parent_) {
    if (parent == this)
      return true;
  }
  return false;
}

void ServerWindow::SetVisible(bool value) {
  if (visible_ == value)
    return;

  for (auto& observer : observers_)
    observer.OnWillChangeWindowVisibility(this);
  visible_ = value;
  for (auto& observer : observers_)
    observer.OnWindowVisibilityChanged(this);
}

void ServerWindow::SetOpacity(float value) {
  if (value == opacity_)
    return;
  float old_opacity = opacity_;
  opacity_ = value;
  for (auto& observer : observers_)
    observer.OnWindowOpacityChanged(this, old_opacity, opacity_);
}

void ServerWindow::SetCursor(ui::CursorData value) {
  if (cursor_.IsSameAs(value))
    return;
  cursor_ = std::move(value);
  for (auto& observer : observers_)
    observer.OnWindowCursorChanged(this, cursor_);
}

void ServerWindow::SetNonClientCursor(ui::CursorData value) {
  if (non_client_cursor_.IsSameAs(value))
    return;
  non_client_cursor_ = std::move(value);
  for (auto& observer : observers_)
    observer.OnWindowNonClientCursorChanged(this, non_client_cursor_);
}

void ServerWindow::SetTransform(const gfx::Transform& transform) {
  if (transform_ == transform)
    return;

  const gfx::Transform old_transform = transform_;
  transform_ = transform;
  for (auto& observer : observers_)
    observer.OnWindowTransformChanged(this, old_transform, transform);
}

void ServerWindow::SetProperty(const std::string& name,
                               const std::vector<uint8_t>* value) {
  auto it = properties_.find(name);
  if (it != properties_.end()) {
    if (value && it->second == *value)
      return;
  } else if (!value) {
    // This property isn't set in |properties_| and |value| is nullptr, so
    // there's
    // no change.
    return;
  }

  if (value) {
    properties_[name] = *value;
  } else if (it != properties_.end()) {
    properties_.erase(it);
  }
#if DCHECK_IS_ON()
  auto* host_frame_sink_manager = delegate_->GetHostFrameSinkManager();
  if (host_frame_sink_manager && name == mojom::WindowManager::kName_Property)
    host_frame_sink_manager->SetFrameSinkDebugLabel(frame_sink_id_, GetName());
#endif

  for (auto& observer : observers_)
    observer.OnWindowSharedPropertyChanged(this, name, value);
}

std::string ServerWindow::GetName() const {
  auto it = properties_.find(mojom::WindowManager::kName_Property);
  if (it == properties_.end())
    return std::string();
  return std::string(it->second.begin(), it->second.end());
}

void ServerWindow::SetTextInputState(const ui::TextInputState& state) {
  const bool changed = !(text_input_state_ == state);
  if (changed) {
    text_input_state_ = state;
    // keyboard even if the state is not changed. So we have to notify
    // |observers_|.
    for (auto& observer : observers_)
      observer.OnWindowTextInputStateChanged(this, state);
  }
}

bool ServerWindow::IsDrawn() const {
  const ServerWindow* root = delegate_->GetRootWindowForDrawn(this);
  if (!root || !root->visible())
    return false;
  const ServerWindow* window = this;
  while (window && window != root && window->visible())
    window = window->parent();
  return root == window;
}

mojom::ShowState ServerWindow::GetShowState() const {
  auto iter = properties_.find(mojom::WindowManager::kShowState_Property);
  if (iter == properties_.end() || iter->second.empty())
    return mojom::ShowState::DEFAULT;

  return static_cast<mojom::ShowState>(iter->second[0]);
}

void ServerWindow::SetUnderlayOffset(const gfx::Vector2d& offset) {
  if (offset == underlay_offset_)
    return;

  underlay_offset_ = offset;
}

void ServerWindow::OnEmbeddedAppDisconnected() {
  for (auto& observer : observers_)
    observer.OnWindowEmbeddedAppDisconnected(this);
}

#if DCHECK_IS_ON()
std::string ServerWindow::GetDebugWindowHierarchy() const {
  std::string result;
  BuildDebugInfo(std::string(), &result);
  return result;
}

std::string ServerWindow::GetDebugWindowInfo() const {
  std::string name = GetName();
  if (name.empty())
    name = "(no name)";

  std::string frame_sink;
  if (has_created_compositor_frame_sink_)
    frame_sink = " [" + frame_sink_id_.ToString() + "]";

  return base::StringPrintf("id=%s visible=%s bounds=%s name=%s%s",
                            id_.ToString().c_str(), visible_ ? "true" : "false",
                            bounds_.ToString().c_str(), name.c_str(),
                            frame_sink.c_str());
}

void ServerWindow::BuildDebugInfo(const std::string& depth,
                                  std::string* result) const {
  *result +=
      base::StringPrintf("%s%s\n", depth.c_str(), GetDebugWindowInfo().c_str());
  for (const ServerWindow* child : children_)
    child->BuildDebugInfo(depth + "  ", result);
}
#endif  // DCHECK_IS_ON()

void ServerWindow::OnFirstSurfaceActivation(
    const viz::SurfaceInfo& surface_info) {
  delegate_->OnFirstSurfaceActivation(surface_info, this);
}

void ServerWindow::RemoveImpl(ServerWindow* window) {
  window->parent_ = nullptr;
  children_.erase(std::find(children_.begin(), children_.end(), window));
}

void ServerWindow::OnStackingChanged() {
  if (stacking_target_) {
    Windows::const_iterator window_i = std::find(
        parent()->children().begin(), parent()->children().end(), this);
    DCHECK(window_i != parent()->children().end());
    if (window_i != parent()->children().begin() &&
        (*(window_i - 1) == stacking_target_)) {
      return;
    }
  }
  RestackTransientDescendants(this, &GetStackingTarget, &ReorderImpl);
}

// static
void ServerWindow::ReorderImpl(ServerWindow* window,
                               ServerWindow* relative,
                               mojom::OrderDirection direction) {
  DCHECK(relative);
  DCHECK_NE(window, relative);
  DCHECK_EQ(window->parent(), relative->parent());

  if (!AdjustStackingForTransientWindows(&window, &relative, &direction,
                                         window->stacking_target_))
    return;

  window->parent_->children_.erase(std::find(window->parent_->children_.begin(),
                                             window->parent_->children_.end(),
                                             window));
  Windows::iterator i = std::find(window->parent_->children_.begin(),
                                  window->parent_->children_.end(), relative);
  if (direction == mojom::OrderDirection::ABOVE) {
    DCHECK(i != window->parent_->children_.end());
    window->parent_->children_.insert(++i, window);
  } else if (direction == mojom::OrderDirection::BELOW) {
    DCHECK(i != window->parent_->children_.end());
    window->parent_->children_.insert(i, window);
  }
  for (auto& observer : window->observers_)
    observer.OnWindowReordered(window, relative, direction);
  window->OnStackingChanged();
}

// static
ServerWindow** ServerWindow::GetStackingTarget(ServerWindow* window) {
  return &window->stacking_target_;
}

}  // namespace ws
}  // namespace ui
