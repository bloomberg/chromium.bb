// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/screen.h"

#import <ApplicationServices/ApplicationServices.h>
#import <Cocoa/Cocoa.h>

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
gfx::Rect Screen::GetPrimaryMonitorWorkArea() {
  // Primary monitor is defined as the monitor with the menubar,
  // which is always at index 0.
  NSScreen* primary = [[NSScreen screens] objectAtIndex:0];
  NSRect frame = [primary frame];
  NSRect visible_frame = [primary visibleFrame];

  // Convert coordinate systems.
  gfx::Rect rect = gfx::Rect(NSRectToCGRect(visible_frame));
  rect.set_y(frame.size.height - visible_frame.origin.y -
             visible_frame.size.height);
  return rect;
}

// static
gfx::Rect Screen::GetPrimaryMonitorBounds() {
  // Primary monitor is defined as the monitor with the menubar,
  // which is always at index 0.
  NSScreen* primary = [[NSScreen screens] objectAtIndex:0];
  return gfx::Rect(NSRectToCGRect([primary frame]));
}

// static
gfx::Rect Screen::GetMonitorWorkAreaMatching(const gfx::Rect& match_rect) {
  NSScreen* match_screen = GetMatchingScreen(match_rect);
  return ConvertCoordinateSystem([match_screen visibleFrame]);
}

// static
gfx::Size Screen::GetPrimaryMonitorSize() {
  CGDirectDisplayID main_display = CGMainDisplayID();
  return gfx::Size(CGDisplayPixelsWide(main_display),
                   CGDisplayPixelsHigh(main_display));
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
