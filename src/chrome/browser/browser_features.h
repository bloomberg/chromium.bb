// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines the browser-specific base::FeatureList features that are
// not shared with other process types.

#ifndef CHROME_BROWSER_BROWSER_FEATURES_H_
#define CHROME_BROWSER_BROWSER_FEATURES_H_

#include "base/feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

namespace features {

// All features in alphabetical order. The features should be documented
// alongside the definition of their values in the .cc file.

extern const base::Feature kClosedTabCache;

extern const base::Feature kDestroyProfileOnBrowserClose;

extern const base::Feature kPromoBrowserCommands;
extern const char kBrowserCommandIdParam[];

#if BUILDFLAG(IS_CHROMEOS_ASH)
extern const base::Feature kDoubleTapToZoomInTabletMode;
extern const base::Feature kQuickSettingsPWANotifications;
#endif

#if !defined(OS_ANDROID)
extern const base::Feature kCopyLinkToText;
extern const base::Feature kMuteNotificationSnoozeAction;
#endif

extern const base::Feature kPwaUpdateDialogForNameAndIcon;

#if !defined(OS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
extern const base::Feature kUserDataSnapshot;
#endif

extern const base::Feature kSandboxExternalProtocolBlocked;
extern const base::Feature kTriggerNetworkDataMigration;

// Returns true if network context data should be migrated to the new data path.
// Prefer using this function over querying the kTriggerNetworkDataMigration
// feature directly, as there might be other platform specific factors that
// determine whether data migration should occur or not.
bool ShouldTriggerNetworkDataMigration();

extern const base::Feature kWebUsbDeviceDetection;

}  // namespace features

#endif  // CHROME_BROWSER_BROWSER_FEATURES_H_
