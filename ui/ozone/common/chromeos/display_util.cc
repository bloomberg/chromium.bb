// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/common/chromeos/display_util.h"

#include "ui/display/types/display_mode.h"
#include "ui/display/types/display_snapshot.h"

namespace ui {

DisplayMode_Params GetDisplayModeParams(const DisplayMode& mode) {
  DisplayMode_Params params;
  params.size = mode.size();
  params.is_interlaced = mode.is_interlaced();
  params.refresh_rate = mode.refresh_rate();

  return params;
}

DisplaySnapshot_Params GetDisplaySnapshotParams(
    const DisplaySnapshot& display) {
  DisplaySnapshot_Params params;
  params.display_id = display.display_id();
  params.has_proper_display_id = display.has_proper_display_id();
  params.origin = display.origin();
  params.physical_size = display.physical_size();
  params.type = display.type();
  params.is_aspect_preserving_scaling = display.is_aspect_preserving_scaling();
  params.has_overscan = display.has_overscan();
  params.display_name = display.display_name();
  for (size_t i = 0; i < display.modes().size(); ++i)
    params.modes.push_back(GetDisplayModeParams(*display.modes()[i]));

  params.has_current_mode = display.current_mode() != NULL;
  if (params.has_current_mode)
    params.current_mode = GetDisplayModeParams(*display.current_mode());

  params.has_native_mode = display.native_mode() != NULL;
  if (params.has_native_mode)
    params.native_mode = GetDisplayModeParams(*display.native_mode());

  params.string_representation = display.ToString();

  return params;
}

}  // namespace ui
