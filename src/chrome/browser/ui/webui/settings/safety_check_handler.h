// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_SAFETY_CHECK_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_SAFETY_CHECK_HANDLER_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "base/util/type_safety/strong_alias.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_delegate.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/ui/webui/help/version_updater.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "components/password_manager/core/browser/bulk_leak_check_service.h"
#include "components/safety_check/safety_check.h"
#include "components/safety_check/update_check_helper.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"

// Settings page UI handler that checks four areas of browser safety:
// browser updates, password leaks, malicious extensions, and unwanted
// software.
class SafetyCheckHandler
    : public settings::SettingsPageUIHandler,
      public password_manager::BulkLeakCheckService::Observer,
      public safety_check::SafetyCheck::SafetyCheckHandlerInterface {
 public:
  // The following enum represent the state of the safety check parent
  // component and  should be kept in sync with the JS frontend
  // (safety_check_browser_proxy.js).
  enum class ParentStatus {
    kBefore = 0,
    kChecking = 1,
    kAfter = 2,
  };

  // The following enums represent the state of each component of the safety
  // check and should be kept in sync with the JS frontend
  // (safety_check_browser_proxy.js) and |SafetyCheck*| metrics enums in
  // enums.xml.
  using SafeBrowsingStatus = safety_check::SafetyCheck::SafeBrowsingStatus;
  enum class UpdateStatus {
    kChecking = 0,
    kUpdated = 1,
    kUpdating = 2,
    kRelaunch = 3,
    kDisabledByAdmin = 4,
    kFailedOffline = 5,
    kFailed = 6,
    // Non-Google branded browsers cannot check for updates using
    // VersionUpdater.
    kUnknown = 7,
    // New enum values must go above here.
    kMaxValue = kUnknown,
  };
  enum class PasswordsStatus {
    kChecking = 0,
    kSafe = 1,
    kCompromisedExist = 2,
    kOffline = 3,
    kNoPasswords = 4,
    kSignedOut = 5,
    kQuotaLimit = 6,
    kError = 7,
    // New enum values must go above here.
    kMaxValue = kError,
  };
  enum class ExtensionsStatus {
    kChecking = 0,
    kError = 1,
    kNoneBlocklisted = 2,
    kBlocklistedAllDisabled = 3,
    kBlocklistedReenabledAllByUser = 4,
    // In this case, at least one of the extensions was re-enabled by admin.
    kBlocklistedReenabledSomeByUser = 5,
    kBlocklistedReenabledAllByAdmin = 6,
    // New enum values must go above here.
    kMaxValue = kBlocklistedReenabledAllByAdmin,
  };

  SafetyCheckHandler();
  ~SafetyCheckHandler() override;

  // Triggers WebUI updates about safety check now running.
  // Note: since the checks deal with sensitive user information, this method
  // should only be called as a result of an explicit user action.
  void SendSafetyCheckStartedWebUiUpdates();

  // Triggers all safety check child checks.
  // Note: since the checks deal with sensitive user information, this method
  // should only be called as a result of an explicit user action.
  void PerformSafetyCheck();

  // Constructs the 'safety check ran' display string by how long ago safety
  // check ran.
  base::string16 GetStringForParentRan(base::Time safety_check_completion_time);
  base::string16 GetStringForParentRan(base::Time safety_check_completion_time,
                                       base::Time system_time);

 protected:
  SafetyCheckHandler(
      std::unique_ptr<safety_check::UpdateCheckHelper> update_helper,
      std::unique_ptr<VersionUpdater> version_updater,
      password_manager::BulkLeakCheckService* leak_service,
      extensions::PasswordsPrivateDelegate* passwords_delegate,
      extensions::ExtensionPrefs* extension_prefs,
      extensions::ExtensionServiceInterface* extension_service);

  void SetVersionUpdaterForTesting(
      std::unique_ptr<VersionUpdater> version_updater) {
    version_updater_ = std::move(version_updater);
  }

 private:
  // These ensure integers are passed in the correct possitions in the extension
  // check methods.
  using Compromised = util::StrongAlias<class CompromisedTag, int>;
  using Done = util::StrongAlias<class DoneTag, int>;
  using Total = util::StrongAlias<class TotalTag, int>;
  using Blocklisted = util::StrongAlias<class BlocklistedTag, int>;
  using ReenabledUser = util::StrongAlias<class ReenabledUserTag, int>;
  using ReenabledAdmin = util::StrongAlias<class ReenabledAdminTag, int>;

  // Handles triggering the safety check from the frontend (by user pressing a
  // button).
  void HandlePerformSafetyCheck(const base::ListValue* args);

  // Handles updating the safety check parent display string to show how long
  // ago the safety check last ran.
  void HandleGetParentRanDisplayString(const base::ListValue* args);

  // Triggers an update check and invokes OnUpdateCheckResult once results
  // are available.
  void CheckUpdates();

  // Triggers a bulk password leak check and invokes OnPasswordsCheckResult once
  // results are available.
  void CheckPasswords();

  // Checks if any of the installed extensions are blocklisted, and in
  // that case, if any of those were re-enabled.
  void CheckExtensions();

  // Callbacks that get triggered when each check completes.
  void OnUpdateCheckResult(UpdateStatus status);
  void OnPasswordsCheckResult(PasswordsStatus status,
                              Compromised compromised,
                              Done done,
                              Total total);
  void OnExtensionsCheckResult(ExtensionsStatus status,
                               Blocklisted blocklisted,
                               ReenabledUser reenabled_user,
                               ReenabledAdmin reenabled_admin);

  // Methods for building user-visible strings based on the safety check
  // state.
  base::string16 GetStringForParent(ParentStatus status);
  base::string16 GetStringForUpdates(UpdateStatus status);
  base::string16 GetStringForSafeBrowsing(SafeBrowsingStatus status);
  base::string16 GetStringForPasswords(PasswordsStatus status,
                                       Compromised compromised,
                                       Done done,
                                       Total total);
  base::string16 GetStringForExtensions(ExtensionsStatus status,
                                        Blocklisted blocklisted,
                                        ReenabledUser reenabled_user,
                                        ReenabledAdmin reenabled_admin);

  // A generic error state often includes the offline state. This method is used
  // as a callback for |UpdateCheckHelper| to check connectivity.
  void DetermineIfOfflineOrError(bool connected);

  // Since the password check API does not distinguish between the cases of
  // having no compromised passwords and not having any passwords at all, it is
  // necessary to use this method as a callback for
  // |PasswordsPrivateDelegate::GetSavedPasswordsList| to distinguish the two
  // states here.
  void DetermineIfNoPasswordsOrSafe(
      const std::vector<extensions::api::passwords_private::PasswordUiEntry>&
          passwords);

  // A callback passed to |VersionUpdater::CheckForUpdate| to receive the update
  // state.
  void OnVersionUpdaterResult(VersionUpdater::Status status,
                              int progress,
                              bool rollback,
                              const std::string& version,
                              int64_t update_size,
                              const base::string16& message);

  // SafetyCheck::SafetyCheckHandlerInterface implementation.
  void OnSafeBrowsingCheckResult(SafeBrowsingStatus status) override;

  // BulkLeakCheckService::Observer implementation.
  void OnStateChanged(
      password_manager::BulkLeakCheckService::State state) override;
  void OnCredentialDone(const password_manager::LeakCheckCredential& credential,
                        password_manager::IsLeaked is_leaked) override;

  // SettingsPageUIHandler implementation.
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;

  // Updates the parent status from the children statuses.
  void CompleteParentIfChildrenCompleted();

  // Fire a safety check element WebUI update with a state and string.
  void FireBasicSafetyCheckWebUiListener(const std::string& event_name,
                                         int new_state,
                                         const base::string16& display_string);

  // The current status of the safety check elements. Before safety
  // check is started, the parent is in the 'before' state.
  ParentStatus parent_status_ = ParentStatus::kBefore;
  UpdateStatus update_status_ = UpdateStatus::kChecking;
  PasswordsStatus passwords_status_ = PasswordsStatus::kChecking;
  SafeBrowsingStatus safe_browsing_status_ = SafeBrowsingStatus::kChecking;
  ExtensionsStatus extensions_status_ = ExtensionsStatus::kChecking;

  // System time when safety check completed.
  base::Time safety_check_completion_time_;

  std::unique_ptr<safety_check::SafetyCheck> safety_check_;
  std::unique_ptr<safety_check::UpdateCheckHelper> update_helper_;

  std::unique_ptr<VersionUpdater> version_updater_;
  password_manager::BulkLeakCheckService* leak_service_ = nullptr;
  extensions::PasswordsPrivateDelegate* passwords_delegate_ = nullptr;
  extensions::ExtensionPrefs* extension_prefs_ = nullptr;
  extensions::ExtensionServiceInterface* extension_service_ = nullptr;
  ScopedObserver<password_manager::BulkLeakCheckService,
                 password_manager::BulkLeakCheckService::Observer>
      observed_leak_check_{this};
  base::WeakPtrFactory<SafetyCheckHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_SAFETY_CHECK_HANDLER_H_
