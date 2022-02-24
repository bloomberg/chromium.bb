// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/turn_sync_on_helper.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/check.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/no_destructor.h"
#include "base/observer_list.h"
#include "base/strings/utf_string_conversions.h"
#include "base/supports_user_data.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/policy/chrome_policy_conversions_client.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service_factory.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/signin/account_id_from_account_info.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/sync/profile_signin_confirmation_helper.h"
#include "chrome/browser/ui/tab_dialogs.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/browser/ui/webui/signin/signin_ui_error.h"
#include "chrome/browser/ui/webui/signin/signin_utils_desktop.h"
#include "chrome/browser/unified_consent/unified_consent_service_factory.h"
#include "chrome/common/pref_names.h"
#include "components/keyed_service/content/browser_context_keyed_service_shutdown_notifier_factory.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/browser/policy_conversions.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/accounts_mutator.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_user_settings.h"
#include "components/unified_consent/unified_consent_service.h"
#include "content/public/browser/storage_partition.h"

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
#include "chrome/browser/signin/dice_signed_in_profile_creator.h"
#include "chrome/browser/ui/webui/signin/turn_sync_on_helper_delegate_impl.h"
#endif

namespace {

const void* const kCurrentTurnSyncOnHelperKey = &kCurrentTurnSyncOnHelperKey;
bool g_show_sync_enabled_ui_for_testing_ = false;

// A helper class to watch profile lifetime.
class TurnSyncOnHelperShutdownNotifierFactory
    : public BrowserContextKeyedServiceShutdownNotifierFactory {
 public:
  TurnSyncOnHelperShutdownNotifierFactory(
      const TurnSyncOnHelperShutdownNotifierFactory&) = delete;
  TurnSyncOnHelperShutdownNotifierFactory& operator=(
      const TurnSyncOnHelperShutdownNotifierFactory&) = delete;

  static TurnSyncOnHelperShutdownNotifierFactory* GetInstance() {
    static base::NoDestructor<TurnSyncOnHelperShutdownNotifierFactory> factory;
    return factory.get();
  }

 private:
  friend class base::NoDestructor<TurnSyncOnHelperShutdownNotifierFactory>;

  TurnSyncOnHelperShutdownNotifierFactory()
      : BrowserContextKeyedServiceShutdownNotifierFactory(
            "TurnSyncOnHelperShutdownNotifier") {
    DependsOn(IdentityManagerFactory::GetInstance());
    DependsOn(SyncServiceFactory::GetInstance());
    DependsOn(UnifiedConsentServiceFactory::GetInstance());
    DependsOn(policy::UserPolicySigninServiceFactory::GetInstance());
  }
  ~TurnSyncOnHelperShutdownNotifierFactory() override {}
};

// User input handler for the signin confirmation dialog.
class SigninDialogDelegate : public ui::ProfileSigninConfirmationDelegate {
 public:
  explicit SigninDialogDelegate(TurnSyncOnHelper::SigninChoiceCallback callback)
      : callback_(std::move(callback)) {
    DCHECK(callback_);
  }
  SigninDialogDelegate(const SigninDialogDelegate&) = delete;
  SigninDialogDelegate& operator=(const SigninDialogDelegate&) = delete;
  ~SigninDialogDelegate() override = default;

  void OnCancelSignin() override {
    DCHECK(callback_);
    std::move(callback_).Run(TurnSyncOnHelper::SIGNIN_CHOICE_CANCEL);
  }

  void OnContinueSignin() override {
    DCHECK(callback_);
    std::move(callback_).Run(TurnSyncOnHelper::SIGNIN_CHOICE_CONTINUE);
  }

  void OnSigninWithNewProfile() override {
    DCHECK(callback_);
    std::move(callback_).Run(TurnSyncOnHelper::SIGNIN_CHOICE_NEW_PROFILE);
  }

