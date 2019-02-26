// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_PREFS_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_PREFS_H_

class PrefService;
class PrefRegistrySimple;

namespace offline_pages {

namespace prefetch_prefs {

extern const char kBackoff[];

void RegisterPrefs(PrefRegistrySimple* registry);

// Configures the user controlled setting that enables or disables the
// prefetching of offline pages to run.
void SetPrefetchingEnabledInSettings(PrefService* prefs, bool enabled);

// Returns whether the prefetch feature is enabled. Checks both the feature
// flag and the user-controlled setting, which may change at runtime.
bool IsEnabled(PrefService* prefs);

}  // namespace prefetch_prefs
}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_PREFS_H_
