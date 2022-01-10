// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_MARKETING_OPT_IN_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_MARKETING_OPT_IN_SCREEN_H_

#include <memory>
#include <unordered_set>
#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/flat_set.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
// TODO(https://crbug.com/1164001): move to forward declaration.
#include "chrome/browser/ui/webui/chromeos/login/marketing_opt_in_screen_handler.h"
#include "components/prefs/pref_change_registrar.h"

namespace ash {

// This is Sync settings screen that is displayed as a part of user first
// sign-in flow.
class MarketingOptInScreen : public BaseScreen {
 public:
  using TView = MarketingOptInScreenView;

  enum class Result { NEXT, NOT_APPLICABLE };

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused. Must coincide with the enum
  // MarketingOptInScreenEvent
  enum class Event {
    kUserOptedInWhenDefaultIsOptIn = 0,
    kUserOptedInWhenDefaultIsOptOut = 1,
    kUserOptedOutWhenDefaultIsOptIn = 2,
    kUserOptedOutWhenDefaultIsOptOut = 3,
    kMaxValue = kUserOptedOutWhenDefaultIsOptOut,
  };

  // Whether the geolocation resolve was successful.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused. Must coincide with the enum
  // MarketingOptInScreenEvent
  enum class GeolocationEvent {
    kCouldNotDetermineCountry = 0,
    kCountrySuccessfullyDetermined = 1,
    kMaxValue = kCountrySuccessfullyDetermined,
  };

  static std::string GetResultString(Result result);

  using ScreenExitCallback = base::RepeatingCallback<void(Result result)>;

  MarketingOptInScreen(MarketingOptInScreenView* view,
                       const ScreenExitCallback& exit_callback);

  MarketingOptInScreen(const MarketingOptInScreen&) = delete;
  MarketingOptInScreen& operator=(const MarketingOptInScreen&) = delete;

  ~MarketingOptInScreen() override;

  // On "Get Started" button pressed.
  void OnGetStarted(bool chromebook_email_opt_in);

  void SetA11yButtonVisibilityForTest(bool shown);

  void set_exit_callback_for_testing(const ScreenExitCallback& exit_callback) {
    exit_callback_ = exit_callback;
  }

  void set_ingore_pref_sync_for_testing(bool ignore_sync) {
    ignore_pref_sync_for_testing_ = ignore_sync;
  }

  const ScreenExitCallback& get_exit_callback_for_testing() {
    return exit_callback_;
  }

 protected:
  // BaseScreen:
  bool MaybeSkip(WizardContext* context) override;
  void ShowImpl() override;
  void HideImpl() override;

 private:
  void OnA11yShelfNavigationButtonPrefChanged();

  // Checks whether this user is managed.
  bool IsCurrentUserManaged();

  // Initializes the screen and determines if it should be visible based on the
  // country.
  void Initialize();

  // Sets the country to be used if the feature is available in this region.
  void SetCountryFromTimezoneIfAvailable(const std::string& timezone_id);

  // Whether the screen should be shown depending if it was shown before and if
  // the user had the option to subscribe to emails.
  bool ShouldShowOptionToSubscribe();

  bool IsDefaultOptInCountry() {
    return default_opt_in_countries_.count(country_);
  }

  MarketingOptInScreenView* const view_;
  ScreenExitCallback exit_callback_;
  std::unique_ptr<PrefChangeRegistrar> active_user_pref_change_registrar_;

  // Whether the email opt-in toggle is visible.
  bool email_opt_in_visible_ = false;

  // Country code. Unknown IFF empty.
  std::string country_;

  // Whether the screen has been initialized. Determines the country based on
  // the available geolocation information and whether the screen will support
  // showing the toggle.
  bool initialized_ = false;

  // Set by tests so that the sync status of preferences does not interfere when
  // showing the opt-in option.
  bool ignore_pref_sync_for_testing_ = false;

  // Default country list.
  const base::flat_set<base::StringPiece> default_countries_{"us", "ca", "gb"};

  // Extended country list. Protected behind the flag:
  // - kOobeMarketingAdditionalCountriesSupported (DEFAULT_ON)
  const base::flat_set<base::StringPiece> additional_countries_{
      "fr", "nl", "fi", "se", "no", "dk", "es", "it", "jp", "au"};

  // Countries with double opt-in.  Behind the flag:
  // - kOobeMarketingDoubleOptInCountriesSupported (DEFAULT_OFF)
  const base::flat_set<base::StringPiece> double_opt_in_countries_{"de"};

  // Countries in which the toggle will be enabled by default.
  const base::flat_set<base::StringPiece> default_opt_in_countries_{"us"};

  // Countries that require the screen to show a footer with legal information.
  const base::flat_set<base::StringPiece> countries_with_legal_footer{"ca"};

  base::WeakPtrFactory<MarketingOptInScreen> weak_factory_{this};
};

}  // namespace ash

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace chromeos {
using ::ash::MarketingOptInScreen;
}

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_MARKETING_OPT_IN_SCREEN_H_
