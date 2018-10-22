// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/sync_confirmation_handler.h"

#include <vector>

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/consent_auditor/consent_auditor_factory.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/signin_view_controller_delegate.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "chrome/browser/ui/webui/signin/sync_confirmation_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "components/consent_auditor/consent_auditor.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/avatar_icon_util.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/unified_consent/feature.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "url/gurl.h"

namespace {
// Used for UMA. Do not reorder, append new values at the end.
enum class UnifiedConsentBumpAction {
  kOptIn = 0,
  kMoreOptionsOptIn = 1,
  kMoreOptionsSettings = 2,
  kMoreOptionsNoChanges = 3,
  kAbort = 4,

  kMaxValue = kAbort
};
}  // namespace

const int kProfileImageSize = 128;

SyncConfirmationHandler::SyncConfirmationHandler(
    Browser* browser,
    const std::unordered_map<std::string, int>& string_to_grd_id_map,
    consent_auditor::Feature consent_feature)
    : profile_(browser->profile()),
      browser_(browser),
      did_user_explicitly_interact(false),
      string_to_grd_id_map_(string_to_grd_id_map),
      consent_feature_(consent_feature) {
  DCHECK(profile_);
  DCHECK(browser_);
  BrowserList::AddObserver(this);
}

SyncConfirmationHandler::~SyncConfirmationHandler() {
  BrowserList::RemoveObserver(this);
  AccountTrackerServiceFactory::GetForProfile(profile_)->RemoveObserver(this);

  // Abort signin and prevent sync from starting if none of the actions on the
  // sync confirmation dialog are taken by the user.
  if (!did_user_explicitly_interact) {
    HandleUndo(nullptr);
    if (IsUnifiedConsentBumpDialog()) {
      UMA_HISTOGRAM_ENUMERATION("UnifiedConsent.ConsentBump.Action",
                                UnifiedConsentBumpAction::kAbort);
    } else {
      base::RecordAction(base::UserMetricsAction("Signin_Abort_Signin"));
    }
  }
}

void SyncConfirmationHandler::OnBrowserRemoved(Browser* browser) {
  if (browser_ == browser)
    browser_ = nullptr;
}

void SyncConfirmationHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "confirm", base::BindRepeating(&SyncConfirmationHandler::HandleConfirm,
                                     base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "undo", base::BindRepeating(&SyncConfirmationHandler::HandleUndo,
                                  base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "goToSettings",
      base::BindRepeating(&SyncConfirmationHandler::HandleGoToSettings,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "initializedWithSize",
      base::BindRepeating(&SyncConfirmationHandler::HandleInitializedWithSize,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "accountImageRequest",
      base::BindRepeating(&SyncConfirmationHandler::HandleAccountImageRequest,
                          base::Unretained(this)));
}

void SyncConfirmationHandler::HandleConfirm(const base::ListValue* args) {
  did_user_explicitly_interact = true;
  RecordConsent(args);
  CloseModalSigninWindow(LoginUIService::SYNC_WITH_DEFAULT_SETTINGS);
}

void SyncConfirmationHandler::HandleGoToSettings(const base::ListValue* args) {
  did_user_explicitly_interact = true;
  RecordConsent(args);
  CloseModalSigninWindow(LoginUIService::CONFIGURE_SYNC_FIRST);
}

void SyncConfirmationHandler::HandleUndo(const base::ListValue* args) {
  did_user_explicitly_interact = true;
  CloseModalSigninWindow(LoginUIService::ABORT_SIGNIN);
}

void SyncConfirmationHandler::HandleAccountImageRequest(
    const base::ListValue* args) {
  std::string account_id = SigninManagerFactory::GetForProfile(profile_)
                               ->GetAuthenticatedAccountId();
  AccountInfo account_info =
      AccountTrackerServiceFactory::GetForProfile(profile_)->GetAccountInfo(
          account_id);

  // Fire the "account-image-changed" listener from |SetUserImageURL()|.
  // Note: If the account info is not available yet in the
  // AccountTrackerService, i.e. account_info is empty, the listener will be
  // fired again through |OnAccountUpdated()|.
  SetUserImageURL(account_info.picture_url);
}

void SyncConfirmationHandler::RecordConsent(const base::ListValue* args) {
  CHECK_EQ(2U, args->GetSize());
  const std::vector<base::Value>& consent_description =
      args->GetList()[0].GetList();
  const std::string& consent_confirmation = args->GetList()[1].GetString();

  std::vector<int> consent_text_ids;

  // The strings returned by the WebUI are not free-form, they must belong into
  // a pre-determined set of strings (stored in |string_to_grd_id_map_|). As
  // this has privacy and legal implications, CHECK the integrity of the strings
  // received from the renderer process before recording the consent.
  for (const base::Value& text : consent_description) {
    auto iter = string_to_grd_id_map_.find(text.GetString());
    CHECK(iter != string_to_grd_id_map_.end()) << "Unexpected string:\n"
                                               << text.GetString();
    consent_text_ids.push_back(iter->second);
  }

  auto iter = string_to_grd_id_map_.find(consent_confirmation);
  CHECK(iter != string_to_grd_id_map_.end()) << "Unexpected string:\n"
                                             << consent_confirmation;
  int consent_confirmation_id = iter->second;

  consent_auditor::ConsentAuditor* consent_auditor =
      ConsentAuditorFactory::GetForProfile(profile_);
  const std::string& account_id = SigninManagerFactory::GetForProfile(profile_)
                                      ->GetAuthenticatedAccountId();
  // TODO(markusheintz): Use a bool unified_consent_enabled instead of a
  // consent_auditor::Feature type variable.
  if (consent_feature_ == consent_auditor::Feature::CHROME_UNIFIED_CONSENT) {
    sync_pb::UserConsentTypes::UnifiedConsent unified_consent;
    unified_consent.set_confirmation_grd_id(consent_confirmation_id);
    for (int id : consent_text_ids) {
      unified_consent.add_description_grd_ids(id);
    }
    unified_consent.set_status(sync_pb::UserConsentTypes::ConsentStatus::
                                   UserConsentTypes_ConsentStatus_GIVEN);
    consent_auditor->RecordUnifiedConsent(account_id, unified_consent);
  } else {
    sync_pb::UserConsentTypes::SyncConsent sync_consent;
    sync_consent.set_confirmation_grd_id(consent_confirmation_id);
    for (int id : consent_text_ids) {
      sync_consent.add_description_grd_ids(id);
    }
    sync_consent.set_status(sync_pb::UserConsentTypes::ConsentStatus::
                                UserConsentTypes_ConsentStatus_GIVEN);
    consent_auditor->RecordSyncConsent(account_id, sync_consent);
  }
}

void SyncConfirmationHandler::SetUserImageURL(const std::string& picture_url) {
  std::string picture_url_to_load;
  GURL picture_gurl(picture_url);
  if (picture_gurl.is_valid()) {
    picture_url_to_load =
        signin::GetAvatarImageURLWithOptions(picture_gurl, kProfileImageSize,
                                             false /* no_silhouette */)
            .spec();
  } else {
    // Use the placeholder avatar icon until the account picture URL is fetched.
    picture_url_to_load = profiles::GetPlaceholderAvatarIconUrl();
  }
  base::Value picture_url_value(picture_url_to_load);
  web_ui()->CallJavascriptFunctionUnsafe("sync.confirmation.setUserImageURL",
                                         picture_url_value);

  if (unified_consent::IsUnifiedConsentFeatureEnabled()) {
    AllowJavascript();
    FireWebUIListener("account-image-changed", picture_url_value);
  }
}

void SyncConfirmationHandler::OnAccountUpdated(const AccountInfo& info) {
  if (!info.IsValid())
    return;

  SigninManager* signin_manager = SigninManagerFactory::GetForProfile(profile_);
  if (info.account_id != signin_manager->GetAuthenticatedAccountId())
    return;

  AccountTrackerServiceFactory::GetForProfile(profile_)->RemoveObserver(this);
  SetUserImageURL(info.picture_url);
}

void SyncConfirmationHandler::CloseModalSigninWindow(
    LoginUIService::SyncConfirmationUIClosedResult result) {
  if (!IsUnifiedConsentBumpDialog()) {
    // Metrics for the unified consent bump are recorded directly from
    // javascript.
    switch (result) {
      case LoginUIService::CONFIGURE_SYNC_FIRST:
        base::RecordAction(
            base::UserMetricsAction("Signin_Signin_WithAdvancedSyncSettings"));
        break;
      case LoginUIService::SYNC_WITH_DEFAULT_SETTINGS:
        base::RecordAction(
            base::UserMetricsAction("Signin_Signin_WithDefaultSyncSettings"));
        break;
      case LoginUIService::ABORT_SIGNIN:
        base::RecordAction(base::UserMetricsAction("Signin_Undo_Signin"));
        break;
    }
  }
  LoginUIServiceFactory::GetForProfile(profile_)->SyncConfirmationUIClosed(
      result);
  if (browser_)
    browser_->signin_view_controller()->CloseModalSignin();
}

void SyncConfirmationHandler::HandleInitializedWithSize(
    const base::ListValue* args) {
  if (!browser_)
    return;

  std::string account_id = SigninManagerFactory::GetForProfile(profile_)
                               ->GetAuthenticatedAccountId();
  if (account_id.empty()) {
    // No account is signed in, so there is nothing to be displayed in the sync
    // confirmation dialog.
    return;
  }
  AccountTrackerService* account_tracker =
      AccountTrackerServiceFactory::GetForProfile(profile_);
  AccountInfo account_info = account_tracker->GetAccountInfo(account_id);

  if (!account_info.IsValid()) {
    SetUserImageURL(AccountTrackerService::kNoPictureURLFound);
    account_tracker->AddObserver(this);
  } else {
    SetUserImageURL(account_info.picture_url);
  }

  signin::SetInitializedModalHeight(browser_, web_ui(), args);

  // After the dialog is shown, some platforms might have an element focused.
  // To be consistent, clear the focused element on all platforms.
  // TODO(anthonyvd): Figure out why this is needed on Mac and not other
  // platforms and if there's a way to start unfocused while avoiding this
  // workaround.
  web_ui()->CallJavascriptFunctionUnsafe("sync.confirmation.clearFocus");
}

bool SyncConfirmationHandler::IsUnifiedConsentBumpDialog() {
  return web_ui()->GetWebContents()->GetVisibleURL() ==
         chrome::kChromeUISyncConsentBumpURL;
}
