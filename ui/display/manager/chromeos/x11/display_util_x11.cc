// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/manager/chromeos/x11/display_util_x11.h"

#include "base/macros.h"
#include "base/strings/string_util.h"

namespace ui {

namespace {

struct DisplayConnectionTypeMapping {
  // Prefix of output name.
  const char* name;
  DisplayConnectionType type;
};

const DisplayConnectionTypeMapping kDisplayConnectionTypeMapping[] = {
    {"LVDS", DISPLAY_CONNECTION_TYPE_INTERNAL},
    {"eDP", DISPLAY_CONNECTION_TYPE_INTERNAL},
    {"DSI", DISPLAY_CONNECTION_TYPE_INTERNAL},
    {"VGA", DISPLAY_CONNECTION_TYPE_VGA},
    {"HDMI", DISPLAY_CONNECTION_TYPE_HDMI},
    {"DVI", DISPLAY_CONNECTION_TYPE_DVI},
    {"DP", DISPLAY_CONNECTION_TYPE_DISPLAYPORT}};

}  // namespace

DisplayConnectionType GetDisplayConnectionTypeFromName(
    const std::string& name) {
  for (unsigned int i = 0; i < arraysize(kDisplayConnectionTypeMapping); ++i) {
    if (base::StartsWith(name, kDisplayConnectionTypeMapping[i].name,
                         base::CompareCase::SENSITIVE)) {
      return kDisplayConnectionTypeMapping[i].type;
    }
  }

  return DISPLAY_CONNECTION_TYPE_UNKNOWN;
}

}  // namespace ui
