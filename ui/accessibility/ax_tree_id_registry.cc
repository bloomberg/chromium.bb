// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_tree_id_registry.h"

#include "base/memory/singleton.h"
#include "base/strings/string_number_conversions.h"
#include "ui/accessibility/ax_host_delegate.h"

namespace ui {

// static
AXTreeIDRegistry* AXTreeIDRegistry::GetInstance() {
  return base::Singleton<AXTreeIDRegistry>::get();
}

void AXTreeIDRegistry::SetFrameIDForAXTreeID(const FrameID& frame_id,
                                             const AXTreeID& ax_tree_id) {
  auto it = frame_to_ax_tree_id_map_.find(frame_id);
  if (it != frame_to_ax_tree_id_map_.end()) {
    NOTREACHED();
    return;
  }

  frame_to_ax_tree_id_map_[frame_id] = ax_tree_id;
  ax_tree_to_frame_id_map_[ax_tree_id] = frame_id;
}

AXTreeIDRegistry::FrameID AXTreeIDRegistry::GetFrameID(
    const AXTreeID& ax_tree_id) {
  auto it = ax_tree_to_frame_id_map_.find(ax_tree_id);
  if (it != ax_tree_to_frame_id_map_.end())
    return it->second;

  return FrameID(-1, -1);
}

AXTreeID AXTreeIDRegistry::GetAXTreeID(AXTreeIDRegistry::FrameID frame_id) {
  auto it = frame_to_ax_tree_id_map_.find(frame_id);
  if (it != frame_to_ax_tree_id_map_.end())
    return it->second;

  return ui::AXTreeIDUnknown();
}

AXTreeID AXTreeIDRegistry::GetOrCreateAXTreeID(AXHostDelegate* delegate) {
  for (auto it : id_to_host_delegate_) {
    if (it.second == delegate)
      return it.first;
  }
  AXTreeID new_id = AXTreeID::CreateNewAXTreeID();
  id_to_host_delegate_[new_id] = delegate;
  return new_id;
}

AXHostDelegate* AXTreeIDRegistry::GetHostDelegate(AXTreeID ax_tree_id) {
  auto it = id_to_host_delegate_.find(ax_tree_id);
  if (it == id_to_host_delegate_.end())
    return nullptr;
  return it->second;
}

void AXTreeIDRegistry::RemoveAXTreeID(AXTreeID ax_tree_id) {
  auto frame_it = ax_tree_to_frame_id_map_.find(ax_tree_id);
  if (frame_it != ax_tree_to_frame_id_map_.end()) {
    frame_to_ax_tree_id_map_.erase(frame_it->second);
    ax_tree_to_frame_id_map_.erase(frame_it);
  }

  auto action_it = id_to_host_delegate_.find(ax_tree_id);
  if (action_it != id_to_host_delegate_.end())
    id_to_host_delegate_.erase(action_it);
}

AXTreeIDRegistry::AXTreeIDRegistry() {
}

AXTreeIDRegistry::~AXTreeIDRegistry() {}

}  // namespace ui
