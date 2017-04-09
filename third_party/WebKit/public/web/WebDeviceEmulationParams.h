// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebDeviceEmulationParams_h
#define WebDeviceEmulationParams_h

#include "../platform/WebFloatPoint.h"
#include "../platform/WebPoint.h"
#include "../platform/WebRect.h"
#include "../platform/WebSize.h"
#include "../platform/modules/screen_orientation/WebScreenOrientationType.h"

namespace blink {

// All sizes are measured in device independent pixels.
struct WebDeviceEmulationParams {
  // For mobile, |screenSize| and |viewPosition| are used.
  // For desktop, screen size and view position are preserved.
  enum ScreenPosition { kDesktop, kMobile, kScreenPositionLast = kMobile };

  ScreenPosition screen_position;

  // Emulated screen size. Used with |screenPosition == Mobile|.
  WebSize screen_size;

  // Position of view on the screen. Used with |screenPosition == Mobile|.
  WebPoint view_position;

  // If zero, the original device scale factor is preserved.
  float device_scale_factor;

  // Emulated view size. Empty size means no override.
  WebSize view_size;

  // Whether emulated view should be scaled down if necessary to fit into
  // available space.
  bool fit_to_view;

  // Offset of emulated view inside available space, not in fit to view mode.
  WebFloatPoint offset;

  // Scale of emulated view inside available space, not in fit to view mode.
  float scale;

  // Optional screen orientation type, with WebScreenOrientationUndefined
  // value meaning no emulation necessary.
  WebScreenOrientationType screen_orientation_type;

  // Screen orientation angle, used together with screenOrientationType.
  int screen_orientation_angle;

  WebDeviceEmulationParams()
      : screen_position(kDesktop),
        device_scale_factor(0),
        fit_to_view(false),
        scale(1),
        screen_orientation_type(kWebScreenOrientationUndefined),
        screen_orientation_angle(0) {}
};

inline bool operator==(const WebDeviceEmulationParams& a,
                       const WebDeviceEmulationParams& b) {
  return a.screen_position == b.screen_position &&
         a.screen_size == b.screen_size && a.view_position == b.view_position &&
         a.device_scale_factor == b.device_scale_factor &&
         a.view_size == b.view_size && a.fit_to_view == b.fit_to_view &&
         a.offset == b.offset && a.scale == b.scale &&
         a.screen_orientation_type == b.screen_orientation_type &&
         a.screen_orientation_angle == b.screen_orientation_angle;
}

inline bool operator!=(const WebDeviceEmulationParams& a,
                       const WebDeviceEmulationParams& b) {
  return !(a == b);
}

}  // namespace blink

#endif
