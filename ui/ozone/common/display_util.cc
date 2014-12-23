// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/common/display_util.h"

#include "base/command_line.h"
#include "ui/display/types/display_mode.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/ozone/public/ozone_switches.h"

namespace ui {

namespace {

const int64_t kDummyDisplayId = 1;

}  // namespace

FindDisplayById::FindDisplayById(int64_t display_id) : display_id_(display_id) {
}

bool FindDisplayById::operator()(const DisplaySnapshot_Params& display) const {
  return display.display_id == display_id_;
}

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

DisplaySnapshot_Params CreateSnapshotFromCommandLine() {
  DisplaySnapshot_Params display_param;

  base::CommandLine* cmd = base::CommandLine::ForCurrentProcess();
  std::string spec =
      cmd->GetSwitchValueASCII(switches::kOzoneInitialDisplayBounds);
  std::string physical_spec =
      cmd->GetSwitchValueASCII(switches::kOzoneInitialDisplayPhysicalSizeMm);

  if (spec.empty())
    return display_param;

  int width = 0;
  int height = 0;
  if (sscanf(spec.c_str(), "%dx%d", &width, &height) < 2)
    return display_param;

  if (width == 0 || height == 0)
    return display_param;

  int physical_width = 0;
  int physical_height = 0;
  sscanf(physical_spec.c_str(), "%dx%d", &physical_width, &physical_height);

  DisplayMode_Params mode_param;
  mode_param.size = gfx::Size(width, height);
  mode_param.refresh_rate = 60;

  display_param.display_id = kDummyDisplayId;
  display_param.modes.push_back(mode_param);
  display_param.type = DISPLAY_CONNECTION_TYPE_INTERNAL;
  display_param.physical_size = gfx::Size(physical_width, physical_height);
  display_param.has_current_mode = true;
  display_param.current_mode = mode_param;
  display_param.has_native_mode = true;
  display_param.native_mode = mode_param;

  return display_param;
}

}  // namespace ui