 private:
  TurnSyncOnHelper::SigninChoiceCallback callback_;
};

struct CurrentTurnSyncOnHelperUserData : public base::SupportsUserData::Data {
  TurnSyncOnHelper* current_helper = nullptr;
};

TurnSyncOnHelper* GetCurrentTurnSyncOnHelper(Profile* profile) {
  base::SupportsUserData::Data* data =
      profile->GetUserData(kCurrentTurnSyncOnHelperKey);
  if (!data)
    return nullptr;
  CurrentTurnSyncOnHelperUserData* wrapper =
      static_cast<CurrentTurnSyncOnHelperUserData*>(data);
  TurnSyncOnHelper* helper = wrapper->current_helper;
  DCHECK(helper);
  return helper;
}

void SetCurrentTurnSyncOnHelper(Profile* profile, TurnSyncOnHelper* helper) {
  if (!helper) {
    DCHECK(profile->GetUserData(kCurrentTurnSyncOnHelperKey));
    profile->RemoveUserData(kCurrentTurnSyncOnHelperKey);
    return;
  }

  DCHECK(!profile->GetUserData(kCurrentTurnSyncOnHelperKey));
  std::unique_ptr<CurrentTurnSyncOnHelperUserData> wrapper =
      std::make_unique<CurrentTurnSyncOnHelperUserData>();
  wrapper->current_helper = helper;
  profile->SetUserData(kCurrentTurnSyncOnHelperKey, std::move(wrapper));
}

}  // namespace

// static
void TurnSyncOnHelper::Delegate::ShowLoginErrorForBrowser(
    const SigninUIError& error,
    Browser* browser) {
  LoginUIServiceFactory::GetForProfile(browser->profile())
      ->DisplayLoginResult(browser, error);
}

TurnSyncOnHelper::TurnSyncOnHelper(
    Profile* profile,
    signin_metrics::AccessPoint signin_access_point,
    signin_metrics::PromoAction signin_promo_action,
    signin_metrics::Reason signin_reason,
    const CoreAccountId& account_id,
    SigninAbortedMode signin_aborted_mode,
    std::unique_ptr<Delegate> delegate,
    base::OnceClosure callback)
    : delegate_(std::move(delegate)),
      profile_(profile),
      identity_manager_(IdentityManagerFactory::GetForProfile(profile)),
      signin_access_point_(signin_access_point),
      signin_promo_action_(signin_promo_action),
      signin_reason_(signin_reason),
      signin_aborted_mode_(signin_aborted_mode),
      account_info_(
          identity_manager_->FindExtendedAccountInfoByAccountId(account_id)),
      scoped_callback_runner_(std::move(callback)),
      shutdown_subscription_(
          TurnSyncOnHelperShutdownNotifierFactory::GetInstance()
              ->Get(profile)
              ->Subscribe(base::BindOnce(&TurnSyncOnHelper::AbortAndDelete,
                                         base::Unretained(this)))) {
  DCHECK(delegate_);
  DCHECK(profile_);
  // Should not start syncing if the profile is already authenticated
  DCHECK(!identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSync));

  // Cancel any existing helper.
  AttachToProfile();

  if (account_info_.gaia.empty() || account_info_.email.empty()) {
    LOG(ERROR) << "Cannot turn Sync On for invalid account.";
    base::SequencedTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
    return;
  }

  DCHECK(!account_info_.gaia.empty());
  DCHECK(!account_info_.email.empty());

  if (HasCanOfferSigninError()) {
    // Do not self-destruct synchronously in the constructor.
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&TurnSyncOnHelper::AbortAndDelete,
                                  weak_pointer_factory_.GetWeakPtr()));
    return;
  }

  if (!IsCrossAccountError(profile_, account_info_.email, account_info_.gaia)) {
    TurnSyncOnWithProfileMode(ProfileMode::CURRENT_PROFILE);
    return;
  }

  // Handles cross account sign in error. If |account_info_| does not match the
  // last authenticated account of the current profile, then Chrome will show a
  // confirmation dialog before starting sync.
  // TODO(skym): Warn for high risk upgrade scenario (https://crbug.com/572754).
  std::string last_email =
      profile_->GetPrefs()->GetString(prefs::kGoogleServicesLastUsername);
  delegate_->ShowMergeSyncDataConfirmation(
      last_email, account_info_.email,
      base::BindOnce(&TurnSyncOnHelper::OnMergeAccountConfirmation,
                     weak_pointer_factory_.GetWeakPtr()));
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
TurnSyncOnHelper::TurnSyncOnHelper(
    Profile* profile,
    Browser* browser,
    signin_metrics::AccessPoint signin_access_point,
    signin_metrics::PromoAction signin_promo_action,
    signin_metrics::Reason signin_reason,
    const CoreAccountId& account_id,
    SigninAbortedMode signin_aborted_mode)
    : TurnSyncOnHelper(profile,
                       signin_access_point,
                       signin_promo_action,
                       signin_reason,
                       account_id,
                       signin_aborted_mode,
                       std::make_unique<TurnSyncOnHelperDelegateImpl>(browser),
                       base::OnceClosure()) {}
