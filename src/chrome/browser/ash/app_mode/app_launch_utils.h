// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_APP_LAUNCH_UTILS_H_
#define CHROME_BROWSER_ASH_APP_MODE_APP_LAUNCH_UTILS_H_

#include <string>

class PrefService;
class Profile;

namespace base {
class CommandLine;
}

namespace ash {

class KioskAppId;

// Attempts to launch the app given by |kiosk_app_id| in app mode
// or exit on failure. This function will not show any launch UI
// during the launch. Use StartupAppLauncher for finer control
// over the app launch processes.
void LaunchAppOrDie(Profile* profile, const KioskAppId& kiosk_app_id);

// Removes obsolete preferences left out by previous user session;
void ResetEphemeralKioskPreferences(PrefService* prefs);
// Replace the list of preferences which are reset in tests.
void SetEphemeralKioskPreferencesListForTesting(std::vector<std::string>*);

// Checks whether kiosk auto launch should be started.
bool ShouldAutoLaunchKioskApp(const base::CommandLine& command_line,
                              PrefService* local_state);

}  // namespace ash

// TODO(https://crbug.com/1164001): remove when the migration of
// //chrome/browser/ash is finished.
namespace chromeos {
using ::ash::LaunchAppOrDie;
}

#endif  // CHROME_BROWSER_ASH_APP_MODE_APP_LAUNCH_UTILS_H_
