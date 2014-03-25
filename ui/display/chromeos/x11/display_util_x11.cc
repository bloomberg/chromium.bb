// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/chromeos/x11/display_util_x11.h"

#include "base/macros.h"

namespace ui {

namespace {

struct OutputTypeMapping {
  // Prefix of output name.
  const char* name;
  OutputType type;
};

const OutputTypeMapping kOutputTypeMapping[] = {
  {"LVDS", OUTPUT_TYPE_INTERNAL},
  {"eDP", OUTPUT_TYPE_INTERNAL},
  {"DSI", OUTPUT_TYPE_INTERNAL},
  {"VGA", OUTPUT_TYPE_VGA},
  {"HDMI", OUTPUT_TYPE_HDMI},
  {"DVI", OUTPUT_TYPE_DVI},
  {"DP", OUTPUT_TYPE_DISPLAYPORT}
};

}  // namespace

OutputType GetOutputTypeFromName(const std::string& name) {
  for (unsigned int i = 0; i < arraysize(kOutputTypeMapping); ++i) {
    if (name.find(kOutputTypeMapping[i].name) == 0) {
      return kOutputTypeMapping[i].type;
    }
  }

  return OUTPUT_TYPE_UNKNOWN;
}

}  // namespace ui
