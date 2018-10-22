// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_switcher/browser_switcher_prefs.h"

#include "components/pref_registry/pref_registry_syncable.h"

namespace browser_switcher {
namespace prefs {

// List of host domain names to be opened in an alternative browser.
const char kUrlList[] = "browser_switcher.url_list";

// List of hosts that should not trigger a transition in either browser.
const char kUrlGreylist[] = "browser_switcher.url_greylist";

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(prefs::kUrlList);
  registry->RegisterListPref(prefs::kUrlGreylist);
}

}  // namespace prefs
}  // namespace browser_switcher
