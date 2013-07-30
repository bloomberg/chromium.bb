// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "remoting/base/resources.h"
#include "base/mac/bundle_locations.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/base/resource/resource_bundle.h"

// A dummy class used to locate the host plugin's bundle.
@interface NSBundleLocator : NSObject
@end

@implementation NSBundleLocator
@end

namespace remoting {

bool LoadResources(const std::string& pref_locale) {
  if (ui::ResourceBundle::HasSharedInstance()) {
    ui::ResourceBundle::GetSharedInstance().ReloadLocaleResources(pref_locale);
  } else {
    // Use the plugin's bundle instead of the hosting app bundle.
    base::mac::SetOverrideFrameworkBundle(
        [NSBundle bundleForClass:[NSBundleLocator class]]);

    // Override the locale with the value from Cocoa.
    if (pref_locale.empty())
      l10n_util::OverrideLocaleWithCocoaLocale();

    ui::ResourceBundle::InitSharedInstanceLocaleOnly(pref_locale, NULL);
  }

  return true;
}

void UnloadResources() {
  ui::ResourceBundle::CleanupSharedInstance();
}

}  // namespace remoting
