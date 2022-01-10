// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/consolidated_consent_screen.h"

#include "ash/components/arc/arc_prefs.h"
#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/hash/sha1.h"
#include "base/i18n/timezone.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/optin/arc_optin_preference_handler.h"
#include "chrome/browser/ash/login/demo_mode/demo_setup_controller.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/consent_auditor/consent_auditor_factory.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/consent_auditor/consent_auditor.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/user_manager/user_manager.h"

using ArcBackupAndRestoreConsent =
    sync_pb::UserConsentTypes::ArcBackupAndRestoreConsent;
using ArcGoogleLocationServiceConsent =
    sync_pb::UserConsentTypes::ArcGoogleLocationServiceConsent;
using ArcPlayTermsOfServiceConsent =
    sync_pb::UserConsentTypes::ArcPlayTermsOfServiceConsent;
using sync_pb::UserConsentTypes;

namespace ash {
namespace {
constexpr const char kBackDemoButtonClicked[] = "back";

std::string GetGoogleEulaOnlineUrl() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kOobeEulaUrlForTests)) {
    return base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
        switches::kOobeEulaUrlForTests);
  }

  return base::StringPrintf(chrome::kGoogleEulaOnlineURLPath,
                            g_browser_process->GetApplicationLocale().c_str());
}

std::string GetCrosEulaOnlineUrl() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kOobeEulaUrlForTests)) {
    return base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
        switches::kOobeEulaUrlForTests);
  }

  return base::StringPrintf(chrome::kCrosEulaOnlineURLPath,
                            g_browser_process->GetApplicationLocale().c_str());
}
}  // namespace

std::string ConsolidatedConsentScreen::GetResultString(Result result) {
  switch (result) {
    case Result::ACCEPTED:
    case Result::ACCEPTED_DEMO_ONLINE:
    case Result::ACCEPTED_DEMO_OFFLINE:
      return "Accepted";
    case Result::BACK_DEMO:
      return "Back";
    case Result::NOT_APPLICABLE:
      return BaseScreen::kNotApplicable;
  }
}

ConsolidatedConsentScreen::ConsolidatedConsentScreen(
    ConsolidatedConsentScreenView* view,
    const ScreenExitCallback& exit_callback)
    : BaseScreen(ConsolidatedConsentScreenView::kScreenId,
                 OobeScreenPriority::DEFAULT),
      view_(view),
      exit_callback_(exit_callback) {
  DCHECK(view_);
  if (view_)
    view_->Bind(this);
}

ConsolidatedConsentScreen::~ConsolidatedConsentScreen() {
  if (view_) {
    view_->Unbind();
  }

  for (auto& observer : observer_list_)
    observer.OnConsolidatedConsentScreenDestroyed();
}

void ConsolidatedConsentScreen::OnViewDestroyed(
    ConsolidatedConsentScreenView* view) {
  if (view_ == view)
    view_ = nullptr;
}

bool ConsolidatedConsentScreen::MaybeSkip(WizardContext* context) {
  if (arc::IsArcDemoModeSetupFlow())
    return false;

  // For managed users, admins are required to accept ToS on the server side.
  // So, if the user is managed and no arc negotiation is needed, skip the
  // screen. IsManaged() returns true for child users, don't skip consolidated
  // consent in that case.
  Profile* profile = ProfileManager::GetActiveUserProfile();
  CHECK(profile);
  bool is_child_account =
      user_manager::UserManager::Get()->IsLoggedInAsChildUser();
  bool is_enterprise_managed =
      profile->GetProfilePolicyConnector()->IsManaged() && !is_child_account;
  if ((is_enterprise_managed &&
       !arc::IsArcTermsOfServiceOobeNegotiationNeeded()) ||
      !context->is_branded_build) {
    exit_callback_.Run(Result::NOT_APPLICABLE);
    return true;
  }
  return false;
}

void ConsolidatedConsentScreen::ShowImpl() {
  if (!view_)
    return;

  is_child_account_ = user_manager::UserManager::Get()->IsLoggedInAsChildUser();

  Profile* profile = ProfileManager::GetActiveUserProfile();
  CHECK(profile);
  is_enterprise_managed_account_ =
      profile->GetProfilePolicyConnector()->IsManaged() && !is_child_account_;

  bool is_demo = arc::IsArcDemoModeSetupFlow();
  bool is_arc_enabled = arc::IsArcTermsOfServiceOobeNegotiationNeeded();
  if (!is_demo && is_arc_enabled) {
    // Enable ARC to match ArcSessionManager logic. ArcSessionManager expects
    // that ARC is enabled (prefs::kArcEnabled = true) on showing Terms of
    // Service. If user accepts ToS then prefs::kArcEnabled is left activated.
    // If user skips ToS then prefs::kArcEnabled is automatically reset in
    // ArcSessionManager.
    arc::SetArcPlayStoreEnabledForProfile(profile, true);

    pref_handler_ = std::make_unique<arc::ArcOptInPreferenceHandler>(
        this, profile->GetPrefs());
    pref_handler_->Start();
  }

  ConsolidatedConsentScreenView::ScreenConfig config;
  config.is_arc_enabled = is_arc_enabled;
  config.is_demo = is_demo;
  config.is_enterprise_managed_account = is_enterprise_managed_account_;
  config.is_child_account = is_child_account_;
  config.country_code = base::CountryCodeForCurrentTimezone();
  config.google_eula_url = GetGoogleEulaOnlineUrl();
  config.cros_eula_url = GetCrosEulaOnlineUrl();
  view_->Show(config);
}

void ConsolidatedConsentScreen::HideImpl() {
  pref_handler_.reset();
}

void ConsolidatedConsentScreen::OnUserAction(const std::string& action_id) {
  if (action_id == kBackDemoButtonClicked)
    exit_callback_.Run(Result::BACK_DEMO);
  else
    BaseScreen::OnUserAction(action_id);
}

