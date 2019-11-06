// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_LOCAL_NTP_FIRST_RUN_FIELD_TRIAL_HANDLER_H_
#define CHROME_BROWSER_SEARCH_LOCAL_NTP_FIRST_RUN_FIELD_TRIAL_HANDLER_H_

class PrefRegistrySimple;
class PrefService;

namespace base {
class FeatureList;
}

namespace ntp_first_run {

// Registers hide shortcuts field trial activation in Local State.
void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

// Loads the HideShortcutsOnNtp field trial config, and establishes the chosen
// group for this client as being part of the experiment, control or default.
void ActivateHideShortcutsOnNtpFieldTrial(base::FeatureList* feature_list,
                                          PrefService* local_state);

}  // namespace ntp_first_run

#endif  // CHROME_BROWSER_SEARCH_LOCAL_NTP_FIRST_RUN_FIELD_TRIAL_HANDLER_H_