#endif

TurnSyncOnHelper::~TurnSyncOnHelper() {
  DCHECK_EQ(this, GetCurrentTurnSyncOnHelper(profile_));
  SetCurrentTurnSyncOnHelper(profile_, nullptr);
}

bool TurnSyncOnHelper::HasCanOfferSigninError() {
  SigninUIError can_offer_error =
      CanOfferSignin(profile_, account_info_.gaia, account_info_.email);
  if (can_offer_error.IsOk())
    return false;

  // Display the error message
  delegate_->ShowLoginError(can_offer_error);
  return true;
}

void TurnSyncOnHelper::OnMergeAccountConfirmation(SigninChoice choice) {
  switch (choice) {
    case SIGNIN_CHOICE_NEW_PROFILE:
      base::RecordAction(
          base::UserMetricsAction("Signin_ImportDataPrompt_DontImport"));
      TurnSyncOnWithProfileMode(ProfileMode::NEW_PROFILE);
      break;
    case SIGNIN_CHOICE_CONTINUE:
      base::RecordAction(
          base::UserMetricsAction("Signin_ImportDataPrompt_ImportData"));
      TurnSyncOnWithProfileMode(ProfileMode::CURRENT_PROFILE);
      break;
    case SIGNIN_CHOICE_CANCEL:
      base::RecordAction(
          base::UserMetricsAction("Signin_ImportDataPrompt_Cancel"));
      AbortAndDelete();
      break;
    case SIGNIN_CHOICE_SIZE:
      NOTREACHED();
      AbortAndDelete();
      break;
  }
}

void TurnSyncOnHelper::OnEnterpriseAccountConfirmation(SigninChoice choice) {
  enterprise_account_confirmed_ =
      choice == SIGNIN_CHOICE_CONTINUE || choice == SIGNIN_CHOICE_NEW_PROFILE;
  signin_util::RecordEnterpriseProfileCreationUserChoice(
      profile_, enterprise_account_confirmed_);

  switch (choice) {
    case SIGNIN_CHOICE_CANCEL:
      base::RecordAction(
          base::UserMetricsAction("Signin_EnterpriseAccountPrompt_Cancel"));
      AbortAndDelete();
      break;
    case SIGNIN_CHOICE_CONTINUE:
      base::RecordAction(
          base::UserMetricsAction("Signin_EnterpriseAccountPrompt_ImportData"));
      LoadPolicyWithCachedCredentials();
      break;
    case SIGNIN_CHOICE_NEW_PROFILE:
      base::RecordAction(base::UserMetricsAction(
          "Signin_EnterpriseAccountPrompt_DontImportData"));
      CreateNewSignedInProfile();
      break;
    case SIGNIN_CHOICE_SIZE:
      NOTREACHED();
      AbortAndDelete();
      break;
  }
}

