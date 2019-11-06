// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_OPTIN_ARC_OPTIN_PREFERENCE_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_ARC_OPTIN_ARC_OPTIN_PREFERENCE_HANDLER_H_

#include <memory>

#include "base/macros.h"

#include "chrome/browser/chromeos/settings/stats_reporting_controller.h"
#include "components/prefs/pref_change_registrar.h"

class PrefService;

namespace arc {

class ArcOptInPreferenceHandlerObserver;

// This helper encapsulates access to preferences and metrics mode, used in
// OptIn flow. It provides setters for metrics mode and preferences. It also
// observes changes there. Changes in preferences and metrics mode are passed to
// external consumer via ArcOptInPreferenceHandlerObserver. Once started it
// immediately sends current state of metrics mode and preferences.
//
// Note that the preferences and metrics mode passed by this class should only
// be used for the OptIn flow, as this class overrides some of the defaults in
// order to encourage users to consent with the settings.
class ArcOptInPreferenceHandler {
 public:
  ArcOptInPreferenceHandler(ArcOptInPreferenceHandlerObserver* observer,
                            PrefService* pref_serive);
  ~ArcOptInPreferenceHandler();

  void Start();

  void EnableMetrics(bool is_enabled);
  void EnableBackupRestore(bool is_enabled);
  void EnableLocationService(bool is_enabled);

 private:
  void OnMetricsPreferenceChanged();
  void OnBackupAndRestorePreferenceChanged();
  void OnLocationServicePreferenceChanged();

  // Utilities on preference update.
  void SendMetricsMode();
  void SendBackupAndRestoreMode();
  void SendLocationServicesMode();

  // Unowned pointers.
  ArcOptInPreferenceHandlerObserver* const observer_;
  PrefService* const pref_service_;

  // Used to track metrics preference.
  PrefChangeRegistrar pref_local_change_registrar_;
  // Used to track backup&restore and location service preference.
  PrefChangeRegistrar pref_change_registrar_;
  // Metrics consent observer.
  std::unique_ptr<chromeos::StatsReportingController::ObserverSubscription>
      reporting_consent_subscription_;

  DISALLOW_COPY_AND_ASSIGN(ArcOptInPreferenceHandler);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_OPTIN_ARC_OPTIN_PREFERENCE_HANDLER_H_
