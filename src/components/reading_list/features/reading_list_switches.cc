// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reading_list/features/reading_list_switches.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "build/build_config.h"
#include "components/reading_list/features/reading_list_buildflags.h"

#if !defined(OS_IOS)
namespace features {
// Hosts some content in a side panel. https://crbug.com/1149995
const base::Feature kSidePanel{"SidePanel", base::FEATURE_DISABLED_BY_DEFAULT};
}  // namespace features
#endif  // !defined(OS_IOS)

namespace reading_list {
namespace switches {

// Allow users to save tabs for later. Enables a new button and menu for
// accessing tabs saved for later. https://crbug.com/1109316
#if defined(OS_ANDROID)
const base::Feature kReadLater{"ReadLater", base::FEATURE_DISABLED_BY_DEFAULT};
#else
const base::Feature kReadLater{"ReadLater", base::FEATURE_ENABLED_BY_DEFAULT};
#endif

bool IsReadingListEnabled() {
#if defined(OS_IOS)
  return BUILDFLAG(ENABLE_READING_LIST);
#elif defined(OS_ANDROID)
  return base::FeatureList::IsEnabled(kReadLater);
#else
  return base::FeatureList::IsEnabled(kReadLater) ||
         base::FeatureList::IsEnabled(features::kSidePanel);
#endif
}

const base::Feature kReadLaterBackendMigration{
    "ReadLaterBackendMigration", base::FEATURE_DISABLED_BY_DEFAULT};

#ifdef OS_ANDROID
// Feature flag used for enabling read later reminder notification.
const base::Feature kReadLaterReminderNotification{
    "ReadLaterReminderNotification", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

}  // namespace switches
}  // namespace reading_list
