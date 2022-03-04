// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/about_signin_internals.h"

#include <stddef.h>

#include <algorithm>
#include <tuple>

#include "base/command_line.h"
#include "base/hash/hash.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/time/time_to_iso8601.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/account_reconcilor.h"
#include "components/signin/public/base/signin_client.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/diagnostics_provider.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/load_credentials_state.h"
#include "net/base/backoff_entry.h"

namespace {

// The maximum number of the refresh token events. Only the last
// |kMaxRefreshTokenListSize| events are kept in memory.
const size_t kMaxRefreshTokenListSize = 50;

enum class GaiaCookiesState {
  kAllowed,
  kClearOnExit,
  kBlocked,
};

GaiaCookiesState GetGaiaCookiesState(SigninClient* signin_client) {
  bool signin_cookies_allowed = signin_client->AreSigninCookiesAllowed();
  if (!signin_cookies_allowed)
    return GaiaCookiesState::kBlocked;

  bool clear_cookies_on_exit = signin_client->AreSigninCookiesDeletedOnExit();
  if (clear_cookies_on_exit)
    return GaiaCookiesState::kClearOnExit;

  return GaiaCookiesState::kAllowed;
}

std::string GetGaiaCookiesStateAsString(const GaiaCookiesState state) {
  switch (state) {
    case GaiaCookiesState::kBlocked:
      return "Not allowed";
    case GaiaCookiesState::kClearOnExit:
      return "Cleared on exit";
    case GaiaCookiesState::kAllowed:
      return "Allowed";
  }
}

void AddSection(base::Value::ListStorage& parent_list,
                base::Value section_content,
                const std::string& title) {
  base::Value section(base::Value::Type::DICTIONARY);
  section.SetStringKey("title", title);
  section.SetKey("data", std::move(section_content));
  parent_list.push_back(std::move(section));
}

void AddSectionEntry(base::Value::ListStorage& section_list,
                     const std::string& field_name,
                     const std::string& field_status,
                     const std::string& field_time = "") {
  base::Value entry(base::Value::Type::DICTIONARY);
  entry.SetStringKey("label", field_name);
  entry.SetStringKey("status", field_status);
  entry.SetStringKey("time", field_time);
  section_list.push_back(std::move(entry));
}

void AddCookieEntry(base::Value::ListStorage& accounts_list,
                    const std::string& field_email,
                    const std::string& field_gaia_id,
                    const std::string& field_valid) {
  base::Value entry(base::Value::Type::DICTIONARY);
  entry.SetStringKey("email", field_email);
  entry.SetStringKey("gaia_id", field_gaia_id);
  entry.SetStringKey("valid", field_valid);
  accounts_list.push_back(std::move(entry));
}

std::string SigninStatusFieldToLabel(
    signin_internals_util::UntimedSigninStatusField field) {
  switch (field) {
    case signin_internals_util::ACCOUNT_ID:
      return "Account Id";
    case signin_internals_util::GAIA_ID:
      return "Gaia Id";
    case signin_internals_util::USERNAME:
      return "Username";
    case signin_internals_util::UNTIMED_FIELDS_END:
      NOTREACHED();
      return std::string();
  }
  NOTREACHED();
  return std::string();
}

std::string TokenServiceLoadCredentialsStateToLabel(
    signin::LoadCredentialsState state) {
  switch (state) {
    case signin::LoadCredentialsState::LOAD_CREDENTIALS_NOT_STARTED:
      return "Load credentials not started";
    case signin::LoadCredentialsState::LOAD_CREDENTIALS_IN_PROGRESS:
      return "Load credentials in progress";
    case signin::LoadCredentialsState::LOAD_CREDENTIALS_FINISHED_WITH_SUCCESS:
      return "Load credentials finished with success";
    case signin::LoadCredentialsState::
        LOAD_CREDENTIALS_FINISHED_WITH_DB_CANNOT_BE_OPENED:
      return "Load credentials failed with datase cannot be opened error";
    case signin::LoadCredentialsState::LOAD_CREDENTIALS_FINISHED_WITH_DB_ERRORS:
      return "Load credentials failed with database errors";
    case signin::LoadCredentialsState::
        LOAD_CREDENTIALS_FINISHED_WITH_DECRYPT_ERRORS:
      return "Load credentials failed with decrypt errors";
    case signin::LoadCredentialsState::
        LOAD_CREDENTIALS_FINISHED_WITH_NO_TOKEN_FOR_PRIMARY_ACCOUNT:
      return "Load credentials failed with no refresh token for signed in "
             "account";
    case signin::LoadCredentialsState::
        LOAD_CREDENTIALS_FINISHED_WITH_UNKNOWN_ERRORS:
      return "Load credentials failed with unknown errors";
  }
  NOTREACHED();
  return std::string();
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
std::string SigninStatusFieldToLabel(
    signin_internals_util::TimedSigninStatusField field) {
  switch (field) {
    case signin_internals_util::AUTHENTICATION_RESULT_RECEIVED:
      return "Gaia Authentication Result";
    case signin_internals_util::REFRESH_TOKEN_RECEIVED:
      return "RefreshToken Received";
    case signin_internals_util::TIMED_FIELDS_END:
      NOTREACHED();
      return "Error";
  }
  NOTREACHED();
  return "Error";
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

// It's quite unfortunate that |time| is saved in prefs as a string instead of
// base::Time because any change of the format would create inconsistency.
void SetPref(PrefService* prefs,
             signin_internals_util::TimedSigninStatusField field,
             const std::string& value,
             const std::string& time) {
  std::string value_pref = SigninStatusFieldToString(field) + ".value";
  std::string time_pref = SigninStatusFieldToString(field) + ".time";
  prefs->SetString(value_pref, value);
  prefs->SetString(time_pref, time);
}

void GetPref(PrefService* prefs,
             signin_internals_util::TimedSigninStatusField field,
             std::string* value,
             std::string* time) {
  std::string value_pref = SigninStatusFieldToString(field) + ".value";
  std::string time_pref = SigninStatusFieldToString(field) + ".time";
  *value = prefs->GetString(value_pref);
  *time = prefs->GetString(time_pref);
}

void ClearPref(PrefService* prefs,
               signin_internals_util::TimedSigninStatusField field) {
  std::string value_pref = SigninStatusFieldToString(field) + ".value";
  std::string time_pref = SigninStatusFieldToString(field) + ".time";
  prefs->ClearPref(value_pref);
  prefs->ClearPref(time_pref);
}

std::string GetAccountConsistencyDescription(
    signin::AccountConsistencyMethod method) {
  switch (method) {
    case signin::AccountConsistencyMethod::kDisabled:
      return "None";
    case signin::AccountConsistencyMethod::kMirror:
      return "Mirror";
    case signin::AccountConsistencyMethod::kDice:
      return "DICE";
  }
  NOTREACHED();
  return "";
}

std::string GetSigninStatusDescription(
    signin::IdentityManager* identity_manager) {
  if (!identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    return "Not Signed In";
  } else if (identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync)) {
    return "Signed In, Consented for Sync";
  } else {
    return "Signed In, Not Consented for Sync";
  }
}

}  // anonymous namespace

AboutSigninInternals::AboutSigninInternals(
    signin::IdentityManager* identity_manager,
    SigninErrorController* signin_error_controller,
    signin::AccountConsistencyMethod account_consistency,
    SigninClient* client,
    AccountReconcilor* account_reconcilor)
    : identity_manager_(identity_manager),
      client_(client),
      signin_error_controller_(signin_error_controller),
      account_reconcilor_(account_reconcilor),
      account_consistency_(account_consistency) {
  RefreshSigninPrefs();
  client_->AddContentSettingsObserver(this);
  signin_error_controller_->AddObserver(this);
  identity_manager_->AddObserver(this);
  identity_manager_->AddDiagnosticsObserver(this);
  account_reconcilor_->AddObserver(this);
}

AboutSigninInternals::~AboutSigninInternals() {}

signin_internals_util::UntimedSigninStatusField& operator++(
    signin_internals_util::UntimedSigninStatusField& field) {
  field =
      static_cast<signin_internals_util::UntimedSigninStatusField>(field + 1);
  return field;
}

signin_internals_util::TimedSigninStatusField& operator++(
    signin_internals_util::TimedSigninStatusField& field) {
  field = static_cast<signin_internals_util::TimedSigninStatusField>(field + 1);
  return field;
}

// static
void AboutSigninInternals::RegisterPrefs(PrefRegistrySimple* user_prefs) {
  // TODO(rogerta): leaving untimed fields here for now because legacy
  // profiles still have these prefs.  In three or four version from M43
  // we can probably remove them.
  for (signin_internals_util::UntimedSigninStatusField i =
           signin_internals_util::UNTIMED_FIELDS_BEGIN;
       i < signin_internals_util::UNTIMED_FIELDS_END; ++i) {
    const std::string pref_path = SigninStatusFieldToString(i);
    user_prefs->RegisterStringPref(pref_path, std::string());
  }

  for (signin_internals_util::TimedSigninStatusField i =
           signin_internals_util::TIMED_FIELDS_BEGIN;
       i < signin_internals_util::TIMED_FIELDS_END; ++i) {
    const std::string value = SigninStatusFieldToString(i) + ".value";
    const std::string time = SigninStatusFieldToString(i) + ".time";
    user_prefs->RegisterStringPref(value, std::string());
    user_prefs->RegisterStringPref(time, std::string());
  }
}

void AboutSigninInternals::AddSigninObserver(
    AboutSigninInternals::Observer* observer) {
  signin_observers_.AddObserver(observer);
}

void AboutSigninInternals::RemoveSigninObserver(
    AboutSigninInternals::Observer* observer) {
  signin_observers_.RemoveObserver(observer);
}

void AboutSigninInternals::NotifyTimedSigninFieldValueChanged(
    const signin_internals_util::TimedSigninStatusField& field,
    const std::string& value) {
  unsigned int field_index = field - signin_internals_util::TIMED_FIELDS_BEGIN;
  DCHECK(field_index >= 0 &&
         field_index < signin_status_.timed_signin_fields.size());

  base::Time now = base::Time::NowFromSystemTime();
  std::string time_as_str = base::TimeToISO8601(now);
  TimedSigninStatusValue timed_value(value, time_as_str);

  signin_status_.timed_signin_fields[field_index] = timed_value;

  // Also persist these values in the prefs.
  SetPref(client_->GetPrefs(), field, value, time_as_str);

  // If the user is restarting a sign in process, clear the fields that are
  // to come.
  if (field == signin_internals_util::AUTHENTICATION_RESULT_RECEIVED) {
    ClearPref(client_->GetPrefs(),
              signin_internals_util::REFRESH_TOKEN_RECEIVED);
  }

  NotifyObservers();
}

void AboutSigninInternals::RefreshSigninPrefs() {
  // Return if no client exists. Can occur in unit tests.
  if (!client_)
    return;

  PrefService* pref_service = client_->GetPrefs();
  for (signin_internals_util::TimedSigninStatusField i =
           signin_internals_util::TIMED_FIELDS_BEGIN;
       i < signin_internals_util::TIMED_FIELDS_END; ++i) {
    std::string time_str;
    std::string value_str;
    GetPref(pref_service, i, &value_str, &time_str);
    TimedSigninStatusValue value(value_str, time_str);
    signin_status_
        .timed_signin_fields[i - signin_internals_util::TIMED_FIELDS_BEGIN] =
        value;
  }

  // TODO(rogerta): Get status and timestamps for oauth2 tokens.

  NotifyObservers();
}

void AboutSigninInternals::Shutdown() {
  client_->RemoveContentSettingsObserver(this);
  signin_error_controller_->RemoveObserver(this);
  identity_manager_->RemoveObserver(this);
  identity_manager_->RemoveDiagnosticsObserver(this);
  account_reconcilor_->RemoveObserver(this);
}

void AboutSigninInternals::OnContentSettingChanged(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsTypeSet content_type_set) {
  // If this is not a change to cookie settings, just ignore.
  if (!content_type_set.Contains(ContentSettingsType::COOKIES))
    return;

  NotifyObservers();
}

void AboutSigninInternals::NotifyObservers() {
  if (signin_observers_.empty())
    return;

  base::Value signin_status_value = signin_status_.ToValue(
      identity_manager_, signin_error_controller_, client_,
      account_consistency_, account_reconcilor_);

  for (auto& observer : signin_observers_)
    observer.OnSigninStateChanged(&signin_status_value);
}

base::Value AboutSigninInternals::GetSigninStatus() {
  return signin_status_.ToValue(identity_manager_, signin_error_controller_,
                                client_, account_consistency_,
                                account_reconcilor_);
}

void AboutSigninInternals::OnAccessTokenRequested(
    const CoreAccountId& account_id,
    const std::string& consumer_id,
    const signin::ScopeSet& scopes) {
  TokenInfo* token = signin_status_.FindToken(account_id, consumer_id, scopes);
  if (token) {
    *token = TokenInfo(consumer_id, scopes);
  } else {
    signin_status_.token_info_map[account_id].push_back(
        std::make_unique<TokenInfo>(consumer_id, scopes));
  }

  NotifyObservers();
}

void AboutSigninInternals::OnAccessTokenRequestCompleted(
    const CoreAccountId& account_id,
    const std::string& consumer_id,
    const signin::ScopeSet& scopes,
    GoogleServiceAuthError error,
    base::Time expiration_time) {
  TokenInfo* token = signin_status_.FindToken(account_id, consumer_id, scopes);
  if (!token) {
    DVLOG(1) << "Can't find token: " << account_id << ", " << consumer_id;
    return;
  }

  token->receive_time = base::Time::Now();
  token->error = error;
  token->expiration_time = expiration_time;

  NotifyObservers();
}

void AboutSigninInternals::OnRefreshTokenUpdatedForAccountFromSource(
    const CoreAccountId& account_id,
    bool is_refresh_token_valid,
    const std::string& source) {
  RefreshTokenEvent event;
  event.account_id = account_id;
  event.type =
      is_refresh_token_valid
          ? AboutSigninInternals::RefreshTokenEventType::kUpdateToRegular
          : AboutSigninInternals::RefreshTokenEventType::kUpdateToInvalid;
  event.source = source;
  signin_status_.AddRefreshTokenEvent(event);
}

void AboutSigninInternals::OnRefreshTokenRemovedForAccountFromSource(
    const CoreAccountId& account_id,
    const std::string& source) {
  RefreshTokenEvent event;
  event.account_id = account_id;
  event.type = AboutSigninInternals::RefreshTokenEventType::kRevokeRegular;
  event.source = source;
  signin_status_.AddRefreshTokenEvent(event);
}

void AboutSigninInternals::OnRefreshTokensLoaded() {
  RefreshTokenEvent event;
  // This event concerns all accounts, so it does not have any account id.
  event.type = AboutSigninInternals::RefreshTokenEventType::kAllTokensLoaded;
  signin_status_.AddRefreshTokenEvent(event);
  NotifyObservers();
}

void AboutSigninInternals::OnEndBatchOfRefreshTokenStateChanges() {
  NotifyObservers();
}

void AboutSigninInternals::OnAccessTokenRemovedFromCache(
    const CoreAccountId& account_id,
    const signin::ScopeSet& scopes) {
  for (const std::unique_ptr<TokenInfo>& token :
       signin_status_.token_info_map[account_id]) {
    if (token->scopes == scopes)
      token->Invalidate();
  }
  NotifyObservers();
}

void AboutSigninInternals::OnRefreshTokenReceived(const std::string& status) {
  NotifyTimedSigninFieldValueChanged(
      signin_internals_util::REFRESH_TOKEN_RECEIVED, status);
}

void AboutSigninInternals::OnAuthenticationResultReceived(
    const std::string& status) {
  NotifyTimedSigninFieldValueChanged(
      signin_internals_util::AUTHENTICATION_RESULT_RECEIVED, status);
}

void AboutSigninInternals::OnErrorChanged() {
  NotifyObservers();
}

void AboutSigninInternals::OnBlockReconcile() {
  NotifyObservers();
}

void AboutSigninInternals::OnUnblockReconcile() {
  NotifyObservers();
}

void AboutSigninInternals::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event) {
  NotifyObservers();
}

