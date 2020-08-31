// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/screen.h"

#import <AppKit/AppKit.h>
#import <ApplicationServices/ApplicationServices.h>
#import <Cocoa/Cocoa.h>
#include <stdint.h>

#include <map>
#include <memory>

#include "base/bind.h"
#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_nsobject.h"
#include "base/mac/sdk_forward_declarations.h"
#include "base/stl_util.h"
#include "base/timer/timer.h"
#include "ui/display/display.h"
#include "ui/display/display_change_notifier.h"
#include "ui/display/mac/display_link_mac.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/icc_profile.h"
#include "ui/gfx/mac/coordinate_conversion.h"

extern "C" {
Boolean CGDisplayUsesForceToGray(void);
}

namespace display {
namespace {

NSScreen* GetMatchingScreen(const gfx::Rect& match_rect) {
  // Default to the monitor with the current keyboard focus, in case
  // |match_rect| is not on any screen at all.
  NSScreen* max_screen = [NSScreen mainScreen];
  int max_area = 0;

  for (NSScreen* screen in [NSScreen screens]) {
    gfx::Rect monitor_area = gfx::ScreenRectFromNSRect([screen frame]);
    gfx::Rect intersection = gfx::IntersectRects(monitor_area, match_rect);
    int area = intersection.width() * intersection.height();
    if (area > max_area) {
      max_area = area;
      max_screen = screen;
    }
  }

  return max_screen;
}

Display BuildDisplayForScreen(NSScreen* screen) {
  NSRect frame = [screen frame];

  CGDirectDisplayID display_id = [[[screen deviceDescription]
      objectForKey:@"NSScreenNumber"] unsignedIntValue];

  Display display(display_id, gfx::Rect(NSRectToCGRect(frame)));
  NSRect visible_frame = [screen visibleFrame];
  NSScreen* primary = [[NSScreen screens] firstObject];

  // Convert work area's coordinate systems.
  if ([screen isEqual:primary]) {
    gfx::Rect work_area = gfx::Rect(NSRectToCGRect(visible_frame));
    work_area.set_y(frame.size.height - visible_frame.origin.y -
                    visible_frame.size.height);
    display.set_work_area(work_area);
  } else {
    display.set_bounds(gfx::ScreenRectFromNSRect(frame));
    display.set_work_area(gfx::ScreenRectFromNSRect(visible_frame));
  }

  // Compute device scale factor
  CGFloat scale = [screen backingScaleFactor];
  if (Display::HasForceDeviceScaleFactor())
    scale = Display::GetForcedDeviceScaleFactor();
  display.set_device_scale_factor(scale);

  // Examine the presence of HDR.
  bool enable_hdr = false;
  if (@available(macOS 10.15, *)) {
    if ([screen maximumPotentialExtendedDynamicRangeColorComponentValue] >
        1.0) {
      enable_hdr = true;
    }
  }

  // Compute DisplayColorSpaces.
  gfx::ICCProfile icc_profile;
  {
    CGColorSpaceRef cg_color_space = [[screen colorSpace] CGColorSpace];
    if (cg_color_space) {
      base::ScopedCFTypeRef<CFDataRef> cf_icc_profile(
          CGColorSpaceCopyICCProfile(cg_color_space));
      if (cf_icc_profile) {
        icc_profile = gfx::ICCProfile::FromData(
            CFDataGetBytePtr(cf_icc_profile), CFDataGetLength(cf_icc_profile));
      }
    }
  }
  gfx::DisplayColorSpaces display_color_spaces(icc_profile.GetColorSpace(),
                                               gfx::BufferFormat::RGBA_8888);
  if (Display::HasForceDisplayColorProfile()) {
    if (Display::HasEnsureForcedColorProfile()) {
      if (display_color_spaces != display.color_spaces()) {
        LOG(FATAL) << "The display's color space does not match the color "
                      "space that was forced by the command line. This will "
                      "cause pixel tests to fail.";
      }
    }
  } else {
    if (enable_hdr) {
      bool needs_alpha_values[] = {true, false};
      for (const auto& needs_alpha : needs_alpha_values) {
        display_color_spaces.SetOutputColorSpaceAndBufferFormat(
            gfx::ContentColorUsage::kHDR, needs_alpha,
            gfx::ColorSpace::CreateExtendedSRGB(), gfx::BufferFormat::RGBA_F16);
      }
    }
    display.set_color_spaces(display_color_spaces);
  }

  if (enable_hdr) {
    display.set_color_depth(Display::kHDR10BitsPerPixel);
    display.set_depth_per_component(Display::kHDR10BitsPerComponent);
  } else {
    display.set_color_depth(Display::kDefaultBitsPerPixel);
    display.set_depth_per_component(Display::kDefaultBitsPerComponent);
  }
  display.set_is_monochrome(CGDisplayUsesForceToGray());

  if (auto display_link = ui::DisplayLinkMac::GetForDisplay(display_id))
    display.set_display_frequency(display_link->GetRefreshRate());

  // CGDisplayRotation returns a double. Display::SetRotationAsDegree will
  // handle the unexpected situations were the angle is not a multiple of 90.
  display.SetRotationAsDegree(static_cast<int>(CGDisplayRotation(display_id)));
  return display;
}

Display BuildPrimaryDisplay() {
  return BuildDisplayForScreen([[NSScreen screens] firstObject]);
}

std::vector<Display> BuildDisplaysFromQuartz() {
  // Don't just return all online displays.  This would include displays
  // that mirror other displays, which are not desired in this list.  It's
  // tempting to use the count returned by CGGetActiveDisplayList, but active
  // displays exclude sleeping displays, and those are desired.

  // It would be ridiculous to have this many displays connected, but
  // CGDirectDisplayID is just an integer, so supporting up to this many
  // doesn't hurt.
  CGDirectDisplayID online_displays[1024];
  CGDisplayCount online_display_count = 0;
  if (CGGetOnlineDisplayList(base::size(online_displays), online_displays,
                             &online_display_count) != kCGErrorSuccess) {
    return std::vector<Display>(1, BuildPrimaryDisplay());
  }

  typedef std::map<int64_t, NSScreen*> ScreenIdsToScreensMap;
  ScreenIdsToScreensMap screen_ids_to_screens;
  for (NSScreen* screen in [NSScreen screens]) {
    NSDictionary* screen_device_description = [screen deviceDescription];
    int64_t screen_id = [[screen_device_description
        objectForKey:@"NSScreenNumber"] unsignedIntValue];
    screen_ids_to_screens[screen_id] = screen;
  }

  std::vector<Display> displays;
  for (CGDisplayCount online_display_index = 0;
       online_display_index < online_display_count; ++online_display_index) {
    CGDirectDisplayID online_display = online_displays[online_display_index];
    if (CGDisplayMirrorsDisplay(online_display) == kCGNullDirectDisplay) {
      // If this display doesn't mirror any other, include it in the list.
      // The primary display in a mirrored set will be counted, but those that
      // mirror it will not be.
      ScreenIdsToScreensMap::iterator foundScreen =
          screen_ids_to_screens.find(online_display);
      if (foundScreen != screen_ids_to_screens.end()) {
        displays.push_back(BuildDisplayForScreen(foundScreen->second));
      }
    }
  }

  return displays.empty() ? std::vector<Display>(1, BuildPrimaryDisplay())
                          : displays;
}

// Returns the minimum Manhattan distance from |point| to corners of |screen|
// frame.
CGFloat GetMinimumDistanceToCorner(const NSPoint& point, NSScreen* screen) {
  NSRect frame = [screen frame];
  CGFloat distance =
      fabs(point.x - NSMinX(frame)) + fabs(point.y - NSMinY(frame));
  distance = std::min(
      distance, fabs(point.x - NSMaxX(frame)) + fabs(point.y - NSMinY(frame)));
  distance = std::min(
      distance, fabs(point.x - NSMinX(frame)) + fabs(point.y - NSMaxY(frame)));
  distance = std::min(
      distance, fabs(point.x - NSMaxX(frame)) + fabs(point.y - NSMaxY(frame)));
  return distance;
}

class ScreenMac : public Screen {
 public:
  ScreenMac() {
    displays_ = BuildDisplaysFromQuartz();
    CGDisplayRegisterReconfigurationCallback(
        ScreenMac::DisplayReconfigurationCallBack, this);

    auto update_block = ^(NSNotification* notification) {
      OnNSScreensMayHaveChanged();
    };

    NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
    screen_color_change_observer_.reset(
        [[center addObserverForName:NSScreenColorSpaceDidChangeNotification
                             object:nil
                              queue:nil
                         usingBlock:update_block] retain]);
    screen_params_change_observer_.reset([[center
        addObserverForName:NSApplicationDidChangeScreenParametersNotification
                    object:nil
                     queue:nil
                usingBlock:update_block] retain]);
  }

