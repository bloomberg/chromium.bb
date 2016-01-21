// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/base/cocoa/appkit_utils.h"

#include "base/mac/mac_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

// Gets an NSImage given an image id.
NSImage* GetImage(int image_id) {
  return ui::ResourceBundle::GetSharedInstance().GetNativeImageNamed(image_id)
      .ToNSImage();
}

// Double-click in window title bar actions.
enum class DoubleClickAction {
  NONE,
  MINIMIZE,
  MAXIMIZE,
};

// The action to take when the user double-clicks in the window title bar.
DoubleClickAction WindowTitleBarDoubleClickAction() {
  // El Capitan introduced a Dock preference to configure the window title bar
  // double-click action (Minimize, Maximize, or nothing).
  if (base::mac::IsOSElCapitanOrLater()) {
    NSString* doubleClickAction = [[NSUserDefaults standardUserDefaults]
                                      objectForKey:@"AppleActionOnDoubleClick"];

    if ([doubleClickAction isEqualToString:@"Minimize"]) {
      return DoubleClickAction::MINIMIZE;
    } else if ([doubleClickAction isEqualToString:@"Maximize"]) {
      return DoubleClickAction::MAXIMIZE;
    }

    return DoubleClickAction::NONE;
  }

  // Determine minimize using an undocumented method in Cocoa. If we're
  // running on an earlier version of the OS that doesn't implement it,
  // just default to the minimize action.
  BOOL methodImplemented =
      [NSWindow respondsToSelector:@selector(_shouldMiniaturizeOnDoubleClick)];
  if (!methodImplemented ||
      [NSWindow performSelector:@selector(_shouldMiniaturizeOnDoubleClick)]) {
    return DoubleClickAction::MINIMIZE;
  }

  // At this point _shouldMiniaturizeOnDoubleClick has returned |NO|. On
  // Yosemite, that means a double-click should Maximize the window, and on
  // all prior OSes a double-click should do nothing.
  return base::mac::IsOSYosemite() ? DoubleClickAction::MAXIMIZE
                                   : DoubleClickAction::NONE;
}

}  // namespace

namespace ui {

void DrawNinePartImage(NSRect frame,
                       const NinePartImageIds& image_ids,
                       NSCompositingOperation operation,
                       CGFloat alpha,
                       BOOL flipped) {
  NSDrawNinePartImage(frame,
                      GetImage(image_ids.top_left),
                      GetImage(image_ids.top),
                      GetImage(image_ids.top_right),
                      GetImage(image_ids.left),
                      GetImage(image_ids.center),
                      GetImage(image_ids.right),
                      GetImage(image_ids.bottom_left),
                      GetImage(image_ids.bottom),
                      GetImage(image_ids.bottom_right),
                      operation,
                      alpha,
                      flipped);
}

void WindowTitlebarReceivedDoubleClick(NSWindow* window, id sender) {
  switch (WindowTitleBarDoubleClickAction()) {
    case DoubleClickAction::MINIMIZE:
      [window performMiniaturize:sender];
      break;

    case DoubleClickAction::MAXIMIZE:
      [window performZoom:sender];
      break;

    case DoubleClickAction::NONE:
      break;
  }
}

}  // namespace ui
