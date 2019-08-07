// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_PINNED_TAB_CODEC_H_
#define CHROME_BROWSER_UI_TABS_PINNED_TAB_CODEC_H_

#include <vector>

#include "base/macros.h"
#include "chrome/browser/ui/startup/startup_tab.h"
#include "url/gurl.h"

class Profile;

namespace base {
class Value;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

// PinnedTabCodec is used to read and write the set of pinned tabs to
// preferences. When Chrome exits the sets of pinned tabs are written to prefs.
// On startup if the user has not chosen to restore the last session the set of
// pinned tabs is opened.
//
// The entries are stored in preferences as a list of dictionaries, with each
// dictionary describing the entry.
class PinnedTabCodec {
 public:
  // Registers the preference used by this class.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Resets the preferences state.
  static void WritePinnedTabs(Profile* profile);

  // Sets the preferences state from the specified tab list.
  static void WritePinnedTabs(Profile* profile, const StartupTabs& tabs);

  // Reads and returns the set of pinned tabs to restore from preferences.
  static StartupTabs ReadPinnedTabs(Profile* profile);
  static StartupTabs ReadPinnedTabs(const base::Value* value);

 private:
  PinnedTabCodec();
  ~PinnedTabCodec();

  DISALLOW_COPY_AND_ASSIGN(PinnedTabCodec);
};

#endif  // CHROME_BROWSER_UI_TABS_PINNED_TAB_CODEC_H_
