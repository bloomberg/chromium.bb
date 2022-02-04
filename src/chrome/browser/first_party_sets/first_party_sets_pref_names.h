// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FIRST_PARTY_SETS_FIRST_PARTY_SETS_PREF_NAMES_H_
#define CHROME_BROWSER_FIRST_PARTY_SETS_FIRST_PARTY_SETS_PREF_NAMES_H_

class PrefRegistrySimple;

namespace first_party_sets {

// Add Local State prefs below.
extern const char kFirstPartySetsEnabled[];

void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

}  // namespace first_party_sets

#endif  // CHROME_BROWSER_FIRST_PARTY_SETS_FIRST_PARTY_SETS_PREF_NAMES_H_
