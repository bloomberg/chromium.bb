// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/test_fonts.h"

#include <AppKit/AppKit.h>
#include <Foundation/Foundation.h>

#include "base/logging.h"
#include "base/mac/bundle_locations.h"

namespace skia {

void ConfigureTestFont() {
  // Load font files in the resource folder.
  static const char* const kFontFileNames[] = {"Ahem.TTF",
                                               "ChromiumAATTest.ttf"};

  // mainBundle is Content Shell Helper.app.  Go two levels up to find
  // Content Shell.app. Due to DumpRenderTree injecting the font files into
  // its direct dependents, it's not easily possible to put the ttf files into
  // the helper's resource directory instead of the outer bundle's resource
  // directory.
  NSString* bundle = [base::mac::FrameworkBundle() bundlePath];
  bundle = [bundle stringByAppendingPathComponent:@"../.."];
  NSURL* resources_directory = [[NSBundle bundleWithPath:bundle] resourceURL];

  NSMutableArray* font_urls = [NSMutableArray array];
  for (unsigned i = 0; i < base::size(kFontFileNames); ++i) {
    NSURL* font_url = [resources_directory
        URLByAppendingPathComponent:
            [NSString stringWithUTF8String:kFontFileNames[i]]];
    [font_urls addObject:[font_url absoluteURL]];
  }

  CFArrayRef errors = 0;
  if (!CTFontManagerRegisterFontsForURLs((CFArrayRef)font_urls,
                                         kCTFontManagerScopeProcess, &errors)) {
    DLOG(FATAL) << "Fail to activate fonts.";
    CFRelease(errors);
  }
}

}  // namespace skia