  ~ScreenMac() override {
    NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
    [center removeObserver:screen_color_change_observer_];
    [center removeObserver:screen_params_change_observer_];

    CGDisplayRemoveReconfigurationCallback(
        ScreenMac::DisplayReconfigurationCallBack, this);
  }

  gfx::Point GetCursorScreenPoint() override {
    // Flip coordinates to gfx (0,0 in top-left corner) using primary screen.
    return gfx::ScreenPointFromNSPoint([NSEvent mouseLocation]);
  }

  bool IsWindowUnderCursor(gfx::NativeWindow native_window) override {
    NSWindow* window = native_window.GetNativeNSWindow();
    return [NSWindow windowNumberAtPoint:[NSEvent mouseLocation]
             belowWindowWithWindowNumber:0] == [window windowNumber];
  }

  gfx::NativeWindow GetWindowAtScreenPoint(const gfx::Point& point) override {
    NOTIMPLEMENTED();
    return gfx::NativeWindow();
  }

  gfx::NativeWindow GetLocalProcessWindowAtPoint(
      const gfx::Point& point,
      const std::set<gfx::NativeWindow>& ignore) override {
    const NSPoint ns_point = gfx::ScreenPointToNSPoint(point);

    // Note: [NSApp orderedWindows] doesn't include NSPanels.
    for (NSWindow* window : [NSApp orderedWindows]) {
      if (ignore.count(window))
        continue;

      if (![window isOnActiveSpace])
        continue;

      // NativeWidgetMac::Close() calls -orderOut: on NSWindows before actually
      // closing them.
      if (![window isVisible])
        continue;

      if (NSPointInRect(ns_point, [window frame]))
        return window;
    }

    return nil;
  }

