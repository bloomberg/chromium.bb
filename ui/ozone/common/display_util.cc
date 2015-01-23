// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/common/display_util.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "ui/display/types/display_mode.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/display/util/edid_parser.h"
#include "ui/display/util/edid_parser.h"
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

bool CreateSnapshotFromCommandLine(DisplaySnapshot_Params* snapshot_out) {
  base::CommandLine* cmd = base::CommandLine::ForCurrentProcess();
  std::string spec =
      cmd->GetSwitchValueASCII(switches::kOzoneInitialDisplayBounds);
  std::string physical_spec =
      cmd->GetSwitchValueASCII(switches::kOzoneInitialDisplayPhysicalSizeMm);

  int width = 0;
  int height = 0;
  if (spec.empty() || sscanf(spec.c_str(), "%dx%d", &width, &height) < 2 ||
      width == 0 || height == 0) {
    return false;
  }

  int physical_width = 0;
  int physical_height = 0;
  sscanf(physical_spec.c_str(), "%dx%d", &physical_width, &physical_height);

  DisplayMode_Params mode_param;
  mode_param.size = gfx::Size(width, height);
  mode_param.refresh_rate = 60;

  snapshot_out->display_id = kDummyDisplayId;
  snapshot_out->modes.push_back(mode_param);
  snapshot_out->type = DISPLAY_CONNECTION_TYPE_INTERNAL;
  snapshot_out->physical_size = gfx::Size(physical_width, physical_height);
  snapshot_out->has_current_mode = true;
  snapshot_out->current_mode = mode_param;
  snapshot_out->has_native_mode = true;
  snapshot_out->native_mode = mode_param;
  return true;
}

bool CreateSnapshotFromEDID(bool internal,
                            const std::vector<uint8_t>& edid,
                            DisplaySnapshot_Params* snapshot_out) {
  uint16_t manufacturer_id = 0;
  gfx::Size resolution;

  DisplayMode_Params mode_param;
  mode_param.refresh_rate = 60.0f;

  if (!ParseOutputDeviceData(edid, &manufacturer_id,
                             &snapshot_out->display_name, &mode_param.size,
                             &snapshot_out->physical_size) ||
      !GetDisplayIdFromEDID(edid, 0, &snapshot_out->display_id)) {
    return false;
  }
  ParseOutputOverscanFlag(edid, &snapshot_out->has_overscan);

  snapshot_out->modes.push_back(mode_param);
  // Use VGA for external display for now.
  // TODO(oshima): frecon should set this value in the display_info.bin file.
  snapshot_out->type =
      internal ? DISPLAY_CONNECTION_TYPE_INTERNAL : DISPLAY_CONNECTION_TYPE_VGA;
  snapshot_out->has_current_mode = true;
  snapshot_out->current_mode = mode_param;
  snapshot_out->has_native_mode = true;
  snapshot_out->native_mode = mode_param;
  return true;
}

bool CreateSnapshotFromEDIDFile(const base::FilePath& file,
                                DisplaySnapshot_Params* snapshot_out) {
  std::string raw_display_info;
  const int kEDIDMaxSize = 128;
  if (!base::ReadFileToString(file, &raw_display_info, kEDIDMaxSize + 1) ||
      raw_display_info.size() < 10) {
    return false;
  }
  std::vector<uint8_t> edid;
  // The head of the file contains one byte flag that indicates the type of
  // display.
  bool internal = raw_display_info[0] == 1;
  edid.assign(raw_display_info.c_str() + 1,
              raw_display_info.c_str() + raw_display_info.size());
  return CreateSnapshotFromEDID(internal, edid, snapshot_out);
}

}  // namespace ui
