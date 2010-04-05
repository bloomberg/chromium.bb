// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/support/platform_support.h"

#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>

#include "base/logging.h"

namespace webkit_support {

static NSAutoreleasePool* autorelease_pool;

void BeforeInitialize() {
  autorelease_pool = [[NSAutoreleasePool alloc] init];
  DCHECK(autorelease_pool);
}

void AfterIniitalize() {
  // Load font files in the resource folder.
  static const char* const fontFileNames[] = {
      "AHEM____.TTF",
      "WebKitWeightWatcher100.ttf",
      "WebKitWeightWatcher200.ttf",
      "WebKitWeightWatcher300.ttf",
      "WebKitWeightWatcher400.ttf",
      "WebKitWeightWatcher500.ttf",
      "WebKitWeightWatcher600.ttf",
      "WebKitWeightWatcher700.ttf",
      "WebKitWeightWatcher800.ttf",
      "WebKitWeightWatcher900.ttf",
  };

  NSString* resources = [[NSBundle mainBundle] resourcePath];
  for (unsigned i = 0; i < arraysize(fontFileNames); ++i) {
    const char* resource_path = [[resources stringByAppendingPathComponent:
        [NSString stringWithUTF8String:fontFileNames[i]]] UTF8String];
    FSRef resource_ref;
    const UInt8* uint8_resource_path
        = reinterpret_cast<const UInt8*>(resource_path);
    if (FSPathMakeRef(uint8_resource_path, &resource_ref, nil) != noErr) {
      DLOG(FATAL) << "Fail to open " << resource_path;
    }
    if (ATSFontActivateFromFileReference(&resource_ref, kATSFontContextLocal,
        kATSFontFormatUnspecified, 0, kATSOptionFlagsDefault, 0) != noErr) {
      DLOG(FATAL) << "Fail to activate font: %s" << resource_path;
    }
  }
}

void BeforeShutdown() {
}

void AfterShutdown() {
  [autorelease_pool drain];
}

}  // namespace webkit_support
