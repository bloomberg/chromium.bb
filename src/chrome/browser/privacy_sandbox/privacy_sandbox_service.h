// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_SERVICE_H_
#define CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_SERVICE_H_

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/policy/core/common/policy_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/privacy_sandbox/canonical_topic.h"
#include "components/privacy_sandbox/privacy_sandbox_settings.h"
#include "components/profile_metrics/browser_profile_type.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_service_observer.h"

class PrefService;

namespace content {
class InterestGroupManager;
}

namespace content_settings {
class CookieSettings;
}

// Service which encapsulates logic related to displaying and controlling the
// users Privacy Sandbox settings. This service contains the chrome/ specific
// logic used by the UI, including decision making around what the users'
// Privacy Sandbox settings should be based on their existing settings.
// Ultimately the decisions made by this service are consumed (through
// preferences and content settings) by the PrivacySandboxSettings located in
// components/privacy_sandbox/, which in turn makes them available to Privacy
// Sandbox APIs.
class PrivacySandboxService : public KeyedService,
                              public policy::PolicyService::Observer,
                              public syncer::SyncServiceObserver,
                              public signin::IdentityManager::Observer {
 public:
  // Possible types of Privacy Sandbox dialogs that may be shown to the user.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.privacy_sandbox
  enum class DialogType {
    kNone = 0,
    kNotice = 1,
    kConsent = 2,
    kMaxValue = kConsent,
  };

  // An exhaustive list of actions related to showing & interacting with the
  // dialog. Includes actions which do not impact consent / notice state.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.privacy_sandbox
  enum class DialogAction {
    // Notice Interactions:
    kNoticeShown = 0,
    kNoticeOpenSettings = 1,
    kNoticeAcknowledge = 2,
    kNoticeDismiss = 3,

    // Implies that the browser, or browser window, was shut before the user
    // interacted with the notice.
    kNoticeClosedNoInteraction = 4,

    // Consent Interactions:
    kConsentShown = 5,
    kConsentAccepted = 6,
    kConsentDeclined = 7,
    kConsentMoreInfoOpened = 8,
    kConsentMoreInfoClosed = 9,

    // Implies that the browser, or browser window, was shut before the user
    // has made the decision (accepted or declined the consent).
    kConsentClosedNoDecision = 10,

    kMaxValue = kConsentClosedNoDecision,
  };

  PrivacySandboxService(PrivacySandboxSettings* privacy_sandbox_settings,
                        content_settings::CookieSettings* cookie_settings,
                        PrefService* pref_service,
                        policy::PolicyService* policy_service,
                        syncer::SyncService* sync_service,
                        signin::IdentityManager* identity_manager,
                        content::InterestGroupManager* interest_group_manager,
                        profile_metrics::BrowserProfileType profile_type);
  ~PrivacySandboxService() override;

  // Returns the dialog type that should be shown to the user. This consults
  // previous consent / notice information stored in preferences, the current
  // state of the Privacy Sandbox settings, and the current location of the
  // user, to determine the appropriate type. This is expected to be called by
  // UI code locations determining whether a dialog should be shown on startup.
  DialogType GetRequiredDialogType();

  // Informs the service that |action| occurred with the dialog. This allows
  // the service to record this information in preferences such that future
  // calls to GetRequiredDialogType() are correct. This is expected to be
  // called appropriately by all locations showing the dialog. Metrics shared
  // between platforms will also be recorded.
  // This method is virtual for mocking in tests.
  virtual void DialogActionOccurred(DialogAction action);

  // Returns a description of FLoC ready for display to the user. Correctly
  // takes into account the FLoC feature parameters when determining the number
  // of days between cohort calculations.
  std::u16string GetFlocDescriptionForDisplay() const;

  // Returns the current FLoC cohort identifier for the associated profile in
  // string format suitable for direct display to the user. If the cohort is
  // not valid, the appropriate descriptive string is returned instead.
  std::u16string GetFlocIdForDisplay() const;

  // Returns when the user's current FLoC cohort identifier will next be updated
  // in a string format suitable for direct display to the user. If no compute
  // is scheduled, the appropriate descriptive string is returned instead.
  std::u16string GetFlocIdNextUpdateForDisplay(const base::Time& current_time);

  // Returns the display ready string explaining what happens when the user
  // resets the FLoC cohort identifier.
  std::u16string GetFlocResetExplanationForDisplay() const;

  // Returns a display ready string explaining the current status of FloC. E.g.
  // the effective state of the Finch experiment, and the user's setting.
  std::u16string GetFlocStatusForDisplay() const;

  // Returns whether the user's current FLoC ID can be reset. This requires that
  // the FLoC feature be enabled and FLoC be enabled in preferences. It does not
  // require that the current ID is valid, as resetting the ID also resets the
  // compute timer, it should be available whenever FLoC is active.
  bool IsFlocIdResettable() const;

  // Sets the time when history is accessible for FLoC calculation to the
  // current time and resets the time to the next FLoC id calculation. If
  // |user_initiated| is true, records the associated User Metrics Action.
  void ResetFlocId(bool user_initiated) const;

  // Returns whether the FLoC preference is enabled. This should only be used
  // for displaying the preference state to the user, and should *not* be used
  // for determining whether FLoC is allowed or not.
  bool IsFlocPrefEnabled() const;

  // Sets the FLoC preference to |enabled|.
  void SetFlocPrefEnabled(bool enabled) const;

  // Disables the Privacy Sandbox completely if |enabled| is false, if |enabled|
  // is true, more granular checks will still be performed to determine if
  // specific APIs are available in specific contexts.
  void SetPrivacySandboxEnabled(bool enabled);

  // Used by the UI to check if the API is enabled. Checks the primary
  // pref directly.
  bool IsPrivacySandboxEnabled();

  // Returns whether the state of the API is managed.
  bool IsPrivacySandboxManaged();

  // Called when a preference relevant to the the Privacy Sandbox is changed.
  void OnPrivacySandboxPrefChanged();

  // Returns the set of eTLD + 1's on which the user was joined to a FLEDGE
  // interest group. Consults with the InterestGroupManager associated with
  // |profile_| and formats the returned data for direct display to the user.
  // Virtual to allow mocking in tests.
  virtual void GetFledgeJoiningEtldPlusOneForDisplay(
      base::OnceCallback<void(std::vector<std::string>)> callback);

  // Returns the set of top frames which are blocked from joining the profile to
  // an interest group. Virtual to allow mocking in tests.
  virtual std::vector<std::string> GetBlockedFledgeJoiningTopFramesForDisplay()
      const;

  // Sets Fledge interest group joining to |allowed| for |top_frame_etld_plus1|.
  // This is a wrapper on the equivalent function on PrivacySandboxSettings.
  // Virtual to allow mocking in tests.
  virtual void SetFledgeJoiningAllowed(const std::string& top_frame_etld_plus1,
                                       bool allowed) const;

  // Returns the top topics for the previous N epochs.
  // Virtual for mocking in tests.
  virtual std::vector<privacy_sandbox::CanonicalTopic> GetCurrentTopTopics()
      const;

  // Returns the set of topics which have been blocked by the user.
  // Virtual for mocking in tests.
  virtual std::vector<privacy_sandbox::CanonicalTopic> GetBlockedTopics() const;

  // Sets a |topic_id|, as both a top topic and topic provided to the web, to be
  // allowed/blocked based on the value of |allowed|. This is stored to
  // preferences and made available to the Topics API via the
  // PrivacySandboxSettings class. This function expects that |topic| will have
  // previously been provided by one of the above functions. Virtual for mocking
  // in tests.
  virtual void SetTopicAllowed(privacy_sandbox::CanonicalTopic topic,
                               bool allowed);

  // KeyedService:
  void Shutdown() override;

  // policy::PolicyService::Observer:
  void OnPolicyUpdated(const policy::PolicyNamespace& ns,
                       const policy::PolicyMap& previous,
                       const policy::PolicyMap& current) override;

  // syncer::SyncServiceObserver:
  void OnStateChanged(syncer::SyncService* sync) override;
  void OnSyncCycleCompleted(syncer::SyncService* sync) override;

  // signin::IdentityManager::Observer:
  // TODO(crbug.com/1167680): This is only required to capture failure scenarios
  // that affect sync, yet aren't reported via SyncServiceObserver.
  void OnErrorStateOfRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info,
      const GoogleServiceAuthError& error) override;

 protected:
  friend class PrivacySandboxServiceTest;
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServiceTestReconciliationBlocked,
                           ReconciliationOutcome);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServiceTestReconciliationBlocked,
                           ImmediateReconciliationNoSync);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServiceTestReconciliationBlocked,
                           ImmediateReconciliationSyncComplete);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServiceTestReconciliationBlocked,
                           ImmediateReconciliationPersistentSyncError);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServiceTestReconciliationBlocked,
                           ImmediateReconciliationNoDisable);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServiceTestReconciliationBlocked,
                           DelayedReconciliationSyncSuccess);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServiceTestReconciliationBlocked,
                           DelayedReconciliationSyncFailure);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServiceTestReconciliationBlocked,
                           DelayedReconciliationIdentityFailure);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServiceTestReconciliationBlocked,
                           DelayedReconciliationSyncIssueThenManaged);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServiceTestReconciliationBlocked,
                           NoReconciliationAlreadyRun);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServiceTestReconciliationBlocked,
                           NoReconciliationSandboxSettingsDisabled);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServiceTestReconciliationBlocked,
                           MetricsLoggingOccursCorrectly);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServiceTestNonRegularProfile,
                           NoMetricsRecorded);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServiceDeathTest,
                           GetRequiredDialogType);

  // Should be used only for tests when mocking the service.
  PrivacySandboxService();

  // Contains all possible privacy sandbox states, recorded on startup.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // Must be kept in sync with the SettingsPrivacySandboxEnabled enum in
  // histograms/enums.xml.
  enum class SettingsPrivacySandboxEnabled {
    kPSEnabledAllowAll = 0,
    kPSEnabledBlock3P = 1,
    kPSEnabledBlockAll = 2,
    kPSDisabledAllowAll = 3,
    kPSDisabledBlock3P = 4,
    kPSDisabledBlockAll = 5,
    kPSDisabledPolicyBlock3P = 6,
    kPSDisabledPolicyBlockAll = 7,
    kPSEnabledFlocDisabledAllowAll = 8,
    kPSEnabledFlocDisabledBlock3P = 9,
    kPSEnabledFlocDisabledBlockAll = 10,
    // Add values above this line with a corresponding label in
    // tools/metrics/histograms/enums.xml
    kMaxValue = kPSEnabledFlocDisabledBlockAll,
  };

  // Inspects the current sync state and settings to determine if the Privacy
  // Sandbox prefs should be reconciled. Calls ReconcilePrivacySandbox()
  // immediately if appropriate, or may register sync and identity observers to
  // call ReconcilePrivacySandbox() later as appropriate.
  void MaybeReconcilePrivacySandboxPref();

  // Selectively disable the Privacy Sandbox preference based on the local and
  // synced state. Reconcilliation is only performed once per synced profile.
  // As the sandbox is default enabled, reconcilliation will only ever opt a
  // user out of the sandbox.
  void ReconcilePrivacySandboxPref();

  // Stops any observation of services being performed by this class.
  void StopObserving();

  // Helper function to actually make the metrics call for
  // LogPrivacySandboxState.
  void RecordPrivacySandboxHistogram(SettingsPrivacySandboxEnabled state);

  // Logs the state of the privacy sandbox and cookie settings. Called once per
  // profile startup.
  void LogPrivacySandboxState();

  // Converts the provided list of |top_frames| into eTLD+1s for display, and
  // provides those to |callback|.
  void ConvertFledgeJoiningTopFramesForDisplay(
      base::OnceCallback<void(std::vector<std::string>)> callback,
      std::vector<url::Origin> top_frames);

  // Contains the logic which powers GetRequiredDialogType(). Static to allow
  // EXPECT_DCHECK_DEATH testing, which does not work well with many of the
  // other dependencies of this service.
  static PrivacySandboxService::DialogType GetRequiredDialogTypeInternal(
      PrefService* pref_service,
      profile_metrics::BrowserProfileType profile_type);

 private:
  raw_ptr<PrivacySandboxSettings> privacy_sandbox_settings_;
  raw_ptr<content_settings::CookieSettings> cookie_settings_;
  raw_ptr<PrefService> pref_service_;
  raw_ptr<policy::PolicyService> policy_service_;
  raw_ptr<syncer::SyncService> sync_service_;
  raw_ptr<signin::IdentityManager> identity_manager_;
  raw_ptr<content::InterestGroupManager> interest_group_manager_;
  profile_metrics::BrowserProfileType profile_type_;

  base::ScopedObservation<syncer::SyncService, syncer::SyncServiceObserver>
      sync_service_observer_{this};
  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observer_{this};

  PrefChangeRegistrar user_prefs_registrar_;

  // A manual record of whether policy_service_ is being observerd.
  // Unfortunately PolicyService does not support scoped observers.
  bool policy_service_observed_ = false;

  // Fake implementation for current and blocked topics.
  std::set<privacy_sandbox::CanonicalTopic> fake_current_topics_ = {
      {1, privacy_sandbox::CanonicalTopic::AVAILABLE_TAXONOMY},
      {2, privacy_sandbox::CanonicalTopic::AVAILABLE_TAXONOMY}};
  std::set<privacy_sandbox::CanonicalTopic> fake_blocked_topics_ = {
      {3, privacy_sandbox::CanonicalTopic::AVAILABLE_TAXONOMY},
      {4, privacy_sandbox::CanonicalTopic::AVAILABLE_TAXONOMY}};

  base::WeakPtrFactory<PrivacySandboxService> weak_factory_{this};
};

#endif  // CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_SERVICE_H_
