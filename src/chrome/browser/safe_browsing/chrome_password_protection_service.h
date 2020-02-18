// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CHROME_PASSWORD_PROTECTION_SERVICE_H_
#define CHROME_BROWSER_SAFE_BROWSING_CHROME_PASSWORD_PROTECTION_SERVICE_H_

#include <map>

#include "base/callback.h"
#include "base/callback_list.h"
#include "base/observer_list.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/security_events/security_event_recorder.h"
#include "chrome/browser/security_events/security_event_recorder_factory.h"
#include "components/password_manager/core/browser/hash_password_manager.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/password_protection/password_protection_service.h"
#include "components/safe_browsing/triggers/trigger_manager.h"
#include "components/sessions/core/session_id.h"
#include "components/sync/protocol/gaia_password_reuse.pb.h"
#include "components/sync/protocol/user_event_specifics.pb.h"
#include "ui/base/buildflags.h"
#include "url/origin.h"

struct AccountInfo;
class PrefChangeRegistrar;
class PrefService;
class PrefChangeRegistrar;
class Profile;

namespace content {
class WebContents;
}

namespace policy {
class BrowserPolicyConnector;
}

namespace safe_browsing {

class SafeBrowsingService;
class SafeBrowsingNavigationObserverManager;
class SafeBrowsingUIManager;
class ChromePasswordProtectionService;
class VerdictCacheManager;

using OnWarningDone = base::OnceCallback<void(WarningAction)>;
using StringProvider = base::RepeatingCallback<std::string()>;
using password_manager::metrics_util::PasswordType;
using url::Origin;

#if defined(SYNC_PASSWORD_REUSE_DETECTION_ENABLED)
// Shows the platform-specific password reuse modal dialog.
void ShowPasswordReuseModalWarningDialog(
    content::WebContents* web_contents,
    ChromePasswordProtectionService* service,
    ReusedPasswordAccountType password_type,
    OnWarningDone done_callback);
#endif

// Called by ChromeContentBrowserClient to create a
// PasswordProtectionNavigationThrottle if appropriate.
std::unique_ptr<PasswordProtectionNavigationThrottle>
MaybeCreateNavigationThrottle(content::NavigationHandle* navigation_handle);

// TODO(xinghuilu@): Rearrange this file to reduce number of #ifs
// ChromePasswordProtectionService extends PasswordProtectionService by adding
// access to SafeBrowsingNaivigationObserverManager and Profile.
class ChromePasswordProtectionService : public PasswordProtectionService {
 public:
  // Observer is used to coordinate password protection UIs (e.g. modal warning,
  // change password card, etc) in reaction to user events.
  class Observer {
   public:
    // Called when user completes the GAIA password reset.
    virtual void OnGaiaPasswordChanged() = 0;

    // Called when user marks the site as legitimate.
    virtual void OnMarkingSiteAsLegitimate(const GURL& url) = 0;

    // Only to be used by tests. Subclasses must override to manually call the
    // respective button click handler.
    virtual void InvokeActionForTesting(WarningAction action) = 0;

    // Only to be used by tests.
    virtual WarningUIType GetObserverType() = 0;

   protected:
    virtual ~Observer() = default;
  };

  ChromePasswordProtectionService(SafeBrowsingService* sb_service,
                                  Profile* profile);
  ~ChromePasswordProtectionService() override;

  static ChromePasswordProtectionService* GetPasswordProtectionService(
      Profile* profile);

#if defined(SYNC_PASSWORD_REUSE_DETECTION_ENABLED)
  static bool ShouldShowChangePasswordSettingUI(Profile* profile);

  // Called by SecurityStateTabHelper to determine if page info bubble should
  // show password reuse warning.
  static bool ShouldShowPasswordReusePageInfoBubble(
      content::WebContents* web_contents,
      PasswordType password_type);

  void ShowModalWarning(content::WebContents* web_contents,
                        RequestOutcome outcome,
                        LoginReputationClientResponse::VerdictType verdict_type,
                        const std::string& verdict_token,
                        ReusedPasswordAccountType password_type) override;

  void ShowInterstitial(content::WebContents* web_contens,
                        ReusedPasswordAccountType password_type) override;

  // Called when user interacts with password protection UIs.
  void OnUserAction(content::WebContents* web_contents,
                    ReusedPasswordAccountType password_type,
                    RequestOutcome outcome,
                    LoginReputationClientResponse::VerdictType verdict_type,
                    const std::string& verdict_token,
                    WarningUIType ui_type,
                    WarningAction action);

