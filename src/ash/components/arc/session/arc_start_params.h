// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_SESSION_ARC_START_PARAMS_H_
#define ASH_COMPONENTS_ARC_SESSION_ARC_START_PARAMS_H_

#include <stdint.h>

namespace arc {

// Parameters to start request.
struct StartParams {
  enum class PlayStoreAutoUpdate {
    // Play Store auto-update is left unchanged.
    AUTO_UPDATE_DEFAULT = 0,
    // Play Store auto-update is forced to on.
    AUTO_UPDATE_ON,
    // Play Store auto-update is forced to off.
    AUTO_UPDATE_OFF,
  };

  enum class DalvikMemoryProfile {
    // Default dalvik memory profile suitable for all devices.
    DEFAULT = 0,
    // Dalvik memory profile suitable for 4G devices.
    M4G,
    // Dalvik memory profile suitable for 8G devices.
    M8G,
    // Dalvik memory profile suitable for 16G devices.
    M16G,
  };

  enum class UsapProfile {
    // Default USAP profile suitable for all devices.
    DEFAULT = 0,
    // USAP profile suitable for 4G devices.
    M4G,
    // USAP profile suitable for 8G devices.
    M8G,
    // USAP profile suitable for 16G devices.
    M16G,
  };

  StartParams();

  StartParams(const StartParams&) = delete;
  StartParams& operator=(const StartParams&) = delete;

  StartParams(StartParams&& other);
  StartParams& operator=(StartParams&& other);

  ~StartParams();

  bool native_bridge_experiment = false;
  int lcd_density = -1;

  // Experiment flag for go/arc-file-picker.
  bool arc_file_picker_experiment = false;

  // Optional mode for play store auto-update.
  PlayStoreAutoUpdate play_store_auto_update =
      PlayStoreAutoUpdate::AUTO_UPDATE_DEFAULT;

  DalvikMemoryProfile dalvik_memory_profile = DalvikMemoryProfile::DEFAULT;

  UsapProfile usap_profile = UsapProfile::DEFAULT;

  // Experiment flag for ARC Custom Tabs.
  bool arc_custom_tabs_experiment = false;

  // Flag to disable scheduling of media store periodic maintenance tasks.
  bool disable_media_store_maintenance = false;

  // Flag to disable Download provider in cache based tests in order to prevent
  // installing content, that is impossible to control in ARC and which causes
  // flakiness in tests.
  bool disable_download_provider = false;

  // Flag to disable ureadahead completely, including host and guest parts.
  bool disable_ureadahead = false;

  // The number of logical CPU cores that are currently disabled on the host.
  uint32_t num_cores_disabled = 0;

  // Enables developer options used to generate Play Auto Install rosters.
  bool arc_generate_play_auto_install = false;

  // Flag to enable keyboard shortcut helper integration.
  bool enable_keyboard_shortcut_helper_integration = false;

  // Flag to enable notification refresh.
  bool enable_notifications_refresh = false;

  // Flag to enable TTS caching.
  bool enable_tts_caching = false;

  // Flag to enable disable consumer auto update toggle as part of EU new deal.
  bool enable_consumer_auto_update_toggle = false;
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_SESSION_ARC_START_PARAMS_H_
