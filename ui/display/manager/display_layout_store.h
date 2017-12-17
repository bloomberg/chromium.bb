// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_MANAGER_DISPLAY_LAYOUT_STORE_H_
#define UI_DISPLAY_MANAGER_DISPLAY_LAYOUT_STORE_H_

#include <stdint.h>

#include <map>
#include <memory>

#include "base/macros.h"
#include "ui/display/display.h"
#include "ui/display/display_layout.h"
#include "ui/display/manager/display_manager_export.h"

namespace display {

class DISPLAY_MANAGER_EXPORT DisplayLayoutStore {
 public:
  DisplayLayoutStore();
  ~DisplayLayoutStore();

  // Set true to force mirror mode.
  void set_forced_mirror_mode(bool forced) { forced_mirror_mode_ = forced; }

  void SetDefaultDisplayPlacement(const DisplayPlacement& placement);

  // Registers the display layout info for the specified display(s).
  void RegisterLayoutForDisplayIdList(const DisplayIdList& list,
                                      std::unique_ptr<DisplayLayout> layout);

  // Returns true if it should enter mirror mode for given display |list|.
  bool GetMirrorMode(const DisplayIdList& list);

  // If no layout is registered, it creatas new layout using
  // |default_display_layout_|.
  const DisplayLayout& GetRegisteredDisplayLayout(const DisplayIdList& list);

  // Update the multi display state in the display layout for
  // |display_list|.  This creates new display layout if no layout is
  // registered for |display_list|.
  void UpdateMultiDisplayState(const DisplayIdList& display_list,
                               bool mirrored,
                               bool default_unified);

 private:
  // Creates new layout for display list from |default_display_layout_|.
  DisplayLayout* CreateDefaultDisplayLayout(const DisplayIdList& display_list);

  // The default display placement.
  DisplayPlacement default_display_placement_;

  bool forced_mirror_mode_ = false;

  // Display layout per list of devices.
  std::map<DisplayIdList, std::unique_ptr<DisplayLayout>> layouts_;

  DISALLOW_COPY_AND_ASSIGN(DisplayLayoutStore);
};

}  // namespace display

#endif  // UI_DISPLAY_MANAGER_DISPLAY_LAYOUT_STORE_H_