  // Called during the construction of Observer subclass.
  virtual void AddObserver(Observer* observer);

  // Called during the destruction of the observer subclass.
  virtual void RemoveObserver(Observer* observer);

  // Starts collecting threat details if user has extended reporting enabled and
  // is not in incognito mode.
  void MaybeStartThreatDetailsCollection(
      content::WebContents* web_contents,
      const std::string& token,
      ReusedPasswordAccountType password_type);

  // Sends threat details if user has extended reporting enabled and is not in
  // incognito mode.
  void MaybeFinishCollectingThreatDetails(content::WebContents* web_contents,
                                          bool did_proceed);

  // Check if Gaia password hash has changed. If it is changed, it will call
  // |OnGaiaPasswordChanged|. |username| is used to get the appropriate account
  // to check if the account is a Gmail account as no reporting is done for
  // those accounts.
  void CheckGaiaPasswordChangeForAllSignedInUsers(const std::string& username);

  // Called when user's GAIA password changed. |username| is used to get
  // the account the password is associated with. |is_other_gaia_password|
  // specifies whether the account is syncing or not syncing (content area).
  void OnGaiaPasswordChanged(const std::string& username,
                             bool is_other_gaia_password);
#endif

  // If user has clicked through any Safe Browsing interstitial on this given
  // |web_contents|.
  bool UserClickedThroughSBInterstitial(
      content::WebContents* web_contents) override;

  // If |prefs::kPasswordProtectionWarningTrigger| is not managed by enterprise
  // policy, this function should always return PHISHING_REUSE. Otherwise,
  // returns the specified pref value adjusted for the given username's account
  // type.
  PasswordProtectionTrigger GetPasswordProtectionWarningTriggerPref(
      ReusedPasswordAccountType password_type) const override;

#if defined(SYNC_PASSWORD_REUSE_DETECTION_ENABLED)
  // Gets the enterprise change password URL if specified in policy,
  // otherwise gets the default GAIA change password URL.
  GURL GetEnterpriseChangePasswordURL() const;

  // Gets the GAIA change password URL based on |account_info_|.
  GURL GetDefaultChangePasswordURL() const;
#endif

  // If |url| matches Safe Browsing whitelist domains, password protection
  // change password URL, or password protection login URLs in the enterprise
  // policy.
  bool IsURLWhitelistedForPasswordEntry(const GURL& url,
                                        RequestOutcome* reason) const override;

  // Gets the type of sync account associated with current profile or
  // |NOT_SIGNED_IN|.
  LoginReputationClientRequest::PasswordReuseEvent::SyncAccountType
  GetSyncAccountType() const override;

#if defined(SYNC_PASSWORD_REUSE_DETECTION_ENABLED)
  // Gets the detailed warning text that should show in the modal warning dialog
  // and page info bubble.
  base::string16 GetWarningDetailText(
      ReusedPasswordAccountType password_type) const;

  // If password protection trigger is configured via enterprise policy, gets
  // the name of the organization that owns the enterprise policy. Otherwise,
  // returns an empty string.
  std::string GetOrganizationName(
      ReusedPasswordAccountType password_type) const;

  // If the browser is not incognito and the user is reusing their enterprise
  // password or is a GSuite user, triggers
  // safeBrowsingPrivate.OnPolicySpecifiedPasswordReuseDetected.
  // |username| can be an email address or a username for a non-GAIA or
  // saved-password reuse. No validation has been done on it.
  void MaybeReportPasswordReuseDetected(content::WebContents* web_contents,
                                        const std::string& username,
                                        PasswordType password_type,
                                        bool is_phishing_url) override;

  // Triggers "safeBrowsingPrivate.OnPolicySpecifiedPasswordChanged" API.
  void ReportPasswordChanged() override;

  // Returns true if there's any enterprise password reuses unhandled in
  // |web_contents|. "Unhandled" is defined as user hasn't clicked on
  // "Change Password" button in modal warning dialog.
  bool HasUnhandledEnterprisePasswordReuse(
      content::WebContents* web_contents) const;
#endif

// TODO(crbug.com/995926): Enable caching on Android
// Note: Before enable verdict cache manager on Android,
// add CONTENT_SETTINGS_TYPE_PASSWORD_PROTECTION to Android
// Otherwise, it will fail on /components/ content_settings/core/browser/
// content_settings_default_provider.cc/GetRuleIterator
#if BUILDFLAG(FULL_SAFE_BROWSING)
  // Stores |verdict| in the cache based on its |trigger_type|, |url|,
  // reused |password_type|, |verdict| and |receive_time|.
  void CacheVerdict(const GURL& url,
                    LoginReputationClientRequest::TriggerType trigger_type,
                    ReusedPasswordAccountType password_type,
                    const LoginReputationClientResponse& verdict,
                    const base::Time& receive_time) override;

