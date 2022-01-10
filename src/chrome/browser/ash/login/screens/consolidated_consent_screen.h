// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_CONSOLIDATED_CONSENT_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_CONSOLIDATED_CONSENT_SCREEN_H_

#include "base/observer_list.h"
#include "chrome/browser/ash/arc/optin/arc_optin_preference_handler_observer.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ui/webui/chromeos/login/consolidated_consent_screen_handler.h"

namespace arc {
class ArcOptInPreferenceHandler;
}

namespace ash {

// Controller for the consolidated consent screen.
class ConsolidatedConsentScreen
    : public BaseScreen,
      public arc::ArcOptInPreferenceHandlerObserver {
 public:
  enum class Result {
    // The user accepted terms of service in the regular flow.
    ACCEPTED,

    // The user clicked the back button in the demo mode.
    BACK_DEMO,

    // The user accepted terms of service in online demo mode.
    ACCEPTED_DEMO_ONLINE,

    // The user accepted terms of service in offline demo mode.
    ACCEPTED_DEMO_OFFLINE,

    // Consolidated Consent screen skipped.
    NOT_APPLICABLE,
  };

  class Observer : public base::CheckedObserver {
   public:
    // Called when the user accepts terms of service.
    virtual void OnConsolidatedConsentAccept() = 0;
    virtual void OnConsolidatedConsentScreenDestroyed() = 0;
  };

  using TView = ConsolidatedConsentScreenView;
  using ScreenExitCallback = base::RepeatingCallback<void(Result result)>;

  ConsolidatedConsentScreen(ConsolidatedConsentScreenView* view,
                            const ScreenExitCallback& exit_callback);
  ~ConsolidatedConsentScreen() override;
  ConsolidatedConsentScreen(const ConsolidatedConsentScreen&) = delete;
  ConsolidatedConsentScreen& operator=(const ConsolidatedConsentScreen&) =
      delete;

  static std::string GetResultString(Result result);

  // Called when the screen is being destroyed. This should call Unbind() on the
  // associated View if this class is destroyed before that.
  void OnViewDestroyed(ConsolidatedConsentScreenView* view);

  void set_exit_callback_for_testing(const ScreenExitCallback& exit_callback) {
    exit_callback_ = exit_callback;
  }

  const ScreenExitCallback& get_exit_callback_for_testing() {
    return exit_callback_;
  }

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  void OnAccept(bool enable_stats_usage,
                bool enable_backup_restore,
                bool enable_location_services,
                const std::string& tos_content);

  // arc::ArcOptInPreferenceHandlerObserver:
  void OnMetricsModeChanged(bool enabled, bool managed) override;
  void OnBackupAndRestoreModeChanged(bool enabled, bool managed) override;
  void OnLocationServicesModeChanged(bool enabled, bool managed) override;

 private:
  struct ConsentsParameters {
    std::string tos_content;
    bool record_arc_tos_consent;
    bool record_backup_consent;
    bool backup_accepted;
    bool record_location_consent;
    bool location_accepted;
  };

  // BaseScreen:
  bool MaybeSkip(WizardContext* context) override;
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const std::string& action_id) override;

  void RecordConsents(const ConsentsParameters& params);

  // Exits the screen with `Result::ACCEPTED` in the normal flow, and
  // `Result::ACCEPTED_DEMO_ONLINE` or `Result::ACCEPTED_DEMO_OFFLINE` in the
  // demo setup flow.
  void ExitScreenWithAcceptedResult();

  bool is_child_account_ = false;

  bool is_enterprise_managed_account_ = false;

  // To track if optional ARC features are managed preferences.
  bool backup_restore_managed_ = false;
  bool location_services_managed_ = false;

  base::ObserverList<Observer, true> observer_list_;

  std::unique_ptr<arc::ArcOptInPreferenceHandler> pref_handler_;

  ConsolidatedConsentScreenView* view_ = nullptr;

  ScreenExitCallback exit_callback_;
};

}  // namespace ash

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace chromeos {
using ::ash ::ConsolidatedConsentScreen;
}

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_CONSOLIDATED_CONSENT_SCREEN_H_
