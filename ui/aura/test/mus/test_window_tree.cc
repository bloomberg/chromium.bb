// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/test/mus/test_window_tree.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace aura {

TestWindowTree::TestWindowTree() {}

TestWindowTree::~TestWindowTree() {}

bool TestWindowTree::WasEventAcked(uint32_t event_id) const {
  for (const AckedEvent& acked_event : acked_events_) {
    if (acked_event.event_id == event_id)
      return true;
  }
  return false;
}

ui::mojom::EventResult TestWindowTree::GetEventResult(uint32_t event_id) const {
  for (const AckedEvent& acked_event : acked_events_) {
    if (acked_event.event_id == event_id)
      return acked_event.result;
  }
  return ui::mojom::EventResult::UNHANDLED;
}

base::Optional<std::vector<uint8_t>> TestWindowTree::GetLastPropertyValue() {
  return std::move(last_property_value_);
}

base::Optional<std::unordered_map<std::string, std::vector<uint8_t>>>
TestWindowTree::GetLastNewWindowProperties() {
  return std::move(last_new_window_properties_);
}

void TestWindowTree::AckAllChanges() {
  while (!changes_.empty()) {
    client_->OnChangeCompleted(changes_[0].id, true);
    changes_.erase(changes_.begin());
  }
}

bool TestWindowTree::AckSingleChangeOfType(WindowTreeChangeType type,
                                           bool result) {
  auto match = changes_.end();
  for (auto iter = changes_.begin(); iter != changes_.end(); ++iter) {
    if (iter->type == type) {
      if (match == changes_.end())
        match = iter;
      else
        return false;
    }
  }
  if (match == changes_.end())
    return false;
  const uint32_t change_id = match->id;
  changes_.erase(match);
  client_->OnChangeCompleted(change_id, result);
  return true;
}

bool TestWindowTree::AckFirstChangeOfType(WindowTreeChangeType type,
                                          bool result) {
  uint32_t change_id;
  if (!GetAndRemoveFirstChangeOfType(type, &change_id))
    return false;
  client_->OnChangeCompleted(change_id, result);
  return true;
}

void TestWindowTree::AckAllChangesOfType(WindowTreeChangeType type,
                                         bool result) {
  for (size_t i = 0; i < changes_.size();) {
    if (changes_[i].type != type) {
      ++i;
      continue;
    }
    const uint32_t change_id = changes_[i].id;
    changes_.erase(changes_.begin() + i);
    client_->OnChangeCompleted(change_id, result);
  }
}

bool TestWindowTree::GetAndRemoveFirstChangeOfType(WindowTreeChangeType type,
                                                   uint32_t* change_id) {
  for (auto iter = changes_.begin(); iter != changes_.end(); ++iter) {
    if (iter->type != type)
      continue;
    *change_id = iter->id;
    changes_.erase(iter);
    return true;
  }
  return false;
}

size_t TestWindowTree::GetChangeCountForType(WindowTreeChangeType type) {
  size_t count = 0;
  for (const auto& change : changes_) {
    if (change.type == type)
      ++count;
  }
  return count;
}

void TestWindowTree::OnChangeReceived(uint32_t change_id,
                                      WindowTreeChangeType type) {
  changes_.push_back({type, change_id});
}

void TestWindowTree::NewWindow(
    uint32_t change_id,
    uint32_t window_id,
    const base::Optional<std::unordered_map<std::string, std::vector<uint8_t>>>&
        properties) {
  last_new_window_properties_ = properties;
  OnChangeReceived(change_id, WindowTreeChangeType::NEW_WINDOW);
}

void TestWindowTree::NewTopLevelWindow(
    uint32_t change_id,
    uint32_t window_id,
    const std::unordered_map<std::string, std::vector<uint8_t>>& properties) {
  last_new_window_properties_.emplace(properties);
  window_id_ = window_id;
  OnChangeReceived(change_id, WindowTreeChangeType::NEW_TOP_LEVEL);
}

void TestWindowTree::DeleteWindow(uint32_t change_id, uint32_t window_id) {
  OnChangeReceived(change_id);
}

void TestWindowTree::SetWindowBounds(
    uint32_t change_id,
    uint32_t window_id,
    const gfx::Rect& bounds,
    const base::Optional<viz::LocalSurfaceId>& local_surface_id) {
  window_id_ = window_id;
  last_local_surface_id_ = local_surface_id;
  last_set_window_bounds_ = bounds;
  OnChangeReceived(change_id, WindowTreeChangeType::BOUNDS);
}

void TestWindowTree::SetWindowTransform(uint32_t change_id,
                                        uint32_t window_id,
                                        const gfx::Transform& transform) {
  OnChangeReceived(change_id, WindowTreeChangeType::TRANSFORM);
}

void TestWindowTree::SetClientArea(
    uint32_t window_id,
    const gfx::Insets& insets,
    const base::Optional<std::vector<gfx::Rect>>& additional_client_areas) {
  last_client_area_ = insets;
}

void TestWindowTree::SetHitTestMask(uint32_t window_id,
                                    const base::Optional<gfx::Rect>& mask) {
  last_hit_test_mask_ = mask;
}

void TestWindowTree::SetCanAcceptDrops(uint32_t window_id, bool accepts_drops) {
}

void TestWindowTree::SetWindowVisibility(uint32_t change_id,
                                         uint32_t window_id,
                                         bool visible) {
  OnChangeReceived(change_id, WindowTreeChangeType::VISIBLE);
}