void AboutSigninInternals::OnAccountsInCookieUpdated(
    const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
    const GoogleServiceAuthError& error) {
  if (error.state() != GoogleServiceAuthError::NONE)
    return;

  base::Value::ListStorage cookie_info;
  for (const auto& signed_in_account :
       accounts_in_cookie_jar_info.signed_in_accounts) {
    AddCookieEntry(cookie_info, signed_in_account.raw_email,
                   signed_in_account.gaia_id,
                   signed_in_account.valid ? "Valid" : "Invalid");
  }

  if (accounts_in_cookie_jar_info.signed_in_accounts.size() == 0) {
    AddCookieEntry(cookie_info, "No Accounts Present.", std::string(),
                   std::string());
  }

  base::Value cookie_status(base::Value::Type::DICTIONARY);
  cookie_status.SetKey("cookie_info", base::Value(std::move(cookie_info)));
  // Update the observers that the cookie's accounts are updated.
  for (auto& observer : signin_observers_)
    observer.OnCookieAccountsFetched(&cookie_status);
}

AboutSigninInternals::TokenInfo::TokenInfo(const std::string& consumer_id,
                                           const signin::ScopeSet& scopes)
    : consumer_id(consumer_id),
      scopes(scopes),
      request_time(base::Time::Now()),
      error(GoogleServiceAuthError::AuthErrorNone()),
      removed_(false) {}

