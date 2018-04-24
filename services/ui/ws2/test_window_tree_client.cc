// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws2/test_window_tree_client.h"

#include <utility>

#include "ui/aura/window.h"
#include "ui/gfx/geometry/rect.h"

namespace ui {
namespace ws2 {

TestWindowTreeClient::TestWindowTreeClient() {
  tracker_.set_delegate(this);
}

TestWindowTreeClient::~TestWindowTreeClient() = default;

void TestWindowTreeClient::OnChangeAdded() {}

void TestWindowTreeClient::OnEmbed(
    mojom::WindowDataPtr root,
    mojom::WindowTreePtr tree,
    int64_t display_id,
    Id focused_window_id,
    bool drawn,
    const base::Optional<viz::LocalSurfaceId>& local_surface_id) {
  root_window_id_ = root->window_id;
  tree_ = std::move(tree);
  tracker_.OnEmbed(std::move(root), drawn);
}

void TestWindowTreeClient::OnEmbedFromToken(
    const base::UnguessableToken& token,
    mojom::WindowDataPtr root,
    int64_t display_id,
    const base::Optional<viz::LocalSurfaceId>& local_surface_id) {}

void TestWindowTreeClient::OnEmbeddedAppDisconnected(Id window_id) {
  tracker_.OnEmbeddedAppDisconnected(window_id);
}

void TestWindowTreeClient::OnUnembed(Id window_id) {
  tracker_.OnUnembed(window_id);
}

void TestWindowTreeClient::OnCaptureChanged(Id new_capture_window_id,
                                            Id old_capture_window_id) {
  tracker_.OnCaptureChanged(new_capture_window_id, old_capture_window_id);
}

void TestWindowTreeClient::OnFrameSinkIdAllocated(
    Id window_id,
    const viz::FrameSinkId& frame_sink_id) {}

void TestWindowTreeClient::OnTopLevelCreated(
    uint32_t change_id,
    mojom::WindowDataPtr data,
    int64_t display_id,
    bool drawn,
    const base::Optional<viz::LocalSurfaceId>& local_surface_id) {
  tracker_.OnTopLevelCreated(change_id, std::move(data), drawn);
}

void TestWindowTreeClient::OnWindowBoundsChanged(
    Id window_id,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds,
    const base::Optional<viz::LocalSurfaceId>& local_surface_id) {
  // The bounds of the root may change during startup on Android at random
  // times. As this doesn't matter, and shouldn't impact test exepctations,
  // it is ignored.
  if (window_id == root_window_id_ && !track_root_bounds_changes_)
    return;
  tracker_.OnWindowBoundsChanged(window_id, old_bounds, new_bounds,
                                 local_surface_id);
}

void TestWindowTreeClient::OnWindowTransformChanged(
    Id window_id,
    const gfx::Transform& old_transform,
    const gfx::Transform& new_transform) {
  tracker_.OnWindowTransformChanged(window_id);
}

void TestWindowTreeClient::OnClientAreaChanged(
    Id window_id,
    const gfx::Insets& new_client_area,
    const std::vector<gfx::Rect>& new_additional_client_areas) {}

void TestWindowTreeClient::OnTransientWindowAdded(Id window_id,
                                                  Id transient_window_id) {
  tracker_.OnTransientWindowAdded(window_id, transient_window_id);
}

void TestWindowTreeClient::OnTransientWindowRemoved(Id window_id,
                                                    Id transient_window_id) {
  tracker_.OnTransientWindowRemoved(window_id, transient_window_id);
}

void TestWindowTreeClient::OnWindowHierarchyChanged(
    Id window,
    Id old_parent,
    Id new_parent,
    std::vector<mojom::WindowDataPtr> windows) {
  tracker_.OnWindowHierarchyChanged(window, old_parent, new_parent,
                                    std::move(windows));
}

void TestWindowTreeClient::OnWindowReordered(Id window_id,
                                             Id relative_window_id,
                                             mojom::OrderDirection direction) {
  tracker_.OnWindowReordered(window_id, relative_window_id, direction);
}

void TestWindowTreeClient::OnWindowDeleted(Id window) {
  tracker_.OnWindowDeleted(window);
}

void TestWindowTreeClient::OnWindowVisibilityChanged(Id window, bool visible) {
  tracker_.OnWindowVisibilityChanged(window, visible);
}

void TestWindowTreeClient::OnWindowOpacityChanged(Id window,
                                                  float old_opacity,
                                                  float new_opacity) {
  tracker_.OnWindowOpacityChanged(window, new_opacity);
}

void TestWindowTreeClient::OnWindowParentDrawnStateChanged(Id window,
                                                           bool drawn) {
  tracker_.OnWindowParentDrawnStateChanged(window, drawn);
}

void TestWindowTreeClient::OnWindowInputEvent(
    uint32_t event_id,
    Id window_id,
    int64_t display_id,
    Id display_root_window_id,
    const gfx::PointF& event_location_in_screen_pixel_layout,
    std::unique_ptr<ui::Event> event,
    bool matches_pointer_watcher) {
  tree_->OnWindowInputEventAck(event_id, mojom::EventResult::HANDLED);
}

void TestWindowTreeClient::OnPointerEventObserved(std::unique_ptr<ui::Event>,
                                                  Id window_id,
                                                  int64_t display_id) {}

void TestWindowTreeClient::OnWindowSharedPropertyChanged(
    Id window,
    const std::string& name,
    const base::Optional<std::vector<uint8_t>>& new_data) {
  tracker_.OnWindowSharedPropertyChanged(window, name, new_data);
}

void TestWindowTreeClient::OnWindowFocused(Id focused_window_id) {}

void TestWindowTreeClient::OnWindowCursorChanged(Id window_id,
                                                 ui::CursorData cursor) {
  tracker_.OnWindowCursorChanged(window_id, cursor);
}

void TestWindowTreeClient::OnDragDropStart(
    const base::flat_map<std::string, std::vector<uint8_t>>& drag_data) {}

void TestWindowTreeClient::OnWindowSurfaceChanged(
    Id window_id,
    const viz::SurfaceInfo& surface_info) {
  tracker_.OnWindowSurfaceChanged(window_id, surface_info);
}

void TestWindowTreeClient::OnDragEnter(Id window,
                                       uint32_t key_state,
                                       const gfx::Point& position,
                                       uint32_t effect_bitmask,
                                       OnDragEnterCallback callback) {}

void TestWindowTreeClient::OnDragOver(Id window,
                                      uint32_t key_state,
                                      const gfx::Point& position,
                                      uint32_t effect_bitmask,
                                      OnDragOverCallback callback) {}

void TestWindowTreeClient::OnDragLeave(Id window) {}

void TestWindowTreeClient::OnCompleteDrop(Id window,
                                          uint32_t key_state,
                                          const gfx::Point& position,
                                          uint32_t effect_bitmask,
                                          OnCompleteDropCallback callback) {}

void TestWindowTreeClient::OnPerformDragDropCompleted(uint32_t change_id,
                                                      bool success,
                                                      uint32_t action_taken) {}

void TestWindowTreeClient::OnDragDropDone() {}

void TestWindowTreeClient::OnChangeCompleted(uint32_t change_id, bool success) {
  tracker_.OnChangeCompleted(change_id, success);
}

void TestWindowTreeClient::RequestClose(Id window_id) {}

void TestWindowTreeClient::GetWindowManager(
    mojo::AssociatedInterfaceRequest<mojom::WindowManager> internal) {}

}  // namespace ws2
}  // namespace ui
