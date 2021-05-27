// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/cefclient/browser/window_test_runner_mac.h"

#import <Cocoa/Cocoa.h>

#include "include/wrapper/cef_helpers.h"
#include "tests/shared/browser/main_message_loop.h"

namespace client {
namespace window_test {

namespace {

NSWindow* GetWindow(CefRefPtr<CefBrowser> browser) {
  NSView* view =
      CAST_CEF_WINDOW_HANDLE_TO_NSVIEW(browser->GetHost()->GetWindowHandle());
  return [view window];
}

}  // namespace

WindowTestRunnerMac::WindowTestRunnerMac() {}

void WindowTestRunnerMac::SetPos(CefRefPtr<CefBrowser> browser,
                                 int x,
                                 int y,
                                 int width,
                                 int height) {
  CEF_REQUIRE_UI_THREAD();
  REQUIRE_MAIN_THREAD();

  NSWindow* window = GetWindow(browser);

  // Make sure the window isn't minimized or maximized.
  if ([window isMiniaturized])
    [window deminiaturize:nil];
  else if ([window isZoomed])
    [window performZoom:nil];

  // Retrieve information for the display that contains the window.
  NSScreen* screen = [window screen];
  if (screen == nil)
    screen = [NSScreen mainScreen];
  NSRect frame = [screen frame];
  NSRect visibleFrame = [screen visibleFrame];

  // Make sure the window is inside the display.
  CefRect display_rect(
      visibleFrame.origin.x,
      frame.size.height - visibleFrame.size.height - visibleFrame.origin.y,
      visibleFrame.size.width, visibleFrame.size.height);
  CefRect window_rect(x, y, width, height);
  ModifyBounds(display_rect, window_rect);

  NSRect newRect;
  newRect.origin.x = window_rect.x;
  newRect.origin.y = frame.size.height - window_rect.height - window_rect.y;
  newRect.size.width = window_rect.width;
  newRect.size.height = window_rect.height;
  [window setFrame:newRect display:YES];
}

void WindowTestRunnerMac::Minimize(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();
  REQUIRE_MAIN_THREAD();

  [GetWindow(browser) performMiniaturize:nil];
}

void WindowTestRunnerMac::Maximize(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();
  REQUIRE_MAIN_THREAD();

  [GetWindow(browser) performZoom:nil];
}

void WindowTestRunnerMac::Restore(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();
  REQUIRE_MAIN_THREAD();

  NSWindow* window = GetWindow(browser);
  if ([window isMiniaturized])
    [window deminiaturize:nil];
  else if ([window isZoomed])
    [window performZoom:nil];
}

}  // namespace window_test
}  // namespace client
