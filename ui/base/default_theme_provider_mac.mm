// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/default_theme_provider.h"

#import <Cocoa/Cocoa.h>

#include "ui/base/resource/resource_bundle.h"

namespace ui {

#if !defined(TOOLKIT_VIEWS)
NSImage* DefaultThemeProvider::GetNSImageNamed(int id,
                                               bool allow_default) const {
 return ResourceBundle::GetSharedInstance().
     GetNativeImageNamed(id).ToNSImage();
}

NSColor* DefaultThemeProvider::GetNSImageColorNamed(int id,
                                                    bool allow_default) const {
  NSImage* image = GetNSImageNamed(id, allow_default);
  return [NSColor colorWithPatternImage:image];
}

NSColor* DefaultThemeProvider::GetNSColor(int id,
                                          bool allow_default) const {
  return [NSColor redColor];
}

NSColor* DefaultThemeProvider::GetNSColorTint(int id,
                                              bool allow_default) const {
  return [NSColor redColor];
}

NSGradient* DefaultThemeProvider::GetNSGradient(int id) const {
  return nil;
}
#endif

}  // namespace ui