AboutSigninInternals::TokenInfo::~TokenInfo() {}

bool AboutSigninInternals::TokenInfo::LessThan(
    const std::unique_ptr<TokenInfo>& a,
    const std::unique_ptr<TokenInfo>& b) {
  return std::tie(a->request_time, a->consumer_id, a->scopes) <
         std::tie(b->request_time, b->consumer_id, b->scopes);
}

void AboutSigninInternals::TokenInfo::Invalidate() { removed_ = true; }

base::Value AboutSigninInternals::TokenInfo::ToValue() const {
  base::Value token_info(base::Value::Type::DICTIONARY);
  token_info.SetStringKey("service", consumer_id);

  std::string scopes_str;
  for (auto it = scopes.begin(); it != scopes.end(); ++it) {
    scopes_str += *it + "\n";
  }
  token_info.SetStringKey("scopes", scopes_str);
  token_info.SetStringKey("request_time", base::TimeToISO8601(request_time));

  if (removed_) {
    token_info.SetStringKey("status", "Token was revoked.");
  } else if (!receive_time.is_null()) {
    if (error == GoogleServiceAuthError::AuthErrorNone()) {
      bool token_expired = expiration_time < base::Time::Now();
      std::string expiration_time_string = base::TimeToISO8601(expiration_time);
      if (expiration_time.is_null()) {
        token_expired = false;
        expiration_time_string = "Expiration time not available";
      }
      std::string status_str;
      std::string expire_string = "Expire";
      if (token_expired)
        expire_string = "Expired";
      base::StringAppendF(&status_str, "Received token at %s. %s at %s",
                          base::TimeToISO8601(receive_time).c_str(),
                          expire_string.c_str(),
                          expiration_time_string.c_str());
      // JS code looks for `Expired at` string in order to mark
      // specific status row red color. Changing `Exired at` status
      // requires a change in JS code too.
      token_info.SetStringKey("status", status_str);
    } else {
      token_info.SetStringKey(
          "status",
          base::StringPrintf("Failure: %s", error.ToString().c_str()));
    }
  } else {
    token_info.SetStringKey("status", "Waiting for response");
  }

  return token_info;
}