  // Returns the number of saved verdicts for the given |trigger_type|.
  int GetStoredVerdictCount(
      LoginReputationClientRequest::TriggerType trigger_type) override;

  // Looks up the cached verdict response. If verdict is not available or is
  // expired, return VERDICT_TYPE_UNSPECIFIED. Can be called on any thread.
  LoginReputationClientResponse::VerdictType GetCachedVerdict(
      const GURL& url,
      LoginReputationClientRequest::TriggerType trigger_type,
      ReusedPasswordAccountType password_type,
      LoginReputationClientResponse* out_response) override;
#endif

  // Gets |account_info_| based on |profile_|.
  AccountInfo GetAccountInfo() const override;

 protected:
  // PasswordProtectionService overrides.
  const policy::BrowserPolicyConnector* GetBrowserPolicyConnector()
      const override;
  // Obtains referrer chain of |event_url| and |event_tab_id| and add this
  // info into |frame|.
  void FillReferrerChain(const GURL& event_url,
                         SessionID event_tab_id,
                         LoginReputationClientRequest::Frame* frame) override;

  bool IsExtendedReporting() override;

  bool IsIncognito() override;

  // Checks if pinging should be enabled based on the |trigger_type|,
  // |password_type|, updates |reason| accordingly.
  bool IsPingingEnabled(LoginReputationClientRequest::TriggerType trigger_type,
                        ReusedPasswordAccountType password_type,
                        RequestOutcome* reason) override;

  // If current profile has enabled history syncing.
  bool IsHistorySyncEnabled() override;

  // If primary account is syncing.
  bool IsPrimaryAccountSyncing() const override;

  // If primary account is signed in.
  bool IsPrimaryAccountSignedIn() const override;

  // If a domain is not defined for the primary account. This means the primary
  // account is a Gmail account.
  bool IsPrimaryAccountGmail() const override;

  // Gets the AccountInfo for the account corresponding to |username| from the
  // list of signed-in users.
  AccountInfo GetSignedInNonSyncAccount(
      const std::string& username) const override;

  // If the domain for the non-syncing account is equal to
  // |kNoHostedDomainFound|, this means that the account is a Gmail account.
  bool IsOtherGaiaAccountGmail(const std::string& username) const override;

#if BUILDFLAG(FULL_SAFE_BROWSING)
  // If user is under advanced protection.
  bool IsUnderAdvancedProtection() override;
#endif

#if defined(SYNC_PASSWORD_REUSE_DETECTION_ENABLED)
  void MaybeLogPasswordReuseDetectedEvent(
      content::WebContents* web_contents) override;

  void MaybeLogPasswordReuseLookupEvent(
      content::WebContents* web_contents,
      RequestOutcome outcome,
      PasswordType password_type,
      const LoginReputationClientResponse* response) override;
#endif
  // Updates security state for the current |web_contents| based on
  // |threat_type| and reused |password_type|, such that page info bubble will
  // show appropriate status when user clicks on the security chip.
  void UpdateSecurityState(SBThreatType threat_type,
                           ReusedPasswordAccountType password_type,
                           content::WebContents* web_contents) override;

  void RemoveUnhandledSyncPasswordReuseOnURLsDeleted(
      bool all_history,
      const history::URLRows& deleted_rows) override;

#if defined(SYNC_PASSWORD_REUSE_DETECTION_ENABLED)
  void HandleUserActionOnModalWarning(
      content::WebContents* web_contents,
      ReusedPasswordAccountType password_type,
      RequestOutcome outcome,
      LoginReputationClientResponse::VerdictType verdict_type,
      const std::string& verdict_token,
      WarningAction action);

  void HandleUserActionOnPageInfo(content::WebContents* web_contents,
                                  ReusedPasswordAccountType password_type,
                                  WarningAction action);

  void HandleUserActionOnSettings(content::WebContents* web_contents,
                                  ReusedPasswordAccountType password_type,
                                  WarningAction action);

