// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_tree_manager_map.h"

namespace ui {

AXTreeManagerMap::AXTreeManagerMap() {}

AXTreeManagerMap::~AXTreeManagerMap() {}

AXTreeManagerMap& AXTreeManagerMap::GetInstance() {
  static base::NoDestructor<AXTreeManagerMap> instance;
  return *instance;
}

void AXTreeManagerMap::AddTreeManager(AXTreeID tree_id,
                                      AXTreeManager* manager) {
  if (tree_id != AXTreeIDUnknown())
    map_[tree_id] = manager;
}

void AXTreeManagerMap::RemoveTreeManager(AXTreeID tree_id) {
  if (tree_id != AXTreeIDUnknown())
    map_.erase(tree_id);
}

AXTreeManager* AXTreeManagerMap::GetManager(AXTreeID tree_id) {
  if (tree_id == AXTreeIDUnknown())
    return nullptr;

  return map_[tree_id];
}

}  // namespace ui