  int GetNumDisplays() const override { return GetAllDisplays().size(); }

  const std::vector<Display>& GetAllDisplays() const override {
    return displays_;
  }

  Display GetDisplayNearestWindow(
      gfx::NativeWindow native_window) const override {
    if (displays_.size() == 1)
      return displays_[0];

    NSWindow* window = native_window.GetNativeNSWindow();
    if (!window)
      return GetPrimaryDisplay();

    // Note the following line calls -[NSWindow
    // _bestScreenBySpaceAssignmentOrGeometry] which is quite expensive and
    // performs IPC with the window server process.
    NSScreen* match_screen = [window screen];

    if (!match_screen)
      return GetPrimaryDisplay();
    return GetCachedDisplayForScreen(match_screen);
  }

  Display GetDisplayNearestView(gfx::NativeView native_view) const override {
    NSView* view = native_view.GetNativeNSView();
    NSWindow* window = [view window];
    if (!window)
      return GetPrimaryDisplay();
    return GetDisplayNearestWindow(window);
  }

  Display GetDisplayNearestPoint(const gfx::Point& point) const override {
    NSArray* screens = [NSScreen screens];
    if ([screens count] <= 1)
      return GetPrimaryDisplay();

    NSPoint ns_point = NSPointFromCGPoint(point.ToCGPoint());
    NSScreen* primary = [screens objectAtIndex:0];
    ns_point.y = NSMaxY([primary frame]) - ns_point.y;
    for (NSScreen* screen in screens) {
      if (NSMouseInRect(ns_point, [screen frame], NO))
        return GetCachedDisplayForScreen(screen);
    }

    NSScreen* nearest_screen = primary;
    CGFloat min_distance = CGFLOAT_MAX;
    for (NSScreen* screen in screens) {
      CGFloat distance = GetMinimumDistanceToCorner(ns_point, screen);
      if (distance < min_distance) {
        min_distance = distance;
        nearest_screen = screen;
      }
    }
    return GetCachedDisplayForScreen(nearest_screen);
  }

