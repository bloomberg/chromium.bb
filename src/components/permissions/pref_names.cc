// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/pref_names.h"

namespace permissions {
namespace prefs {

#if defined(OS_ANDROID)
// The current level of backoff for showing the location settings dialog for the
// default search engine.
const char kLocationSettingsBackoffLevelDSE[] =
    "location_settings_backoff_level_dse";

// The current level of backoff for showing the location settings dialog for
// sites other than the default search engine.
const char kLocationSettingsBackoffLevelDefault[] =
    "location_settings_backoff_level_default";

// The next time the location settings dialog can be shown for the default
// search engine.
const char kLocationSettingsNextShowDSE[] = "location_settings_next_show_dse";

// The next time the location settings dialog can be shown for sites other than
// the default search engine.
const char kLocationSettingsNextShowDefault[] =
    "location_settings_next_show_default";
#endif

}  // namespace prefs
}  // namespace permissions