void TestWindowTree::SetWindowProperty(
    uint32_t change_id,
    uint32_t window_id,
    const std::string& name,
    const base::Optional<std::vector<uint8_t>>& value) {
  last_property_value_ = value;
  OnChangeReceived(change_id, WindowTreeChangeType::PROPERTY);
}

void TestWindowTree::SetWindowOpacity(uint32_t change_id,
                                      uint32_t window_id,
                                      float opacity) {
  OnChangeReceived(change_id);
}

void TestWindowTree::AttachCompositorFrameSink(
    uint32_t window_id,
    mojo::InterfaceRequest<viz::mojom::CompositorFrameSink> surface,
    viz::mojom::CompositorFrameSinkClientPtr client) {}

void TestWindowTree::AddWindow(uint32_t change_id,
                               uint32_t parent,
                               uint32_t child) {
  OnChangeReceived(change_id);
}

void TestWindowTree::RemoveWindowFromParent(uint32_t change_id,
                                            uint32_t window_id) {
  OnChangeReceived(change_id);
}

void TestWindowTree::AddTransientWindow(uint32_t change_id,
                                        uint32_t window_id,
                                        uint32_t transient_window_id) {
  transient_data_.parent_id = window_id;
  transient_data_.child_id = transient_window_id;
  OnChangeReceived(change_id, WindowTreeChangeType::ADD_TRANSIENT);
}

void TestWindowTree::RemoveTransientWindowFromParent(
    uint32_t change_id,
    uint32_t transient_window_id) {
  transient_data_.parent_id = kInvalidServerId;
  transient_data_.child_id = transient_window_id;
  OnChangeReceived(change_id, WindowTreeChangeType::REMOVE_TRANSIENT);
}

void TestWindowTree::SetModalType(uint32_t change_id,
                                  uint32_t window_id,
                                  ui::ModalType modal_type) {
  OnChangeReceived(change_id, WindowTreeChangeType::MODAL);
}

void TestWindowTree::SetChildModalParent(uint32_t change_id,
                                         Id window_id,
                                         Id parent_window_id) {}

void TestWindowTree::ReorderWindow(uint32_t change_id,
                                   uint32_t window_id,
                                   uint32_t relative_window_id,
                                   ui::mojom::OrderDirection direction) {
  OnChangeReceived(change_id, WindowTreeChangeType::REORDER);
}

void TestWindowTree::GetWindowTree(uint32_t window_id,
                                   const GetWindowTreeCallback& callback) {}

void TestWindowTree::SetCapture(uint32_t change_id, uint32_t window_id) {
  OnChangeReceived(change_id, WindowTreeChangeType::CAPTURE);
}

void TestWindowTree::ReleaseCapture(uint32_t change_id, uint32_t window_id) {
  OnChangeReceived(change_id, WindowTreeChangeType::CAPTURE);
}

void TestWindowTree::StartPointerWatcher(bool want_moves) {}

void TestWindowTree::StopPointerWatcher() {}

void TestWindowTree::Embed(uint32_t window_id,
                           ui::mojom::WindowTreeClientPtr client,
                           uint32_t flags,
                           const EmbedCallback& callback) {}

void TestWindowTree::SetFocus(uint32_t change_id, uint32_t window_id) {
  OnChangeReceived(change_id, WindowTreeChangeType::FOCUS);
}

void TestWindowTree::SetCanFocus(uint32_t window_id, bool can_focus) {}

void TestWindowTree::SetEventTargetingPolicy(
    uint32_t window_id,
    ui::mojom::EventTargetingPolicy policy) {}

void TestWindowTree::SetCursor(uint32_t change_id,
                               Id transport_window_id,
                               ui::CursorData cursor_data) {
  OnChangeReceived(change_id);
}

void TestWindowTree::SetWindowTextInputState(uint32_t window_id,
                                             mojo::TextInputStatePtr state) {}

void TestWindowTree::SetImeVisibility(uint32_t window_id,
                                      bool visible,
                                      mojo::TextInputStatePtr state) {}

void TestWindowTree::OnWindowInputEventAck(uint32_t event_id,
                                           ui::mojom::EventResult result) {
  EXPECT_FALSE(WasEventAcked(event_id));
  acked_events_.push_back({event_id, result});
}

void TestWindowTree::DeactivateWindow(uint32_t window_id) {}

void TestWindowTree::StackAbove(uint32_t change_id, uint32_t above_id,
                                uint32_t below_id) {}

void TestWindowTree::StackAtTop(uint32_t change_id, uint32_t window_id) {}

void TestWindowTree::GetWindowManagerClient(
    mojo::AssociatedInterfaceRequest<ui::mojom::WindowManagerClient> internal) {
}

void TestWindowTree::GetCursorLocationMemory(
    const GetCursorLocationMemoryCallback& callback) {
  callback.Run(mojo::ScopedSharedBufferHandle());
}

void TestWindowTree::PerformDragDrop(
    uint32_t change_id,
    uint32_t source_window_id,
    const gfx::Point& screen_location,
    const std::unordered_map<std::string, std::vector<uint8_t>>& drag_data,
    const SkBitmap& drag_image,
    const gfx::Vector2d& drag_image_offset,
    uint32_t drag_operation,
    ui::mojom::PointerKind source) {
  OnChangeReceived(change_id);
}

void TestWindowTree::CancelDragDrop(uint32_t window_id) {}

void TestWindowTree::PerformWindowMove(uint32_t change_id,
                                       uint32_t window_id,
                                       ui::mojom::MoveLoopSource source,
                                       const gfx::Point& cursor_location) {
  OnChangeReceived(change_id);
}

void TestWindowTree::CancelWindowMove(uint32_t window_id) {}

}  // namespace aura