void TurnSyncOnHelper::TurnSyncOnWithProfileMode(ProfileMode profile_mode) {
  switch (profile_mode) {
    case ProfileMode::CURRENT_PROFILE: {
      // If this is a new signin (no account authenticated yet) try loading
      // policy for this user now, before any signed in services are
      // initialized.
      policy::UserPolicySigninService* policy_service =
          policy::UserPolicySigninServiceFactory::GetForProfile(profile_);
      policy_service->RegisterForPolicyWithAccountId(
          account_info_.email, account_info_.account_id,
          base::BindOnce(&TurnSyncOnHelper::OnRegisteredForPolicy,
                         weak_pointer_factory_.GetWeakPtr()));
      break;
    }
    case ProfileMode::NEW_PROFILE:
      // If this is a new signin (no account authenticated yet) in a new
      // profile, then just create the new signed-in profile and skip loading
      // the policy as there is no need to ask the user again if they should be
      // signed in to a new profile. Note that in this case the policy will be
      // applied after the new profile is signed in.
      CreateNewSignedInProfile();
      break;
  }
}

void TurnSyncOnHelper::OnRegisteredForPolicy(const std::string& dm_token,
                                             const std::string& client_id) {
  // If there's no token for the user (policy registration did not succeed) just
  // finish signing in.
  if (dm_token.empty()) {
    DVLOG(1) << "Policy registration failed";
    SigninAndShowSyncConfirmationUI();
    return;
  }

  DVLOG(1) << "Policy registration succeeded: dm_token=" << dm_token;

  DCHECK(dm_token_.empty());
  DCHECK(client_id_.empty());
  dm_token_ = dm_token;
  client_id_ = client_id;

  if (!chrome::enterprise_util::UserAcceptedAccountManagement(profile_)) {
    // Allow user to create a new profile before continuing with sign-in.
    delegate_->ShowEnterpriseAccountConfirmation(
        account_info_,
        base::BindOnce(&TurnSyncOnHelper::OnEnterpriseAccountConfirmation,
                       weak_pointer_factory_.GetWeakPtr()));
    return;
  }

  DCHECK(chrome::enterprise_util::UserAcceptedAccountManagement(profile_));
  LoadPolicyWithCachedCredentials();
}

void TurnSyncOnHelper::LoadPolicyWithCachedCredentials() {
  DCHECK(!dm_token_.empty());
  DCHECK(!client_id_.empty());
  policy::UserPolicySigninService* policy_service =
      policy::UserPolicySigninServiceFactory::GetForProfile(profile_);
  policy_service->FetchPolicyForSignedInUser(
      AccountIdFromAccountInfo(account_info_), dm_token_, client_id_,
      profile_->GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess(),
      base::BindOnce(&TurnSyncOnHelper::OnPolicyFetchComplete,
                     weak_pointer_factory_.GetWeakPtr()));
}

void TurnSyncOnHelper::OnPolicyFetchComplete(bool success) {
  // For now, we allow signin to complete even if the policy fetch fails. If
  // we ever want to change this behavior, we could call
  // PrimaryAccountMutator::ClearPrimaryAccount() here instead.
  DLOG_IF(ERROR, !success) << "Error fetching policy for user";
  DVLOG_IF(1, success) << "Policy fetch successful - completing signin";
  if (VLOG_IS_ON(2)) {
    // User cloud policies have been fetched from the server. Dump all policy
    // values into log once these new policies are merged.
    profile_->GetProfilePolicyConnector()
        ->policy_service()
        ->AddProviderUpdateObserver(this);
  }
  SigninAndShowSyncConfirmationUI();
}

void TurnSyncOnHelper::OnProviderUpdatePropagated(
    policy::ConfigurationPolicyProvider* provider) {
  if (provider != profile_->GetUserCloudPolicyManager())
    return;
  VLOG(2) << "Policies after sign in:";
  VLOG(2) << policy::DictionaryPolicyConversions(
                 std::make_unique<policy::ChromePolicyConversionsClient>(
                     profile_))
                 .ToJSON();
  profile_->GetProfilePolicyConnector()
      ->policy_service()
      ->RemoveProviderUpdateObserver(this);
}

