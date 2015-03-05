// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_HOST_DISPLAY_MANAGER_H_
#define UI_OZONE_PLATFORM_DRM_HOST_DISPLAY_MANAGER_H_

#include <stdint.h>

#include <map>

#include "base/macros.h"

namespace ui {

class DisplaySnapshot;

class DisplayManager {
 public:
  DisplayManager();
  ~DisplayManager();

  void RegisterDisplay(DisplaySnapshot* display);
  void UnregisterDisplay(DisplaySnapshot* display);

  // Returns the display state for |display| or NULL if not found.
  DisplaySnapshot* GetDisplay(int64_t display);

 private:
  typedef std::map<int64_t, DisplaySnapshot*> DisplayMap;

  // Keeps a mapping between the display ID and a pointer to the display state.
  DisplayMap display_map_;

  DISALLOW_COPY_AND_ASSIGN(DisplayManager);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_HOST_DISPLAY_MANAGER_H_
