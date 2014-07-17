// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/chromeos/touchscreen_delegate_impl.h"

#include <cmath>
#include <set>

#include "ui/display/types/chromeos/display_mode.h"
#include "ui/display/types/chromeos/display_snapshot.h"
#include "ui/display/types/chromeos/touchscreen_device_manager.h"

namespace ui {

TouchscreenDelegateImpl::TouchscreenDelegateImpl(
    scoped_ptr<TouchscreenDeviceManager> touch_device_manager)
    : touch_device_manager_(touch_device_manager.Pass()) {}

TouchscreenDelegateImpl::~TouchscreenDelegateImpl() {}

void TouchscreenDelegateImpl::AssociateTouchscreens(
    std::vector<DisplayConfigurator::DisplayState>* displays) {
  std::set<int> no_match_touchscreen;
  std::vector<TouchscreenDevice> devices = touch_device_manager_->GetDevices();

  int internal_touchscreen = -1;
  for (size_t i = 0; i < devices.size(); ++i) {
    if (devices[i].is_internal) {
      internal_touchscreen = i;
      break;
    }
  }

  DisplayConfigurator::DisplayState* internal_state = NULL;
  for (size_t i = 0; i < displays->size(); ++i) {
    DisplayConfigurator::DisplayState* state = &(*displays)[i];
    if (state->display->type() == DISPLAY_CONNECTION_TYPE_INTERNAL &&
        state->display->native_mode() &&
        state->touch_device_id == 0) {
      internal_state = state;
      break;
    }
  }

  if (internal_state && internal_touchscreen >= 0) {
    internal_state->touch_device_id = devices[internal_touchscreen].id;
    VLOG(2) << "Found internal touchscreen for internal display "
            << internal_state->display->display_id() << " touch_device_id "
            << internal_state->touch_device_id << " size "
            << devices[internal_touchscreen].size.ToString();
  }

  for (size_t i = 0; i < devices.size(); ++i) {
    if (internal_state &&
        internal_state->touch_device_id == devices[i].id)
      continue;
    bool found_mapping = false;
    for (size_t j = 0; j < displays->size(); ++j) {
      DisplayConfigurator::DisplayState* state = &(*displays)[j];
      const DisplayMode* mode = state->display->native_mode();
      if (state->touch_device_id != 0 || !mode)
        continue;

      // Allow 1 pixel difference between screen and touchscreen
      // resolutions. Because in some cases for monitor resolution
      // 1024x768 touchscreen's resolution would be 1024x768, but for
      // some 1023x767. It really depends on touchscreen's firmware
      // configuration.
      if (std::abs(mode->size().width() - devices[i].size.width()) <= 1 &&
          std::abs(mode->size().height() - devices[i].size.height()) <= 1) {
        state->touch_device_id = devices[i].id;

        VLOG(2) << "Found touchscreen for display "
                << state->display->display_id() << " touch_device_id "
                << state->touch_device_id << " size "
                << devices[i].size.ToString();
        found_mapping = true;
        break;
      }
    }

    if (!found_mapping) {
      no_match_touchscreen.insert(devices[i].id);
      VLOG(2) << "No matching display for touch_device_id "
              << devices[i].id << " size " << devices[i].size.ToString();
    }
  }

  // Sometimes we can't find a matching screen for the touchscreen, e.g.
  // due to the touchscreen's reporting range having no correlation with the
  // screen's resolution. In this case, we arbitrarily assign unmatched
  // touchscreens to unmatched screens.
  for (std::set<int>::iterator it = no_match_touchscreen.begin();
       it != no_match_touchscreen.end();
       ++it) {
    for (size_t i = 0; i < displays->size(); ++i) {
      DisplayConfigurator::DisplayState* state = &(*displays)[i];
      if (state->display->type() != DISPLAY_CONNECTION_TYPE_INTERNAL &&
          state->display->native_mode() != NULL &&
          state->touch_device_id == 0) {
        state->touch_device_id = *it;
        VLOG(2) << "Arbitrarily matching touchscreen "
                << state->touch_device_id << " to display "
                << state->display->display_id();
        break;
      }
    }
  }
}

}  // namespace ui