void TurnSyncOnHelper::CreateNewSignedInProfile() {
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  DCHECK(!dice_signed_in_profile_creator_);
  // Unretained is fine because the profile creator is owned by this.
  dice_signed_in_profile_creator_ =
      std::make_unique<DiceSignedInProfileCreator>(
          profile_, account_info_.account_id,
          /*local_profile_name=*/std::u16string(), /*icon_index=*/absl::nullopt,
          /*use_guest=*/false,
          base::BindOnce(&TurnSyncOnHelper::OnNewSignedInProfileCreated,
                         base::Unretained(this)));
#else
  NOTIMPLEMENTED() << "Creating profiles is not yet supported on lacros.";
#endif
}

syncer::SyncService* TurnSyncOnHelper::GetSyncService() {
  return SyncServiceFactory::IsSyncAllowed(profile_)
             ? SyncServiceFactory::GetForProfile(profile_)
             : nullptr;
}

void TurnSyncOnHelper::OnNewSignedInProfileCreated(Profile* new_profile) {
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  DCHECK(dice_signed_in_profile_creator_);
  dice_signed_in_profile_creator_.reset();
  ProfileMetrics::LogProfileAddNewUser(ProfileMetrics::ADD_NEW_USER_SYNC_FLOW);

  if (!new_profile) {
    // TODO(atwilson): On error, unregister the client to release the DMToken
    // and surface a better error for the user.
    AbortAndDelete();
    return;
  }

  DCHECK_NE(profile_, new_profile);
  SwitchToProfile(new_profile);
  DCHECK_EQ(profile_, new_profile);

  if (!dm_token_.empty()) {
    // Load policy for the just-created profile - once policy has finished
    // loading the signin process will complete.
    DCHECK(!client_id_.empty());
    LoadPolicyWithCachedCredentials();
  } else {
    // No policy to load - simply complete the signin process.
    SigninAndShowSyncConfirmationUI();
  }
#else
  NOTIMPLEMENTED() << "Creating profiles is not yet supported for lacros.";
#endif
}

void TurnSyncOnHelper::SigninAndShowSyncConfirmationUI() {
  // Signin.
  auto* primary_account_mutator = identity_manager_->GetPrimaryAccountMutator();
  primary_account_mutator->SetPrimaryAccount(account_info_.account_id,
                                             signin::ConsentLevel::kSync);
  signin_metrics::LogSigninAccessPointCompleted(signin_access_point_,
                                                signin_promo_action_);
  signin_metrics::LogSigninReason(signin_reason_);
  base::RecordAction(base::UserMetricsAction("Signin_Signin_Succeed"));

  bool user_accepted_management =
      chrome::enterprise_util::UserAcceptedAccountManagement(profile_);
  if (!user_accepted_management) {
    chrome::enterprise_util::SetUserAcceptedAccountManagement(
        profile_, enterprise_account_confirmed_);
    user_accepted_management = enterprise_account_confirmed_;
  }
  if (user_accepted_management)
    signin_aborted_mode_ = SigninAbortedMode::KEEP_ACCOUNT;

  syncer::SyncService* sync_service = GetSyncService();
  if (sync_service) {
    // Take a SyncSetupInProgressHandle, so that the UI code can use
    // IsFirstSyncSetupInProgress() as a way to know if there is a signin in
    // progress.
    // TODO(https://crbug.com/811211): Remove this handle.
    sync_blocker_ = sync_service->GetSetupInProgressHandle();
    sync_service->GetUserSettings()->SetSyncRequested(true);

    // For managed users and users on enterprise machines that might have cloud
    // policies, it is important to wait until sync is initialized so that the
    // confirmation UI can be aware of startup errors. Since all users can be
    // subjected to cloud policies through device or browser management (CBCM),
    // this is needed to make sure that all cloud policies are loaded before any
    // dialog is shown to check whether sync was disabled by admin. Only wait
    // for cloud policies because local policies are instantly available. See
    // http://crbug.com/812546
    bool may_have_cloud_policies =
        !policy::BrowserPolicyConnector::IsNonEnterpriseUser(
            account_info_.email) ||
        policy::ManagementServiceFactory::GetForProfile(profile_)
            ->HasManagementAuthority(
                policy::EnterpriseManagementAuthority::CLOUD) ||
        policy::ManagementServiceFactory::GetForProfile(profile_)
            ->HasManagementAuthority(
                policy::EnterpriseManagementAuthority::CLOUD_DOMAIN) ||
        policy::ManagementServiceFactory::GetForPlatform()
            ->HasManagementAuthority(
                policy::EnterpriseManagementAuthority::CLOUD) ||
        policy::ManagementServiceFactory::GetForPlatform()
            ->HasManagementAuthority(
                policy::EnterpriseManagementAuthority::CLOUD_DOMAIN);

    if (may_have_cloud_policies &&
        SyncStartupTracker::GetSyncServiceState(sync_service) ==
            SyncStartupTracker::SYNC_STARTUP_PENDING) {
      sync_startup_tracker_ =
          std::make_unique<SyncStartupTracker>(sync_service, this);
      return;
    }
  }
  ShowSyncConfirmationUI();
}

