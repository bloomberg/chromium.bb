// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_DISPLAY_PLATFORM_SCREEN_OZONE_H_
#define SERVICES_UI_DISPLAY_PLATFORM_SCREEN_OZONE_H_

#include <stdint.h>

#include <set>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "services/ui/display/platform_screen.h"
#include "services/ui/public/interfaces/display/display_controller.mojom.h"
#include "ui/display/chromeos/display_configurator.h"
#include "ui/display/display.h"

namespace display {

// PlatformScreenOzone provides the necessary functionality to configure all
// attached physical displays on the ozone platform.
class PlatformScreenOzone : public PlatformScreen,
                            public ui::DisplayConfigurator::Observer,
                            public mojom::DisplayController {
 public:
  PlatformScreenOzone();
  ~PlatformScreenOzone() override;

  // PlatformScreen:
  void Init(PlatformScreenDelegate* delegate) override;
  int64_t GetPrimaryDisplayId() const override;

  // mojom::DisplayController:
  void ToggleVirtualDisplay(
      const ToggleVirtualDisplayCallback& callback) override;

 private:
  // TODO(kylechar): This struct is just temporary until we migrate
  // DisplayManager code out of ash so it can be used here.
  struct DisplayInfo {
    DisplayInfo(int64_t new_id, const gfx::Rect& new_bounds)
        : id(new_id), bounds(new_bounds) {}

    int64_t id;
    // The display bounds.
    gfx::Rect bounds;
    // The display bounds have been modified and delegate should be updated.
    bool modified = false;
    // The display has been removed and delegate should be updated.
    bool removed = false;
  };
  using CachedDisplayIterator = std::vector<DisplayInfo>::iterator;

  // Processes list of display snapshots and sets |removed| on any displays that
  // have been removed. Updates |primary_display_id_| if the primary display was
  // removed. Does not remove displays from |cached_displays_| or send updates
  // to delegate.
  void ProcessRemovedDisplays(
      const ui::DisplayConfigurator::DisplayStateList& snapshots);

  // Processes list of display snapshots and updates the bounds of any displays
  // in |cached_displays_| that have changed size. Does not send updates to
  // delegate.
  void ProcessModifiedDisplays(
      const ui::DisplayConfigurator::DisplayStateList& snapshots);

  // Looks at |cached_displays_| for modified or removed displays. Also updates
  // display bounds in response to modified or removed displays. Sends updates
  // to the delegate when appropriate by calling OnDisplayModified() or
  // OnDisplayRemoved(). Makes at most one call to delegate per display.
  //
  // Usually used after ProcessRemovedDisplays() and ProcessModifiedDisplays().
  void UpdateCachedDisplays();

  // Processes list of display snapshots and adds any new displays to
  // |cached_displays_|. Updates delegate by calling OnDisplayAdded().
  void AddNewDisplays(
      const ui::DisplayConfigurator::DisplayStateList& snapshots);

  // Returns an iterator to the cached display with |display_id| or an end
  // iterator if there is no display with that id.
  CachedDisplayIterator GetCachedDisplayIterator(int64_t display_id);

  // ui::DisplayConfigurator::Observer:
  void OnDisplayModeChanged(
      const ui::DisplayConfigurator::DisplayStateList& displays) override;
  void OnDisplayModeChangeFailed(
      const ui::DisplayConfigurator::DisplayStateList& displays,
      ui::MultipleDisplayState failed_new_state) override;

  PlatformScreenDelegate* delegate_ = nullptr;
  ui::DisplayConfigurator display_configurator_;

  // TODO(kylechar): These values can/should be replaced by DisplayLayout.
  int64_t primary_display_id_ = display::Display::kInvalidDisplayID;
  std::vector<DisplayInfo> cached_displays_;
  gfx::Point next_display_origin_;

  DISALLOW_COPY_AND_ASSIGN(PlatformScreenOzone);
};

}  // namespace display

#endif  // SERVICES_UI_DISPLAY_PLATFORM_SCREEN_OZONE_H_
