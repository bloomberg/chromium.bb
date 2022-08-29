// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_READING_LIST_FEATURES_READING_LIST_SWITCHES_H_
#define COMPONENTS_READING_LIST_FEATURES_READING_LIST_SWITCHES_H_

#include "base/feature_list.h"
#include "build/build_config.h"

namespace reading_list {
namespace switches {

// Feature flag used for enabling Read later on desktop and Android.
extern const base::Feature kReadLater;

// Whether Reading List is enabled on this device. On iOS this is true if the
// buildflag for Reading List is enabled (no experiment). On Desktop it is also
// true if `kSidePanel` is enabled as it assumes a reading list.
bool IsReadingListEnabled();

// Feature flag used for enabling the reading list backend migration.
// When enabled, reading list data will also be stored in the Bookmarks backend.
// This allows each platform to migrate their reading list front end to point at
// the new reading list data stored in the bookmarks backend without
// interruption to cross device sync if some syncing devices are on versions
// with the migration behavior while others aren't. See crbug/1234426 for more
// details.
extern const base::Feature kReadLaterBackendMigration;

#if BUILDFLAG(IS_ANDROID)
// Feature flag used for enabling read later reminder notification.
extern const base::Feature kReadLaterReminderNotification;
#endif

}  // namespace switches
}  // namespace reading_list

#endif  // COMPONENTS_READING_LIST_FEATURES_READING_LIST_SWITCHES_H_