void TurnSyncOnHelper::SyncStartupCompleted() {
  DCHECK(sync_startup_tracker_);
  sync_startup_tracker_.reset();
  ShowSyncConfirmationUI();
}

void TurnSyncOnHelper::SyncStartupFailed() {
  DCHECK(sync_startup_tracker_);
  sync_startup_tracker_.reset();
  ShowSyncConfirmationUI();
}

// static
void TurnSyncOnHelper::SetShowSyncEnabledUiForTesting(
    bool show_sync_enabled_ui_for_testing) {
  g_show_sync_enabled_ui_for_testing_ = show_sync_enabled_ui_for_testing;
}

// static
bool TurnSyncOnHelper::HasCurrentTurnSyncOnHelperForTesting(Profile* profile) {
  return !!GetCurrentTurnSyncOnHelper(profile);
}

void TurnSyncOnHelper::ShowSyncConfirmationUI() {
  if (g_show_sync_enabled_ui_for_testing_ || GetSyncService()) {
    delegate_->ShowSyncConfirmation(
        base::BindOnce(&TurnSyncOnHelper::FinishSyncSetupAndDelete,
                       weak_pointer_factory_.GetWeakPtr()));
  } else {
    // The sync disabled dialog has an explicit "sign-out" label for the
    // LoginUIService::ABORT_SYNC action, force the mode to remove the account.
    signin_aborted_mode_ = SigninAbortedMode::REMOVE_ACCOUNT;
    // Use the email-based heuristic if `account_info_` isn't fully initialized.
    const bool is_managed_account =
        account_info_.IsValid()
            ? account_info_.IsManaged()
            : !policy::BrowserPolicyConnector::IsNonEnterpriseUser(
                  account_info_.email);
    delegate_->ShowSyncDisabledConfirmation(
        is_managed_account,
        base::BindOnce(&TurnSyncOnHelper::FinishSyncSetupAndDelete,
                       weak_pointer_factory_.GetWeakPtr()));
  }
}

