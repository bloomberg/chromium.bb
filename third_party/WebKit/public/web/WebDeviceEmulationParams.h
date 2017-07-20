// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebDeviceEmulationParams_h
#define WebDeviceEmulationParams_h

#include "public/platform/WebFloatPoint.h"
#include "public/platform/WebPoint.h"
#include "public/platform/WebRect.h"
#include "public/platform/WebSize.h"
#include "public/platform/modules/screen_orientation/WebScreenOrientationType.h"

namespace blink {

// All sizes are measured in device independent pixels.
struct WebDeviceEmulationParams {
  // For mobile, |screen_size| and |view_position| are used.
  // For desktop, |screen_size| and |view_position| are preserved.
  enum ScreenPosition { kDesktop, kMobile, kScreenPositionLast = kMobile };

  ScreenPosition screen_position;

  // Emulated screen size. Typically full / physical size of the device screen
  // in DIP. Used with |screen_position == Mobile|.
  WebSize screen_size;

  // Position of view on the screen. Used with |screen_position == Mobile|.
  WebPoint view_position;

  // Emulated view size. Empty size means no override.
  WebSize view_size;

  // If zero, the original device scale factor is preserved.
  float device_scale_factor;

  // Scale of emulated view inside available space, not in fit to view mode.
  float scale;

  // Forced viewport offset for screenshots during emulation, (-1, -1) for
  // disabled.
  WebFloatPoint viewport_offset;

  // Viewport scale for screenshots during emulation, 0 for current.
  float viewport_scale;

  // Optional screen orientation type, with WebScreenOrientationUndefined
  // value meaning no emulation necessary.
  WebScreenOrientationType screen_orientation_type;

  // Screen orientation angle, used together with screenOrientationType.
  int screen_orientation_angle;

  WebDeviceEmulationParams()
      : screen_position(kDesktop),
        device_scale_factor(0),
        scale(1),
        viewport_offset(-1, -1),
        viewport_scale(0),
        screen_orientation_type(kWebScreenOrientationUndefined),
        screen_orientation_angle(0) {}
};

inline bool operator==(const WebDeviceEmulationParams& a,
                       const WebDeviceEmulationParams& b) {
  return a.screen_position == b.screen_position &&
         a.screen_size == b.screen_size && a.view_position == b.view_position &&
         a.device_scale_factor == b.device_scale_factor &&
         a.view_size == b.view_size && a.scale == b.scale &&
         a.screen_orientation_type == b.screen_orientation_type &&
         a.screen_orientation_angle == b.screen_orientation_angle &&
         a.viewport_offset == b.viewport_offset &&
         a.viewport_scale == b.viewport_scale;
}

inline bool operator!=(const WebDeviceEmulationParams& a,
                       const WebDeviceEmulationParams& b) {
  return !(a == b);
}

}  // namespace blink

#endif
