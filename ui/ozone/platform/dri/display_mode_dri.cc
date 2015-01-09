// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/dri/display_mode_dri.h"

namespace ui {

namespace {

float GetRefreshRate(const drmModeModeInfo& mode) {
  if (!mode.htotal || !mode.vtotal)
    return mode.vrefresh;

  float clock = mode.clock;
  float htotal = mode.htotal;
  float vtotal = mode.vtotal;

  return (clock * 1000.0f) / (htotal * vtotal);
}

}  // namespace

DisplayModeDri::DisplayModeDri(const drmModeModeInfo& mode)
    : DisplayMode(gfx::Size(mode.hdisplay, mode.vdisplay),
                  mode.flags & DRM_MODE_FLAG_INTERLACE,
                  GetRefreshRate(mode)),
      mode_info_(mode) {
}

DisplayModeDri::~DisplayModeDri() {
}

}  // namespace ui