void TurnSyncOnHelper::FinishSyncSetupAndDelete(
    LoginUIService::SyncConfirmationUIClosedResult result) {
  unified_consent::UnifiedConsentService* consent_service =
      UnifiedConsentServiceFactory::GetForProfile(profile_);

  switch (result) {
    case LoginUIService::CONFIGURE_SYNC_FIRST:
      if (consent_service)
        consent_service->SetUrlKeyedAnonymizedDataCollectionEnabled(true);
      delegate_->ShowSyncSettings();
      break;
    case LoginUIService::SYNC_WITH_DEFAULT_SETTINGS: {
      syncer::SyncService* sync_service = GetSyncService();
      if (sync_service)
        sync_service->GetUserSettings()->SetFirstSetupComplete(
            syncer::SyncFirstSetupCompleteSource::BASIC_FLOW);
      if (consent_service)
        consent_service->SetUrlKeyedAnonymizedDataCollectionEnabled(true);
      break;
    }
    case LoginUIService::ABORT_SYNC: {
      auto* primary_account_mutator =
          identity_manager_->GetPrimaryAccountMutator();
      DCHECK(primary_account_mutator);
      primary_account_mutator->RevokeSyncConsent(
          signin_metrics::ABORT_SIGNIN,
          signin_metrics::SignoutDelete::kIgnoreMetric);
      AbortAndDelete();
      return;
    }
    // No explicit action when the ui gets closed. If the embedder wants the
    // helper to abort sync in this case, it must redirect this action to
    // ABORT_SYNC. For UI_CLOSED, also no final callback is sent.
    case LoginUIService::UI_CLOSED:
      scoped_callback_runner_.ReplaceClosure(base::OnceClosure());
      break;
  }
  delete this;
}

void TurnSyncOnHelper::SwitchToProfile(Profile* new_profile) {
  DCHECK(!sync_blocker_);
  DCHECK(!sync_startup_tracker_);

  policy::UserPolicySigninServiceFactory::GetForProfile(profile_)
      ->ShutdownUserCloudPolicyManager();
  SetCurrentTurnSyncOnHelper(profile_, nullptr);  // Detach from old profile
  profile_ = new_profile;
  AttachToProfile();

  identity_manager_ = IdentityManagerFactory::GetForProfile(profile_);
  shutdown_subscription_ =
      TurnSyncOnHelperShutdownNotifierFactory::GetInstance()
          ->Get(profile_)
          ->Subscribe(base::BindOnce(&TurnSyncOnHelper::AbortAndDelete,
                                     base::Unretained(this)));
  delegate_->SwitchToProfile(new_profile);
}

void TurnSyncOnHelper::AttachToProfile() {
  // Delete any current helper.
  TurnSyncOnHelper* current_helper = GetCurrentTurnSyncOnHelper(profile_);
  if (current_helper) {
    // If the existing flow was using the same account, keep the account.
    if (current_helper->account_info_.account_id == account_info_.account_id)
      current_helper->signin_aborted_mode_ = SigninAbortedMode::KEEP_ACCOUNT;
    policy::UserPolicySigninServiceFactory::GetForProfile(profile_)
        ->ShutdownUserCloudPolicyManager();
    current_helper->AbortAndDelete();
  }
  DCHECK(!GetCurrentTurnSyncOnHelper(profile_));

  // Set this as the current helper.
  SetCurrentTurnSyncOnHelper(profile_, this);
}

void TurnSyncOnHelper::AbortAndDelete() {
  if (signin_aborted_mode_ == SigninAbortedMode::REMOVE_ACCOUNT) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    // TODO(https://crbug.com/1260291): Implement or make sure this is
    // unreachable on lacros.
    NOTIMPLEMENTED() << "REMOVE_ACCOUNT mode is not supported on lacros";
#endif
    policy::UserPolicySigninServiceFactory::GetForProfile(profile_)
        ->ShutdownUserCloudPolicyManager();
    // Revoke the token, and the AccountReconcilor and/or the Gaia server will
    // take care of invalidating the cookies.
    auto* accounts_mutator = identity_manager_->GetAccountsMutator();
    accounts_mutator->RemoveAccount(
        account_info_.account_id,
        signin_metrics::SourceForRefreshTokenOperation::
            kTurnOnSyncHelper_Abort);
  }

  delete this;
}