  void HandleResetPasswordOnInterstitial(content::WebContents* web_contents,
                                         WarningAction action);
#endif
  // Returns base-10 string representation of the uint64t hash.
  std::string GetSyncPasswordHashFromPrefs();

  void SetGaiaPasswordHashForTesting(const std::string& new_password_hash) {
    sync_password_hash_ = new_password_hash;
  }

#if defined(SYNC_PASSWORD_REUSE_DETECTION_ENABLED)
  // Determines if we should show chrome://reset-password interstitial based on
  // previous request outcome, the reused |password_type| and the
  // |main_frame_url|.
  bool CanShowInterstitial(RequestOutcome reason,
                           ReusedPasswordAccountType password_type,
                           const GURL& main_frame_url) override;
#endif

  // Unit tests
  FRIEND_TEST_ALL_PREFIXES(ChromePasswordProtectionServiceTest,
                           VerifyUserPopulationForPasswordOnFocusPing);
  FRIEND_TEST_ALL_PREFIXES(ChromePasswordProtectionServiceTest,
                           VerifyUserPopulationForSyncPasswordEntryPing);
  FRIEND_TEST_ALL_PREFIXES(ChromePasswordProtectionServiceTest,
                           VerifyUserPopulationForSavedPasswordEntryPing);
  FRIEND_TEST_ALL_PREFIXES(
      ChromePasswordProtectionServiceTest,
      VerifyPasswordReuseUserEventNotRecordedDueToIncognito);
  FRIEND_TEST_ALL_PREFIXES(
      ChromePasswordProtectionServiceTest,
      VerifyPasswordReuseUserEventRecordedForOtherGaiaPassword);
  FRIEND_TEST_ALL_PREFIXES(ChromePasswordProtectionServiceTest,
                           VerifyPasswordReuseDetectedUserEventRecorded);
  FRIEND_TEST_ALL_PREFIXES(
      ChromePasswordProtectionServiceTest,
      VerifyPasswordReuseLookupEventNotRecordedFeatureNotEnabled);
  FRIEND_TEST_ALL_PREFIXES(ChromePasswordProtectionServiceTest,
                           VerifyPasswordReuseLookupUserEventRecorded);
  FRIEND_TEST_ALL_PREFIXES(ChromePasswordProtectionServiceTest,
                           VerifyGetSyncAccountType);
  FRIEND_TEST_ALL_PREFIXES(ChromePasswordProtectionServiceTest,
                           VerifyUpdateSecurityState);
  FRIEND_TEST_ALL_PREFIXES(ChromePasswordProtectionServiceTest,
                           VerifyGetChangePasswordURL);
  FRIEND_TEST_ALL_PREFIXES(
      ChromePasswordProtectionServiceTest,
      VerifyUnhandledSyncPasswordReuseUponClearHistoryDeletion);
  FRIEND_TEST_ALL_PREFIXES(ChromePasswordProtectionServiceTest,
                           VerifyCanShowInterstitial);
  FRIEND_TEST_ALL_PREFIXES(ChromePasswordProtectionServiceTest,
                           VerifySendsPingForAboutBlank);
  FRIEND_TEST_ALL_PREFIXES(ChromePasswordProtectionServiceTest,
                           VerifyPasswordCaptureEventScheduledOnStartup);
  FRIEND_TEST_ALL_PREFIXES(ChromePasswordProtectionServiceTest,
                           VerifyPasswordCaptureEventScheduledFromPref);
  FRIEND_TEST_ALL_PREFIXES(ChromePasswordProtectionServiceTest,
                           VerifyPasswordCaptureEventReschedules);
  FRIEND_TEST_ALL_PREFIXES(ChromePasswordProtectionServiceTest,
                           VerifyPasswordCaptureEventRecorded);
  FRIEND_TEST_ALL_PREFIXES(ChromePasswordProtectionServiceTest,
                           VerifyPasswordReuseDetectedSecurityEventRecorded);
  // Browser tests
  FRIEND_TEST_ALL_PREFIXES(ChromePasswordProtectionServiceBrowserTest,
                           VerifyCheckGaiaPasswordChange);
  FRIEND_TEST_ALL_PREFIXES(ChromePasswordProtectionServiceBrowserTest,
                           OnEnterpriseTriggerOff);
  FRIEND_TEST_ALL_PREFIXES(ChromePasswordProtectionServiceBrowserTest,
                           OnEnterpriseTriggerOffGSuite);

