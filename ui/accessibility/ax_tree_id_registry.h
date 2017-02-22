// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_TREE_ID_REGISTRY_H_
#define UI_ACCESSIBILITY_AX_TREE_ID_REGISTRY_H_

#include <map>
#include <utility>

#include "base/macros.h"
#include "ui/accessibility/ax_export.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

namespace ui {

// A class which generates a unique id.
class AX_EXPORT AXTreeIDRegistry {
 public:
  using FrameID = std::pair<int, int>;

  using AXTreeID = int;

  static const AXTreeID kNoAXTreeID;

  // Get the single instance of this class.
  static AXTreeIDRegistry* GetInstance();

  // Obtains a unique id given a |process_id| and |routing_id|. Placeholder
  // for full implementation once out of process iframe accessibility finalizes.
  AXTreeID GetOrCreateAXTreeID(int process_id, int routing_id);
  FrameID GetFrameID(AXTreeID ax_tree_id);
  void RemoveAXTreeID(AXTreeID ax_tree_id);

  // Create an id not associated with any process.
  int CreateID();

 private:
  friend struct base::DefaultSingletonTraits<AXTreeIDRegistry>;

  AXTreeIDRegistry();
  virtual ~AXTreeIDRegistry();

  // Tracks the current unique ax frame id.
  AXTreeID ax_tree_id_counter_;

  // Maps an accessibility tree to its frame via ids.
  std::map<AXTreeID, FrameID> ax_tree_to_frame_id_map_;

  // Maps frames to an accessibility tree via ids.
  std::map<FrameID, AXTreeID> frame_to_ax_tree_id_map_;

  DISALLOW_COPY_AND_ASSIGN(AXTreeIDRegistry);
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_TREE_ID_REGISTRY_H_
