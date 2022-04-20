// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reading_list/features/reading_list_switches.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "build/build_config.h"
#include "components/reading_list/features/reading_list_buildflags.h"

namespace reading_list {
namespace switches {

// Allow users to save tabs for later. Enables a new button and menu for
// accessing tabs saved for later.
// android: https://crbug.com/1123087
// desktop: https://crbug.com/1109316
// ios: https://crbug.com/577659
const base::Feature kReadLater{"ReadLater", base::FEATURE_ENABLED_BY_DEFAULT};

bool IsReadingListEnabled() {
#if BUILDFLAG(IS_IOS)
  return BUILDFLAG(ENABLE_READING_LIST);
#else
  return base::FeatureList::IsEnabled(kReadLater);
#endif
}

const base::Feature kReadLaterBackendMigration{
    "ReadLaterBackendMigration", base::FEATURE_DISABLED_BY_DEFAULT};

#if BUILDFLAG(IS_ANDROID)
// Feature flag used for enabling read later reminder notification.
const base::Feature kReadLaterReminderNotification{
    "ReadLaterReminderNotification", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

}  // namespace switches
}  // namespace reading_list