 private:
  friend class MockChromePasswordProtectionService;
  friend class ChromePasswordProtectionServiceBrowserTest;
  friend class SecurityStateTabHelperTest;
  FRIEND_TEST_ALL_PREFIXES(
      ChromePasswordProtectionServiceTest,
      VerifyOnPolicySpecifiedPasswordReuseDetectedEventForPhishingReuse);

  // Gets prefs associated with |profile_|.
  PrefService* GetPrefs();

  // Returns whether the profile is valid and has safe browsing service enabled.
  bool IsSafeBrowsingEnabled();

#if defined(SYNC_PASSWORD_REUSE_DETECTION_ENABLED)
  void MaybeLogPasswordReuseLookupResult(
      content::WebContents* web_contents,
      sync_pb::GaiaPasswordReuse::PasswordReuseLookup::LookupResult result);

  void MaybeLogPasswordReuseLookupResultWithVerdict(
      content::WebContents* web_contents,
      PasswordType password_type,
      sync_pb::GaiaPasswordReuse::PasswordReuseLookup::LookupResult result,
      sync_pb::GaiaPasswordReuse::PasswordReuseLookup::ReputationVerdict
          verdict,
      const std::string& verdict_token);

  void MaybeLogPasswordReuseDialogInteraction(
      int64_t navigation_id,
      sync_pb::GaiaPasswordReuse::PasswordReuseDialogInteraction::
          InteractionResult interaction_result);
#endif

  // Log that we captured the password, either due to log-in or by timer.
  // This also sets the reoccuring timer.
  void MaybeLogPasswordCapture(bool did_log_in);
  void SetLogPasswordCaptureTimer(const base::TimeDelta& delay);

#if defined(SYNC_PASSWORD_REUSE_DETECTION_ENABLED)
  void OnModalWarningShownForGaiaPassword(
      content::WebContents* web_contents,
      ReusedPasswordAccountType password_type,
      const std::string& verdict_token);

  void OnModalWarningShownForEnterprisePassword(
      content::WebContents* web_contents,
      ReusedPasswordAccountType password_type,
      const std::string& verdict_token);
#endif

  // Constructor used for tests only.
  ChromePasswordProtectionService(
      Profile* profile,
      scoped_refptr<SafeBrowsingUIManager> ui_manager,
      StringProvider sync_password_hash_provider,
      VerdictCacheManager* cache_manager);

  // Code shared by both ctors.
  void Init();

#if defined(SYNC_PASSWORD_REUSE_DETECTION_ENABLED)
  // If enterprise admin turns off password protection, removes all captured
  // enterprise password hashes.
  void OnWarningTriggerChanged();

  // Informs PasswordReuseDetector that enterprise password URLs (login URL or
  // change password URL) have been changed.
  void OnEnterprisePasswordUrlChanged();
#endif

#if BUILDFLAG(FULL_SAFE_BROWSING)
  // Get the content area size of current browsing window.
  gfx::Size GetCurrentContentAreaSize() const override;
#endif

  scoped_refptr<SafeBrowsingUIManager> ui_manager_;
  TriggerManager* trigger_manager_;
  // Profile associated with this instance.
  Profile* profile_;
  // Current sync password hash.
  std::string sync_password_hash_;
  scoped_refptr<SafeBrowsingNavigationObserverManager>
      navigation_observer_manager_;
  base::ObserverList<Observer>::Unchecked observer_list_;
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;
  std::set<content::WebContents*>
      web_contents_with_unhandled_enterprise_reuses_;

  // Subscription for state changes. When this subscription is notified, it
  // means HashPasswordManager password data list has changed.
  std::unique_ptr<
      base::CallbackList<void(const std::string& username)>::Subscription>
      hash_password_manager_subscription_;

  // Reference to the current profile's VerdictCacheManager. This is unowned.
  VerdictCacheManager* cache_manager_;

  // Schedules the next time to log the PasswordCaptured event.
  base::OneShotTimer log_password_capture_timer_;

  // Used to inject a different password hash, for testing. It's done as a
  // member callback rather than a virtual function because it's needed in the
  // constructor.
  StringProvider sync_password_hash_provider_for_testing_;

  DISALLOW_COPY_AND_ASSIGN(ChromePasswordProtectionService);
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CHROME_PASSWORD_PROTECTION_SERVICE_H_
