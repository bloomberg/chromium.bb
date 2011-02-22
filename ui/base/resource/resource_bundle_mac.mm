// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/resource/resource_bundle.h"

#import <Foundation/Foundation.h>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/mac/mac_util.h"
#include "base/sys_string_conversions.h"
#include "skia/ext/skia_utils_mac.h"

namespace ui {

namespace {

FilePath GetResourcesPakFilePath(NSString* name, NSString* mac_locale) {
  NSString *resource_path;
  // Some of the helper processes need to be able to fetch resources
  // (chrome_main.cc: SubprocessNeedsResourceBundle()). Fetch the same locale
  // as the already-running browser instead of using what NSBundle might pick
  // based on values at helper launch time.
  if ([mac_locale length]) {
    resource_path = [base::mac::MainAppBundle() pathForResource:name
                                                        ofType:@"pak"
                                                   inDirectory:@""
                                               forLocalization:mac_locale];
  } else {
    resource_path = [base::mac::MainAppBundle() pathForResource:name
                                                        ofType:@"pak"];
  }
  if (!resource_path)
    return FilePath();
  return FilePath([resource_path fileSystemRepresentation]);
}

}  // namespace

// static
FilePath ResourceBundle::GetResourcesFilePath() {
  return GetResourcesPakFilePath(@"chrome", nil);
}

// static
FilePath ResourceBundle::GetLocaleFilePath(const std::string& app_locale) {
  NSString* mac_locale = base::SysUTF8ToNSString(app_locale);

  // Mac OS X uses "_" instead of "-", so swap to get a Mac-style value.
  mac_locale = [mac_locale stringByReplacingOccurrencesOfString:@"-"
                                                     withString:@"_"];

  // On disk, the "en_US" resources are just "en" (http://crbug.com/25578).
  if ([mac_locale isEqual:@"en_US"])
    mac_locale = @"en";

  return GetResourcesPakFilePath(@"locale", mac_locale);
}

gfx::Image& ResourceBundle::GetNativeImageNamed(int resource_id) {
  // Currently this just returns the Skia-backed image, which will convert to
  // NSImage and cache that result when necessary.
  // TODO(rsesek): Load the raw bytes directly into an NSImage instead.
  return GetImageNamed(resource_id);
}

}  // namespace ui