AboutSigninInternals::RefreshTokenEvent::RefreshTokenEvent()
    : timestamp(base::Time::Now()) {}

std::string AboutSigninInternals::RefreshTokenEvent::GetTypeAsString() const {
  switch (type) {
    case AboutSigninInternals::RefreshTokenEventType::kUpdateToRegular:
      return "Updated";
    case AboutSigninInternals::RefreshTokenEventType::kUpdateToInvalid:
      return "Invalidated";
    case AboutSigninInternals::RefreshTokenEventType::kRevokeRegular:
      return "Revoked";
    case AboutSigninInternals::RefreshTokenEventType::kAllTokensLoaded:
      return "Loaded";
  }
}

AboutSigninInternals::SigninStatus::SigninStatus()
    : timed_signin_fields(signin_internals_util::TIMED_FIELDS_COUNT) {}

AboutSigninInternals::SigninStatus::~SigninStatus() {}

AboutSigninInternals::TokenInfo* AboutSigninInternals::SigninStatus::FindToken(
    const CoreAccountId& account_id,
    const std::string& consumer_id,
    const signin::ScopeSet& scopes) {
  for (const std::unique_ptr<TokenInfo>& token : token_info_map[account_id]) {
    if (token->consumer_id == consumer_id && token->scopes == scopes)
      return token.get();
  }
  return nullptr;
}

