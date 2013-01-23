// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/capturer/mac/desktop_configuration.h"

#include <Cocoa/Cocoa.h>

#include "base/logging.h"
#include "skia/ext/skia_utils_mac.h"

namespace remoting {

MacDisplayConfiguration::MacDisplayConfiguration()
  : id(0),
    logical_bounds(SkIRect::MakeEmpty()),
    pixel_bounds(SkIRect::MakeEmpty()),
    dpi(SkIPoint::Make(0, 0)),
    logical_to_pixel_scale(1.0f) {
}

static SkIRect NSRectToSkIRect(const NSRect& ns_rect) {
  SkIRect result;
  gfx::CGRectToSkRect(NSRectToCGRect(ns_rect)).roundOut(&result);
  return result;
}

static MacDisplayConfiguration GetConfigurationForScreen(NSScreen* screen) {
  MacDisplayConfiguration display_config;

  // Fetch the NSScreenNumber, which is also the CGDirectDisplayID.
  NSDictionary* device_description = [screen deviceDescription];
  display_config.id = static_cast<CGDirectDisplayID>(
      [[device_description objectForKey:@"NSScreenNumber"] intValue]);

  // Determine the display's logical & physical dimensions.
  NSRect ns_logical_bounds = [screen frame];
  NSRect ns_pixel_bounds = [screen convertRectToBacking: ns_logical_bounds];
  display_config.logical_bounds = NSRectToSkIRect(ns_logical_bounds);
  display_config.pixel_bounds = NSRectToSkIRect(ns_pixel_bounds);

  // Determine the display's resolution in Dots-Per-Inch.
  NSSize ns_dots_per_inch =
      [[device_description objectForKey: NSDeviceResolution] sizeValue];
  display_config.dpi.set(ns_dots_per_inch.width, ns_dots_per_inch.height);
  display_config.logical_to_pixel_scale = [screen backingScaleFactor];

  return display_config;
}

MacDesktopConfiguration::MacDesktopConfiguration()
  : logical_bounds(SkIRect::MakeEmpty()),
    pixel_bounds(SkIRect::MakeEmpty()),
    dpi(SkIPoint::Make(0,0)),
    logical_to_pixel_scale(1.0f) {
}

MacDesktopConfiguration::~MacDesktopConfiguration() {
}

// static
MacDesktopConfiguration MacDesktopConfiguration::GetCurrent() {
  MacDesktopConfiguration desktop_config;

  NSArray* screens = [NSScreen screens];
  CHECK(screens != NULL);

  // Iterator over the monitors, adding the primary monitor and monitors whose
  // DPI match that of the primary monitor.
  for (NSUInteger i = 0; i < [screens count]; ++i) {
    MacDisplayConfiguration display_config
        = GetConfigurationForScreen([screens objectAtIndex: i]);

    // Handling mixed-DPI is hard, so we only return displays that match the
    // "primary" display's DPI.  The primary display is always the first in the
    // list returned by [NSScreen screens].
    if (i == 0) {
      desktop_config.dpi = display_config.dpi;
      desktop_config.logical_to_pixel_scale =
          display_config.logical_to_pixel_scale;
    } else if (display_config.dpi != desktop_config.dpi) {
      continue;
    }

    CHECK_EQ(desktop_config.logical_to_pixel_scale,
             display_config.logical_to_pixel_scale);

    // Add the display to the configuration.
    desktop_config.displays.push_back(display_config);

    // Update the desktop bounds to account for this display.
    desktop_config.logical_bounds.join(display_config.logical_bounds);
    desktop_config.pixel_bounds.join(display_config.pixel_bounds);
  }

  return desktop_config;
}

}  // namespace remoting
