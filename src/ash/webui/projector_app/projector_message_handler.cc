// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/projector_app/projector_message_handler.h"

#include <memory>
#include <string>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/projector/projector_controller.h"
#include "ash/webui/projector_app/projector_app_client.h"
#include "base/bind.h"
#include "base/check.h"
#include "base/json/values_util.h"
#include "base/time/time.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "content/public/browser/web_ui.h"
#include "url/gurl.h"

namespace ash {

namespace {

// Response keys.
constexpr char kUserName[] = "name";
constexpr char kUserEmail[] = "email";
constexpr char kUserPictureURL[] = "pictureURL";
constexpr char kIsPrimaryUser[] = "isPrimaryUser";
constexpr char kToken[] = "token";
constexpr char kExpirationTime[] = "expirationTime";
constexpr char kError[] = "error";
constexpr char kOAuthTokenInfo[] = "oauthTokenInfo";
constexpr char kXhrSuccess[] = "success";
constexpr char kXhrResponseBody[] = "response";
constexpr char kXhrError[] = "error";

// Used when a request is rejected.
constexpr char kRejectedRequestMessage[] = "Request Rejected";
constexpr char kRejectedRequestMessageKey[] = "message";
constexpr char kRejectedRequestArgsKey[] = "requestArgs";

// Projector Error Strings.
constexpr char kNoneStr[] = "NONE";
constexpr char kOtherStr[] = "OTHER";
constexpr char kTokenFetchFailureStr[] = "TOKEN_FETCH_FAILURE";

// Projector NewScreencastPreconditionState keys.
constexpr char kNewScreencastPreconditionState[] = "state";
constexpr char kNewScreencastPreconditionReasons[] = "reasons";

// Struct used to describe args to set user's preference.
struct SetUserPrefArgs {
  std::string pref_name;
  base::Value value;
};

base::Value AccessTokenInfoToValue(const signin::AccessTokenInfo& info) {
  base::Value value(base::Value::Type::DICTIONARY);
  value.SetKey(kToken, base::Value(info.token));
  value.SetKey(kExpirationTime, base::TimeToValue(info.expiration_time));
  return value;
}

std::string ProjectorErrorToString(ProjectorError mode) {
  switch (mode) {
    case ProjectorError::kNone:
      return kNoneStr;
    case ProjectorError::kTokenFetchFailure:
      return kTokenFetchFailureStr;
    case ProjectorError::kOther:
      return kOtherStr;
  }
}

base::Value ScreencastListToValue(
    const std::set<PendingScreencast>& screencasts) {
  std::vector<base::Value> value;
  value.reserve(screencasts.size());
  for (const auto& item : screencasts)
    value.push_back(item.ToValue());

  return base::Value(std::move(value));
}

bool IsUserPrefSupported(const std::string& pref) {
  return pref == ash::prefs::kProjectorCreationFlowEnabled ||
         pref == ash::prefs::kProjectorGalleryOnboardingShowCount ||
         pref == ash::prefs::kProjectorViewerOnboardingShowCount;
}

bool IsValidOnboardingPref(const SetUserPrefArgs& args) {
  return args.value.is_int() &&
         (args.pref_name == ash::prefs::kProjectorGalleryOnboardingShowCount ||
          args.pref_name == ash::prefs::kProjectorViewerOnboardingShowCount);
}

bool IsValidCreationFlowPref(const SetUserPrefArgs& args) {
  return args.value.is_bool() &&
         args.pref_name == ash::prefs::kProjectorCreationFlowEnabled;
}

bool IsValidPrefValueArg(const SetUserPrefArgs& args) {
  return IsValidCreationFlowPref(args) || IsValidOnboardingPref(args);
}

// Returns true if the request, `args`, contains a valid user preference string.
// The `out` string is only valid if the function returns true.
bool GetUserPrefName(const base::Value& args, std::string* out) {
  if (!args.is_list())
    return false;

  const auto& args_list = args.GetList();

  if (args_list.size() != 1 || !args_list[0].is_string())
    return false;

  *out = args_list[0].GetString();

  return IsUserPrefSupported(*out);
}

// Returns true if the request, `args`, is valid and supported.
// The `out` struct is only valid if the function returns true.
bool GetSetUserPrefArgs(const base::Value& args, SetUserPrefArgs* out) {
  if (!args.is_list())
    return false;

  const auto& args_list = args.GetList();

  if (args_list.size() != 2 || !args_list[0].is_string()) {
    return false;
  }

  out->pref_name = args_list[0].GetString();
  out->value = args_list[1].Clone();
  return IsValidPrefValueArg(*out);
}

base::Value CreateRejectMessageForArgs(const base::Value& value) {
  base::Value rejected_response(base::Value::Type::DICTIONARY);
  rejected_response.SetKey(kRejectedRequestMessageKey,
                           base::Value(kRejectedRequestMessage));
  rejected_response.SetKey(kRejectedRequestArgsKey, value.Clone());
  return rejected_response;
}

base::Value GetNewScreencastPreconditionValue(bool can_start) {
  // TODO(b/204233075): Provide Hidden state if device doesn't support on-device
  // speech recognition.
  auto state = can_start ? NewScreencastPreconditionState::kEnabled
                         : NewScreencastPreconditionState::kDisabled;
  base::Value response(base::Value::Type::DICTIONARY);
  response.SetIntKey(kNewScreencastPreconditionState, static_cast<int>(state));

  base::Value reasons(base::Value::Type::LIST);

  if (!can_start) {
    // TODO(b/204233075): Provide more fine grained than kOthers.
    reasons.Append(static_cast<int>(NewScreencastPreconditionReason::kOthers));
  }

  response.SetKey(kNewScreencastPreconditionReasons, std::move(reasons));
  return response;
}

}  // namespace

ProjectorMessageHandler::ProjectorMessageHandler(PrefService* pref_service)
    : content::WebUIMessageHandler(),
      xhr_sender_(std::make_unique<ProjectorXhrSender>(
          ProjectorAppClient::Get()->GetUrlLoaderFactory())),
      pref_service_(pref_service) {
  ProjectorAppClient::Get()->AddObserver(this);
}

ProjectorMessageHandler::~ProjectorMessageHandler() {
  ProjectorAppClient::Get()->RemoveObserver(this);
}

base::WeakPtr<ProjectorMessageHandler> ProjectorMessageHandler::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void ProjectorMessageHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getAccounts", base::BindRepeating(&ProjectorMessageHandler::GetAccounts,
                                         base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "getNewScreencastPreconditionState",
      base::BindRepeating(
          &ProjectorMessageHandler::GetNewScreencastPrecondition,
          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "startProjectorSession",
      base::BindRepeating(&ProjectorMessageHandler::StartProjectorSession,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "getOAuthTokenForAccount",
      base::BindRepeating(&ProjectorMessageHandler::GetOAuthTokenForAccount,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "onError", base::BindRepeating(&ProjectorMessageHandler::OnError,
                                     base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "sendXhr", base::BindRepeating(&ProjectorMessageHandler::SendXhr,
                                     base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "shouldDownloadSoda",
      base::BindRepeating(&ProjectorMessageHandler::ShouldDownloadSoda,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "installSoda", base::BindRepeating(&ProjectorMessageHandler::InstallSoda,
                                         base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getPendingScreencasts",
      base::BindRepeating(&ProjectorMessageHandler::GetPendingScreencasts,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getUserPref", base::BindRepeating(&ProjectorMessageHandler::GetUserPref,
                                         base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setUserPref", base::BindRepeating(&ProjectorMessageHandler::SetUserPref,
                                         base::Unretained(this)));
}

void ProjectorMessageHandler::OnScreencastsPendingStatusChanged(
    const std::set<PendingScreencast>& pending_screencast) {
  AllowJavascript();
  FireWebUIListener("onScreencastsStateChange",
                    ScreencastListToValue(pending_screencast));
}

void ProjectorMessageHandler::OnSodaProgress(int combined_progress) {
  AllowJavascript();
  FireWebUIListener("onSodaInstallProgressUpdated",
                    base::Value(combined_progress));
}

void ProjectorMessageHandler::OnSodaError() {
  AllowJavascript();
  FireWebUIListener("onSodaInstallError");
}

void ProjectorMessageHandler::OnSodaInstalled() {
  AllowJavascript();
  FireWebUIListener("onSodaInstalled");
}

void ProjectorMessageHandler::OnNewScreencastPreconditionChanged(
    bool can_start) {
  AllowJavascript();
  FireWebUIListener("onNewScreencastPreconditionChanged",
                    GetNewScreencastPreconditionValue(can_start));
}

void ProjectorMessageHandler::GetAccounts(base::Value::ConstListView args) {
  AllowJavascript();

  // Check that there is only one argument which is the callback id.
  DCHECK_EQ(args.size(), 1u);
  auto* controller = ProjectorController::Get();
  DCHECK(controller);

  const std::vector<AccountInfo> accounts = oauth_token_fetcher_.GetAccounts();
  const CoreAccountInfo primary_account =
      oauth_token_fetcher_.GetPrimaryAccountInfo();

  std::vector<base::Value> response;
  response.reserve(accounts.size());
  for (const auto& info : accounts) {
    base::Value account_info(base::Value::Type::DICTIONARY);
    account_info.SetKey(kUserName, base::Value(info.full_name));
    account_info.SetKey(kUserEmail, base::Value(info.email));
    account_info.SetKey(kUserPictureURL, base::Value(info.picture_url));
    account_info.SetKey(kIsPrimaryUser,
                        base::Value(info.gaia == primary_account.gaia));
    response.push_back(std::move(account_info));
  }

  ResolveJavascriptCallback(args[0], base::Value(std::move(response)));
}

void ProjectorMessageHandler::GetNewScreencastPrecondition(
    base::Value::ConstListView args) {
  AllowJavascript();

  // Check that there is only one argument which is the callback id.
  DCHECK_EQ(args.size(), 1u);

  ResolveJavascriptCallback(
      args[0], GetNewScreencastPreconditionValue(
                   ProjectorController::Get()->CanStartNewSession()));
}

void ProjectorMessageHandler::StartProjectorSession(
    base::Value::ConstListView args) {
  AllowJavascript();

  // There are two arguments. The first is the callback and the second is a list
  // containing the account which we need to start the recording with.
  DCHECK_EQ(args.size(), 2u);

  const auto& func_args = args[1];
  DCHECK(func_args.is_list());

  // The first entry is the drive directory to save the screen cast to.
  // TODO(b/177959166): Pass the directory to ProjectorController when starting
  // a new session.
  DCHECK_EQ(func_args.GetList().size(), 1u);

  // TODO(b/195113693): Start the projector session with the selected account
  // and folder.
  auto* controller = ProjectorController::Get();
  if (!controller->CanStartNewSession()) {
    ResolveJavascriptCallback(args[0], base::Value(false));
    return;
  }

  controller->StartProjectorSession(func_args.GetList()[0].GetString());
  ResolveJavascriptCallback(args[0], base::Value(true));
}

void ProjectorMessageHandler::GetOAuthTokenForAccount(
    const base::Value::ConstListView args) {
  // Two arguments. The first is callback id, and the second is the list
  // containing the account for which to fetch the oauth token.
  DCHECK_EQ(args.size(), 2u);

  const auto& requested_account = args[1];
  DCHECK(requested_account.is_list());
  DCHECK_EQ(requested_account.GetList().size(), 1u);

  auto& oauth_token_fetch_callback = args[0].GetString();
  const std::string& email = requested_account.GetList()[0].GetString();

  oauth_token_fetcher_.GetAccessTokenFor(
      email,
      base::BindOnce(&ProjectorMessageHandler::OnAccessTokenRequestCompleted,
                     GetWeakPtr(), oauth_token_fetch_callback));
}

void ProjectorMessageHandler::SendXhr(const base::Value::ConstListView args) {
  // Two arguments. The first is callback id, and the second is the list
  // containing function arguments for making the request.
  DCHECK_EQ(args.size(), 2u);
  const auto& callback_id = args[0].GetString();

  const auto& func_args = args[1].GetList();
  // Four function arguments:
  // 1. The request URL.
  // 2. The request method, for example: GET
  // 3. The request body data.
  // 4. A bool to indicate whether or not to use end user credential to
  // authorize the request.
  DCHECK_EQ(func_args.size(), 4u);

  const auto& url = func_args[0].GetString();
  const auto& method = func_args[1].GetString();
  std::string request_body = func_args[2].GetString();
  bool use_credentials = func_args[3].GetBool();
  DCHECK(!url.empty());
  DCHECK(!method.empty());

  xhr_sender_->Send(
      GURL(url), method, request_body, use_credentials,
      base::BindOnce(&ProjectorMessageHandler::OnXhrRequestCompleted,
                     GetWeakPtr(), callback_id));
}

void ProjectorMessageHandler::ShouldDownloadSoda(
    const base::Value::ConstListView args) {
  AllowJavascript();

  // The device should be eligible to download SODA and SODA should not have
  // already been downloaded on the device.
  ResolveJavascriptCallback(
      args[0], base::Value(ProjectorAppClient::Get()->ShouldDownloadSoda()));
}

void ProjectorMessageHandler::InstallSoda(
    const base::Value::ConstListView args) {
  AllowJavascript();
  ProjectorAppClient::Get()->InstallSoda();
  ResolveJavascriptCallback(args[0], base::Value(true));
}

void ProjectorMessageHandler::OnError(const base::Value::ConstListView args) {
  // TODO(b/195113693): Get the SWA dialog associated with this WebUI and close
  // it.
}

void ProjectorMessageHandler::GetUserPref(
    const base::Value::ConstListView args) {
  AllowJavascript();

  std::string user_pref;
  if (!GetUserPrefName(args[1], &user_pref)) {
    RejectJavascriptCallback(args[0], CreateRejectMessageForArgs(args[1]));
    return;
  }

  ResolveJavascriptCallback(args[0], *(pref_service_->Get(user_pref)));
}

void ProjectorMessageHandler::SetUserPref(
    const base::Value::ConstListView args) {
  AllowJavascript();
  SetUserPrefArgs parsed_args;
  if (!GetSetUserPrefArgs(args[1], &parsed_args)) {
    RejectJavascriptCallback(args[0], CreateRejectMessageForArgs(args[1]));
    return;
  }

  pref_service_->Set(parsed_args.pref_name, parsed_args.value);
  ResolveJavascriptCallback(args[0], base::Value());
}

void ProjectorMessageHandler::OnAccessTokenRequestCompleted(
    const std::string& js_callback_id,
    const std::string& email,
    GoogleServiceAuthError error,
    const signin::AccessTokenInfo& info) {
  AllowJavascript();

  base::Value response(base::Value::Type::DICTIONARY);
  response.SetKey(kUserEmail, base::Value(email));
  if (error.state() != GoogleServiceAuthError::State::NONE) {
    response.SetKey(kOAuthTokenInfo, base::Value());
    response.SetKey(kError, base::Value(ProjectorErrorToString(
                                ProjectorError::kTokenFetchFailure)));
  } else {
    response.SetKey(kError,
                    base::Value(ProjectorErrorToString(ProjectorError::kNone)));
    response.SetKey(kOAuthTokenInfo, AccessTokenInfoToValue(info));
  }

  ResolveJavascriptCallback(base::Value(js_callback_id), std::move(response));
}

void ProjectorMessageHandler::OnXhrRequestCompleted(
    const std::string& js_callback_id,
    bool success,
    const std::string& response_body,
    const std::string& error) {
  AllowJavascript();

  base::Value response(base::Value::Type::DICTIONARY);
  response.SetBoolKey(kXhrSuccess, success);
  response.SetStringKey(kXhrResponseBody, response_body);
  response.SetStringKey(kXhrError, error);

  ResolveJavascriptCallback(base::Value(js_callback_id), std::move(response));
}

void ProjectorMessageHandler::GetPendingScreencasts(
    const base::Value::ConstListView args) {
  AllowJavascript();
  // Check that there is only one argument which is the callback id.
  DCHECK_EQ(args.size(), 1u);

  const std::set<PendingScreencast>& pending_screencasts =
      ProjectorAppClient::Get()->GetPendingScreencasts();
  ResolveJavascriptCallback(args[0],
                            ScreencastListToValue(pending_screencasts));
}

}  // namespace ash
