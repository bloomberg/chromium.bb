// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/host/display_manager.h"

#include "base/logging.h"
#include "ui/display/types/display_snapshot.h"

namespace ui {

DisplayManager::DisplayManager() {
}

DisplayManager::~DisplayManager() {
}

void DisplayManager::RegisterDisplay(DisplaySnapshot* display) {
  std::pair<DisplayMap::iterator, bool> result = display_map_.insert(
      std::pair<int64_t, DisplaySnapshot*>(display->display_id(), display));
  DCHECK(result.second) << "Display " << display->display_id()
                        << " already added.";
}

void DisplayManager::UnregisterDisplay(DisplaySnapshot* display) {
  DisplayMap::iterator it = display_map_.find(display->display_id());
  if (it != display_map_.end())
    display_map_.erase(it);
  else
    NOTREACHED() << "Attempting to remove non-existing display "
                 << display->display_id();
}

DisplaySnapshot* DisplayManager::GetDisplay(int64_t display) {
  DisplayMap::iterator it = display_map_.find(display);
  if (it == display_map_.end())
    return nullptr;

  return it->second;
}

}  // namespace ui