void ConsolidatedConsentScreen::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void ConsolidatedConsentScreen::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void ConsolidatedConsentScreen::OnMetricsModeChanged(bool enabled,
                                                     bool managed) {
  if (view_)
    view_->SetUsageMode(enabled, managed);
}

void ConsolidatedConsentScreen::OnBackupAndRestoreModeChanged(bool enabled,
                                                              bool managed) {
  backup_restore_managed_ = managed;
  if (view_)
    view_->SetBackupMode(enabled, managed);
}

void ConsolidatedConsentScreen::OnLocationServicesModeChanged(bool enabled,
                                                              bool managed) {
  location_services_managed_ = managed;
  if (view_)
    view_->SetLocationMode(enabled, managed);
}

void ConsolidatedConsentScreen::RecordConsents(
    const ConsentsParameters& params) {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  consent_auditor::ConsentAuditor* consent_auditor =
      ConsentAuditorFactory::GetForProfile(profile);
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
  // The account may or may not have consented to browser sync.
  DCHECK(identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  const CoreAccountId account_id =
      identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSignin);

  ArcPlayTermsOfServiceConsent play_consent;
  play_consent.set_status(UserConsentTypes::GIVEN);
  play_consent.set_confirmation_grd_id(
      IDS_CONSOLIDATED_CONSENT_ACCEPT_AND_CONTINUE);
  play_consent.set_consent_flow(ArcPlayTermsOfServiceConsent::SETUP);
  if (params.record_arc_tos_consent) {
    play_consent.set_confirmation_grd_id(
        IDS_CONSOLIDATED_CONSENT_ACCEPT_AND_CONTINUE);
    play_consent.set_play_terms_of_service_text_length(
        params.tos_content.length());
    play_consent.set_play_terms_of_service_hash(
        base::SHA1HashString(params.tos_content));
  }
  consent_auditor->RecordArcPlayConsent(account_id, play_consent);

  if (params.record_backup_consent) {
    ArcBackupAndRestoreConsent backup_and_restore_consent;
    backup_and_restore_consent.set_confirmation_grd_id(
        IDS_CONSOLIDATED_CONSENT_ACCEPT_AND_CONTINUE);
    backup_and_restore_consent.add_description_grd_ids(
        IDS_CONSOLIDATED_CONSENT_BACKUP_OPT_IN_TITLE);
    backup_and_restore_consent.add_description_grd_ids(
        is_child_account_ ? IDS_CONSOLIDATED_CONSENT_BACKUP_OPT_IN_CHILD
                          : IDS_CONSOLIDATED_CONSENT_BACKUP_OPT_IN);
    backup_and_restore_consent.set_status(params.backup_accepted
                                              ? UserConsentTypes::GIVEN
                                              : UserConsentTypes::NOT_GIVEN);

    consent_auditor->RecordArcBackupAndRestoreConsent(
        account_id, backup_and_restore_consent);
  }

  if (params.record_location_consent) {
    ArcGoogleLocationServiceConsent location_service_consent;
    location_service_consent.set_confirmation_grd_id(
        IDS_CONSOLIDATED_CONSENT_ACCEPT_AND_CONTINUE);
    location_service_consent.add_description_grd_ids(
        IDS_CONSOLIDATED_CONSENT_LOCATION_OPT_IN_TITLE);
    location_service_consent.add_description_grd_ids(
        is_child_account_ ? IDS_CONSOLIDATED_CONSENT_LOCATION_OPT_IN_CHILD
                          : IDS_CONSOLIDATED_CONSENT_LOCATION_OPT_IN);
    location_service_consent.set_status(params.location_accepted
                                            ? UserConsentTypes::GIVEN
                                            : UserConsentTypes::NOT_GIVEN);

    consent_auditor->RecordArcGoogleLocationServiceConsent(
        account_id, location_service_consent);
  }
}

void ConsolidatedConsentScreen::OnAccept(bool enable_stats_usage,
                                         bool enable_backup_restore,
                                         bool enable_location_services,
                                         const std::string& tos_content) {
  // TODO: Handle usage stats reporting for current user

  if (arc::IsArcDemoModeSetupFlow() ||
      !arc::IsArcTermsOfServiceOobeNegotiationNeeded()) {
    for (auto& observer : observer_list_)
      observer.OnConsolidatedConsentAccept();

    ExitScreenWithAcceptedResult();
    return;
  }

  pref_handler_->EnableBackupRestore(enable_backup_restore);
  pref_handler_->EnableLocationService(enable_location_services);

  ConsentsParameters consents;
  consents.tos_content = tos_content;
  consents.record_arc_tos_consent = !is_enterprise_managed_account_;
  consents.record_backup_consent = !backup_restore_managed_;
  consents.backup_accepted = enable_backup_restore;
  consents.record_location_consent = !location_services_managed_;
  consents.location_accepted = enable_location_services;
  RecordConsents(consents);

  for (auto& observer : observer_list_)
    observer.OnConsolidatedConsentAccept();

  ExitScreenWithAcceptedResult();
}

void ConsolidatedConsentScreen::ExitScreenWithAcceptedResult() {
  StartupUtils::MarkEulaAccepted();

  const DemoSetupController* const demo_setup_controller =
      WizardController::default_controller()->demo_setup_controller();

  if (!demo_setup_controller) {
    exit_callback_.Run(Result::ACCEPTED);
    return;
  }
  if (demo_setup_controller->IsOfflineEnrollment())
    exit_callback_.Run(Result::ACCEPTED_DEMO_OFFLINE);
  else
    exit_callback_.Run(Result::ACCEPTED_DEMO_ONLINE);
}
}  // namespace ash
