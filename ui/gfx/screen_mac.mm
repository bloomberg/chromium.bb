// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/screen.h"

#import <ApplicationServices/ApplicationServices.h>
#import <Cocoa/Cocoa.h>

@interface NSScreen (LionAPI)
- (CGFloat)backingScaleFactor;
@end

#include "base/logging.h"
#include "ui/gfx/monitor.h"

namespace {

gfx::Rect ConvertCoordinateSystem(NSRect ns_rect) {
  // Primary monitor is defined as the monitor with the menubar,
  // which is always at index 0.
  NSScreen* primary_screen = [[NSScreen screens] objectAtIndex:0];
  float primary_screen_height = [primary_screen frame].size.height;
  gfx::Rect rect(NSRectToCGRect(ns_rect));
  rect.set_y(primary_screen_height - rect.y() - rect.height());
  return rect;
}

NSScreen* GetMatchingScreen(const gfx::Rect& match_rect) {
  // Default to the monitor with the current keyboard focus, in case
  // |match_rect| is not on any screen at all.
  NSScreen* max_screen = [NSScreen mainScreen];
  int max_area = 0;

  for (NSScreen* screen in [NSScreen screens]) {
    gfx::Rect monitor_area = ConvertCoordinateSystem([screen frame]);
    gfx::Rect intersection = monitor_area.Intersect(match_rect);
    int area = intersection.width() * intersection.height();
    if (area > max_area) {
      max_area = area;
      max_screen = screen;
    }
  }

  return max_screen;
}

gfx::Monitor GetMonitorForScreen(NSScreen* screen, bool is_primary) {
  NSRect frame = [screen frame];
  // TODO(oshima): Implement ID and Observer.
  gfx::Monitor monitor(0, gfx::Rect(NSRectToCGRect(frame)));

  NSRect visible_frame = [screen visibleFrame];

  // Convert work area's coordinate systems.
  if (is_primary) {
    gfx::Rect work_area = gfx::Rect(NSRectToCGRect(visible_frame));
    work_area.set_y(frame.size.height - visible_frame.origin.y -
                    visible_frame.size.height);
    monitor.set_work_area(work_area);
  } else {
    monitor.set_work_area(ConvertCoordinateSystem(visible_frame));
  }
  CGFloat scale;
  if ([screen respondsToSelector:@selector(backingScaleFactor)])
    scale = [screen backingScaleFactor];
  else
    scale = [screen userSpaceScaleFactor];
  monitor.set_device_scale_factor(scale);
  return monitor;
}

}  // namespace

namespace gfx {

// static
gfx::Point Screen::GetCursorScreenPoint() {
  NSPoint mouseLocation  = [NSEvent mouseLocation];
  // Flip coordinates to gfx (0,0 in top-left corner) using primary screen.
  NSScreen* screen = [[NSScreen screens] objectAtIndex:0];
  mouseLocation.y = NSMaxY([screen frame]) - mouseLocation.y;
  return gfx::Point(mouseLocation.x, mouseLocation.y);
}

// static
gfx::Monitor Screen::GetPrimaryMonitor() {
  // Primary monitor is defined as the monitor with the menubar,
  // which is always at index 0.
  NSScreen* primary = [[NSScreen screens] objectAtIndex:0];
  gfx::Monitor monitor = GetMonitorForScreen(primary, true /* primary */);
  return monitor;
}

// static
gfx::Monitor Screen::GetMonitorMatching(const gfx::Rect& match_rect) {
  NSScreen* match_screen = GetMatchingScreen(match_rect);
  return GetMonitorForScreen(match_screen, false /* may not be primary */);
}

// static
int Screen::GetNumMonitors() {
  // Don't just return the number of online displays.  It includes displays
  // that mirror other displays, which are not desired in the count.  It's
  // tempting to use the count returned by CGGetActiveDisplayList, but active
  // displays exclude sleeping displays, and those are desired in the count.

  // It would be ridiculous to have this many displays connected, but
  // CGDirectDisplayID is just an integer, so supporting up to this many
  // doesn't hurt.
  CGDirectDisplayID online_displays[128];
  CGDisplayCount online_display_count = 0;
  if (CGGetOnlineDisplayList(arraysize(online_displays),
                             online_displays,
                             &online_display_count) != kCGErrorSuccess) {
    // 1 is a reasonable assumption.
    return 1;
  }

  int display_count = 0;
  for (CGDisplayCount online_display_index = 0;
       online_display_index < online_display_count;
       ++online_display_index) {
    CGDirectDisplayID online_display = online_displays[online_display_index];
    if (CGDisplayMirrorsDisplay(online_display) == kCGNullDirectDisplay) {
      // If this display doesn't mirror any other, include it in the count.
      // The primary display in a mirrored set will be counted, but those that
      // mirror it will not be.
      ++display_count;
    }
  }

  return display_count;
}

}  // namespace gfx
