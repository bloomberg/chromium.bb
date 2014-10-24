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

// Whether windows should miniaturize on a double-click on the title bar.
bool ShouldWindowsMiniaturizeOnDoubleClick() {
  // We use an undocumented method in Cocoa; if it doesn't exist, default to
  // |true|. If it ever goes away, we can do (using an undocumented pref key):
  //   NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  //   return ![defaults objectForKey:@"AppleMiniaturizeOnDoubleClick"] ||
  //          [defaults boolForKey:@"AppleMiniaturizeOnDoubleClick"];
  BOOL methodImplemented =
      [NSWindow respondsToSelector:@selector(_shouldMiniaturizeOnDoubleClick)];
  DCHECK(methodImplemented);
  return !methodImplemented ||
         [NSWindow performSelector:@selector(_shouldMiniaturizeOnDoubleClick)];
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
  if (ShouldWindowsMiniaturizeOnDoubleClick()) {
    [window performMiniaturize:sender];
  } else if (base::mac::IsOSYosemiteOrLater()) {
    [window performZoom:sender];
  }
}

}  // namespace ui
