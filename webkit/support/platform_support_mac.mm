// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/support/platform_support.h"

#import <AppKit/AppKit.h>
#include <AvailabilityMacros.h>
#import <Foundation/Foundation.h>
#import <objc/objc-runtime.h>

#include "base/base_paths.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "grit/webkit_resources.h"
#include "ui/base/resource/data_pack.h"
#import "webkit/support/drt_application_mac.h"
#import "webkit/support/mac/DumpRenderTreePasteboard.h"
#include "webkit/support/test_webkit_platform_support.h"

namespace webkit_support {

static NSAutoreleasePool* autorelease_pool;

void BeforeInitialize() {
  [CrDrtApplication sharedApplication];
  autorelease_pool = [[NSAutoreleasePool alloc] init];
  DCHECK(autorelease_pool);
}

void AfterInitialize() {
}

void BeforeShutdown() {
}

void AfterShutdown() {
  [DumpRenderTreePasteboard releaseLocalPasteboards];
  [autorelease_pool drain];
}

}  // namespace webkit_support