  // Returns the display that most closely intersects the provided bounds.
  Display GetDisplayMatching(const gfx::Rect& match_rect) const override {
    NSScreen* match_screen = GetMatchingScreen(match_rect);
    return GetCachedDisplayForScreen(match_screen);
  }

  // Returns the primary display.
  Display GetPrimaryDisplay() const override {
    // Primary display is defined as the display with the menubar,
    // which is always at index 0.
    NSScreen* primary = [[NSScreen screens] firstObject];
    Display display = GetCachedDisplayForScreen(primary);
    return display;
  }

  void AddObserver(DisplayObserver* observer) override {
    change_notifier_.AddObserver(observer);
  }

  void RemoveObserver(DisplayObserver* observer) override {
    change_notifier_.RemoveObserver(observer);
  }

  static void DisplayReconfigurationCallBack(CGDirectDisplayID display,
                                             CGDisplayChangeSummaryFlags flags,
                                             void* userInfo) {
    ScreenMac* screen_mac = static_cast<ScreenMac*>(userInfo);
    screen_mac->OnNSScreensMayHaveChanged();
  }

 private:
  Display GetCachedDisplayForScreen(NSScreen* screen) const {
    const CGDirectDisplayID display_id = [[[screen deviceDescription]
        objectForKey:@"NSScreenNumber"] unsignedIntValue];
    for (const Display& display : displays_) {
      if (display_id == display.id())
        return display;
    }
    // In theory, this should not be reached, but in practice, on Catalina, it
    // has been observed that -[NSScreen screens] changes before any
    // notifications are received.
    // https://crbug.com/1021340.
    DLOG(ERROR) << "Value of -[NSScreen screens] changed before notification.";
    return BuildDisplayForScreen(screen);
  }

  void OnNSScreensMayHaveChanged() {
    auto new_displays = BuildDisplaysFromQuartz();
    if (displays_ == new_displays)
      return;
    auto old_displays = std::move(displays_);
    displays_ = std::move(new_displays);
    change_notifier_.NotifyDisplaysChanged(old_displays, displays_);
  }

  // The displays currently attached to the device. Updated by
  // OnNSScreensMayHaveChanged.
  std::vector<Display> displays_;

  // The observers notified by NSScreenColorSpaceDidChangeNotification and
  // NSApplicationDidChangeScreenParametersNotification.
  base::scoped_nsobject<id> screen_color_change_observer_;
  base::scoped_nsobject<id> screen_params_change_observer_;

  DisplayChangeNotifier change_notifier_;

  DISALLOW_COPY_AND_ASSIGN(ScreenMac);
};

}  // namespace

// static
gfx::NativeWindow Screen::GetWindowForView(gfx::NativeView native_view) {
#if !defined(USE_AURA)
  NSView* view = native_view.GetNativeNSView();
  return [view window];
#else
  gfx::NativeWindow window = nil;
  return window;
#endif
}

#if !defined(USE_AURA)
Screen* CreateNativeScreen() {
  return new ScreenMac;
}
#endif

}  // namespace display
