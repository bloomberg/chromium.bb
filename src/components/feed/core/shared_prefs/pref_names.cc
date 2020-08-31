// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_SHARED_PREFS_PREF_NAMES_CC_
#define COMPONENTS_FEED_CORE_SHARED_PREFS_PREF_NAMES_CC_

#include "components/feed/core/shared_prefs/pref_names.h"

#include "components/prefs/pref_registry_simple.h"

namespace feed {
namespace prefs {

const char kEnableSnippets[] = "ntp_snippets.enable";
const char kArticlesListVisible[] = "ntp_snippets.list_visible";

void RegisterFeedSharedProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kEnableSnippets, true);
  registry->RegisterBooleanPref(kArticlesListVisible, true);
}

}  // namespace prefs
}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_SHARED_PREFS_PREF_NAMES_CC_