void AboutSigninInternals::SigninStatus::AddRefreshTokenEvent(
    const AboutSigninInternals::RefreshTokenEvent& event) {
  if (refresh_token_events.size() > kMaxRefreshTokenListSize)
    refresh_token_events.pop_front();

  refresh_token_events.push_back(event);
}

base::Value AboutSigninInternals::SigninStatus::ToValue(
    signin::IdentityManager* identity_manager,
    SigninErrorController* signin_error_controller,
    SigninClient* signin_client,
    signin::AccountConsistencyMethod account_consistency,
    AccountReconcilor* account_reconcilor) {
  base::Value::ListStorage signin_info;

  // A summary of signin related info first.
  {
    base::Value::ListStorage basic_info;
    AddSectionEntry(basic_info, "Account Consistency",
                    GetAccountConsistencyDescription(account_consistency));
    AddSectionEntry(basic_info, "Signin Status",
                    GetSigninStatusDescription(identity_manager));
    signin::LoadCredentialsState load_tokens_state =
        identity_manager->GetDiagnosticsProvider()
            ->GetDetailedStateOfLoadingOfRefreshTokens();
    AddSectionEntry(basic_info, "TokenService Load Status",
                    TokenServiceLoadCredentialsStateToLabel(load_tokens_state));
    AddSectionEntry(
        basic_info, "Gaia cookies state",
        GetGaiaCookiesStateAsString(GetGaiaCookiesState(signin_client)));

    if (identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
      CoreAccountInfo account_info = identity_manager->GetPrimaryAccountInfo(
          signin::ConsentLevel::kSignin);
      AddSectionEntry(
          basic_info,
          SigninStatusFieldToLabel(signin_internals_util::ACCOUNT_ID),
          account_info.account_id.ToString());
      AddSectionEntry(basic_info,
                      SigninStatusFieldToLabel(signin_internals_util::GAIA_ID),
                      account_info.gaia);
      AddSectionEntry(basic_info,
                      SigninStatusFieldToLabel(signin_internals_util::USERNAME),
                      account_info.email);
      if (signin_error_controller->HasError()) {
        const CoreAccountId error_account_id =
            signin_error_controller->error_account_id();
        const AccountInfo error_account_info =
            identity_manager->FindExtendedAccountInfoByAccountId(
                error_account_id);
        AddSectionEntry(basic_info, "Auth Error",
                        signin_error_controller->auth_error().ToString());
        AddSectionEntry(basic_info, "Auth Error Account Id",
                        error_account_id.ToString());
        AddSectionEntry(basic_info, "Auth Error Username",
                        error_account_info.email);
      } else {
        AddSectionEntry(basic_info, "Auth Error", "None");
      }
    }

    AddSectionEntry(
        basic_info, "Account Reconcilor blocked",
        account_reconcilor->IsReconcileBlocked() ? "True" : "False");

    AddSection(signin_info, base::Value(std::move(basic_info)),
               "Basic Information");
  }

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  // Time and status information of the possible sign in types.
  {
    base::Value::ListStorage detailed_info;
    for (signin_internals_util::TimedSigninStatusField i =
             signin_internals_util::TIMED_FIELDS_BEGIN;
         i < signin_internals_util::TIMED_FIELDS_END; ++i) {
      const std::string status_field_label = SigninStatusFieldToLabel(i);

      AddSectionEntry(
          detailed_info, status_field_label,
          timed_signin_fields[i - signin_internals_util::TIMED_FIELDS_BEGIN]
              .first,
          timed_signin_fields[i - signin_internals_util::TIMED_FIELDS_BEGIN]
              .second);
    }

    base::TimeDelta cookie_requests_delay =
        identity_manager->GetDiagnosticsProvider()
            ->GetDelayBeforeMakingCookieRequests();

    if (cookie_requests_delay.is_positive()) {
      base::Time next_retry_time =
          base::Time::NowFromSystemTime() + cookie_requests_delay;
      AddSectionEntry(detailed_info, "Cookie Manager Next Retry",
                      base::TimeToISO8601(next_retry_time), "");
    }

    base::TimeDelta token_requests_delay =
        identity_manager->GetDiagnosticsProvider()
            ->GetDelayBeforeMakingAccessTokenRequests();

    if (token_requests_delay.is_positive()) {
      base::Time next_retry_time =
          base::Time::NowFromSystemTime() + token_requests_delay;
      AddSectionEntry(detailed_info, "Token Service Next Retry",
                      base::TimeToISO8601(next_retry_time), "");
    }

    AddSection(signin_info, base::Value(std::move(detailed_info)),
               "Last Signin Details");
  }
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

  base::Value signin_status(base::Value::Type::DICTIONARY);
  signin_status.SetKey("signin_info", base::Value(std::move(signin_info)));

  // Token information for all services.
  base::Value::ListStorage token_info;
  for (auto& it : token_info_map) {
    base::Value::ListStorage token_details;
    std::sort(it.second.begin(), it.second.end(), TokenInfo::LessThan);
    for (const std::unique_ptr<TokenInfo>& token : it.second)
      token_details.push_back(token->ToValue());

    AddSection(token_info, base::Value(std::move(token_details)),
               it.first.ToString());
  }
  signin_status.SetKey("token_info", base::Value(std::move(token_info)));

  // Account info section
  base::Value::ListStorage account_info_section;
  const std::vector<CoreAccountInfo>& accounts_with_refresh_tokens =
      identity_manager->GetAccountsWithRefreshTokens();
  if (accounts_with_refresh_tokens.size() == 0) {
    base::Value no_token_entry(base::Value::Type::DICTIONARY);
    no_token_entry.SetStringKey("accountId", "No token in Token Service.");
    account_info_section.push_back(std::move(no_token_entry));
  } else {
    for (const CoreAccountInfo& account_info : accounts_with_refresh_tokens) {
      base::Value entry(base::Value::Type::DICTIONARY);
      entry.SetStringKey("accountId", account_info.account_id.ToString());
      // TODO(https://crbug.com/919793): Remove this field once the token
      // service is internally consistent on all platforms.
      entry.SetBoolKey("hasRefreshToken",
                       identity_manager->HasAccountWithRefreshToken(
                           account_info.account_id));
      entry.SetBoolKey(
          "hasAuthError",
          identity_manager->HasAccountWithRefreshTokenInPersistentErrorState(
              account_info.account_id));
      account_info_section.push_back(std::move(entry));
    }
  }
  signin_status.SetKey("accountInfo",
                       base::Value(std::move(account_info_section)));

  // Refresh token events section
  base::Value::ListStorage refresh_token_events_value;
  for (const auto& event : refresh_token_events) {
    base::Value entry(base::Value::Type::DICTIONARY);
    entry.SetStringKey("accountId", event.account_id.ToString());
    entry.SetStringKey("timestamp", base::TimeToISO8601(event.timestamp));
    entry.SetStringKey("type", event.GetTypeAsString());
    entry.SetStringKey("source", event.source);
    refresh_token_events_value.push_back(std::move(entry));
  }
  signin_status.SetKey("refreshTokenEvents",
                       base::Value(std::move(refresh_token_events_value)));

  return signin_status;
}
