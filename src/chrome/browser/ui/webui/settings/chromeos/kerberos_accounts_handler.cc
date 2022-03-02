// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/kerberos_accounts_handler.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "chrome/browser/ash/kerberos/kerberos_credentials_manager.h"
#include "chrome/browser/ash/kerberos/kerberos_credentials_manager_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/settings/chromeos/os_settings_section.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_ui_data_source.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/chromeos/resources/grit/ui_chromeos_resources.h"
#include "ui/gfx/image/image_skia.h"

namespace chromeos {
namespace settings {
namespace {

bool IsKerberosEnabled(
    KerberosCredentialsManager* kerberos_credentials_manager) {
  return kerberos_credentials_manager != nullptr &&
         kerberos_credentials_manager->IsKerberosEnabled();
}

// Adds title for Kerberos subsection and Add Accounts page.
void AddKerberosTitleStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"kerberosAccountsSubMenuLabel",
       IDS_SETTINGS_KERBEROS_ACCOUNTS_SUBMENU_LABEL},
      {"kerberosAccountsPageTitle", IDS_SETTINGS_KERBEROS_ACCOUNTS_PAGE_TITLE},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);
}

// Adds load time boolean corresponding to Kerberos enable state.
void AddKerberosEnabledFlag(
    content::WebUIDataSource* html_source,
    KerberosCredentialsManager* kerberos_credentials_manager) {
  html_source->AddBoolean("isKerberosEnabled",
                          IsKerberosEnabled(kerberos_credentials_manager));
}

// Adds load time strings to Kerberos Add Accounts dialog.
void AddKerberosAddAccountDialogStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"kerberosAccountsAdvancedConfigLabel",
       IDS_SETTINGS_KERBEROS_ACCOUNTS_ADVANCED_CONFIG_LABEL},
      {"kerberosAdvancedConfigTitle",
       IDS_SETTINGS_KERBEROS_ADVANCED_CONFIG_TITLE},
      {"kerberosAdvancedConfigDesc",
       IDS_SETTINGS_KERBEROS_ADVANCED_CONFIG_DESC},
      {"addKerberosAccountRememberPassword",
       IDS_SETTINGS_ADD_KERBEROS_ACCOUNT_REMEMBER_PASSWORD},
      {"kerberosPassword", IDS_SETTINGS_KERBEROS_PASSWORD},
      {"kerberosUsername", IDS_SETTINGS_KERBEROS_USERNAME},
      {"addKerberosAccountDescription",
       IDS_SETTINGS_ADD_KERBEROS_ACCOUNT_DESCRIPTION},
      {"kerberosErrorNetworkProblem",
       IDS_SETTINGS_KERBEROS_ERROR_NETWORK_PROBLEM},
      {"kerberosErrorUsernameInvalid",
       IDS_SETTINGS_KERBEROS_ERROR_USERNAME_INVALID},
      {"kerberosErrorUsernameUnknown",
       IDS_SETTINGS_KERBEROS_ERROR_USERNAME_UNKNOWN},
      {"kerberosErrorDuplicatePrincipalName",
       IDS_SETTINGS_KERBEROS_ERROR_DUPLICATE_PRINCIPAL_NAME},
      {"kerberosErrorContactingServer",
       IDS_SETTINGS_KERBEROS_ERROR_CONTACTING_SERVER},
      {"kerberosErrorPasswordInvalid",
       IDS_SETTINGS_KERBEROS_ERROR_PASSWORD_INVALID},
      {"kerberosErrorPasswordExpired",
       IDS_SETTINGS_KERBEROS_ERROR_PASSWORD_EXPIRED},
      {"kerberosErrorKdcEncType", IDS_SETTINGS_KERBEROS_ERROR_KDC_ENC_TYPE},
      {"kerberosErrorGeneral", IDS_SETTINGS_KERBEROS_ERROR_GENERAL},
      {"kerberosConfigErrorSectionNestedInGroup",
       IDS_SETTINGS_KERBEROS_CONFIG_ERROR_SECTION_NESTED_IN_GROUP},
      {"kerberosConfigErrorSectionSyntax",
       IDS_SETTINGS_KERBEROS_CONFIG_ERROR_SECTION_SYNTAX},
      {"kerberosConfigErrorExpectedOpeningCurlyBrace",
       IDS_SETTINGS_KERBEROS_CONFIG_ERROR_EXPECTED_OPENING_CURLY_BRACE},
      {"kerberosConfigErrorExtraCurlyBrace",
       IDS_SETTINGS_KERBEROS_CONFIG_ERROR_EXTRA_CURLY_BRACE},
      {"kerberosConfigErrorRelationSyntax",
       IDS_SETTINGS_KERBEROS_CONFIG_ERROR_RELATION_SYNTAX_ERROR},
      {"kerberosConfigErrorKeyNotSupported",
       IDS_SETTINGS_KERBEROS_CONFIG_ERROR_KEY_NOT_SUPPORTED},
      {"kerberosConfigErrorSectionNotSupported",
       IDS_SETTINGS_KERBEROS_CONFIG_ERROR_SECTION_NOT_SUPPORTED},
      {"kerberosConfigErrorKrb5FailedToParse",
       IDS_SETTINGS_KERBEROS_CONFIG_ERROR_KRB5_FAILED_TO_PARSE},
      {"kerberosConfigErrorTooManyNestedGroups",
       IDS_SETTINGS_KERBEROS_CONFIG_ERROR_TOO_MANY_NESTED_GROUPS},
      {"addKerberosAccountRefreshButtonLabel",
       IDS_SETTINGS_ADD_KERBEROS_ACCOUNT_REFRESH_BUTTON_LABEL},
      {"addKerberosAccount", IDS_SETTINGS_ADD_KERBEROS_ACCOUNT},
      {"refreshKerberosAccount", IDS_SETTINGS_REFRESH_KERBEROS_ACCOUNT},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);

  PrefService* local_state = g_browser_process->local_state();

  // Whether the 'Remember password' checkbox is enabled.
  html_source->AddBoolean(
      "kerberosRememberPasswordEnabled",
      local_state->GetBoolean(::prefs::kKerberosRememberPasswordEnabled));

  // Kerberos default configuration.
  html_source->AddString(
      "defaultKerberosConfig",
      chromeos::KerberosCredentialsManager::GetDefaultKerberosConfig());
}

// Adds load time strings to Kerberos Accounts page.
void AddKerberosAccountsPageStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"kerberosAccountsAddAccountLabel",
       IDS_SETTINGS_KERBEROS_ACCOUNTS_ADD_ACCOUNT_LABEL},
      {"kerberosAccountsRefreshNowLabel",
       IDS_SETTINGS_KERBEROS_ACCOUNTS_REFRESH_NOW_LABEL},
      {"kerberosAccountsSetAsActiveAccountLabel",
       IDS_SETTINGS_KERBEROS_ACCOUNTS_SET_AS_ACTIVE_ACCOUNT_LABEL},
      {"kerberosAccountsSignedOut", IDS_SETTINGS_KERBEROS_ACCOUNTS_SIGNED_OUT},
      {"kerberosAccountsListHeader",
       IDS_SETTINGS_KERBEROS_ACCOUNTS_LIST_HEADER},
      {"kerberosAccountsRemoveAccountLabel",
       IDS_SETTINGS_KERBEROS_ACCOUNTS_REMOVE_ACCOUNT_LABEL},
      {"kerberosAccountsReauthenticationLabel",
       IDS_SETTINGS_KERBEROS_ACCOUNTS_REAUTHENTICATION_LABEL},
      {"kerberosAccountsTicketActive",
       IDS_SETTINGS_KERBEROS_ACCOUNTS_TICKET_ACTIVE},
      {"kerberosAccountsAccountRemovedTip",
       IDS_SETTINGS_KERBEROS_ACCOUNTS_ACCOUNT_REMOVED_TIP},
      {"kerberosAccountsAccountRefreshedTip",
       IDS_SETTINGS_KERBEROS_ACCOUNTS_ACCOUNT_REFRESHED_TIP},
      {"kerberosAccountsSignedIn", IDS_SETTINGS_KERBEROS_ACCOUNTS_SIGNED_IN},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);

  PrefService* local_state = g_browser_process->local_state();

  // Whether new Kerberos accounts may be added.
  html_source->AddBoolean(
      "kerberosAddAccountsAllowed",
      local_state->GetBoolean(::prefs::kKerberosAddAccountsAllowed));

  // Kerberos accounts page with "Learn more" link.
  html_source->AddString(
      "kerberosAccountsDescription",
      l10n_util::GetStringFUTF16(IDS_SETTINGS_KERBEROS_ACCOUNTS_DESCRIPTION,
                                 OsSettingsSection::GetHelpUrlWithBoard(
                                     chrome::kKerberosAccountsLearnMoreURL)));
}

}  // namespace

// static
std::unique_ptr<KerberosAccountsHandler>
KerberosAccountsHandler::CreateIfKerberosEnabled(Profile* profile) {
  KerberosCredentialsManager* kerberos_credentials_manager =
      KerberosCredentialsManagerFactory::GetExisting(profile);
  if (!IsKerberosEnabled(kerberos_credentials_manager))
    return nullptr;
  return base::WrapUnique(
      new KerberosAccountsHandler(kerberos_credentials_manager));
}

// static
void KerberosAccountsHandler::AddLoadTimeKerberosStrings(
    content::WebUIDataSource* html_source,
    KerberosCredentialsManager* kerberos_credentials_manager) {
  AddKerberosEnabledFlag(html_source, kerberos_credentials_manager);
  AddKerberosTitleStrings(html_source);
  AddKerberosAccountsPageStrings(html_source);
  AddKerberosAddAccountDialogStrings(html_source);
}

KerberosAccountsHandler::~KerberosAccountsHandler() = default;

KerberosAccountsHandler::KerberosAccountsHandler(
    KerberosCredentialsManager* kerberos_credentials_manager)
    : kerberos_credentials_manager_(kerberos_credentials_manager) {
  DCHECK(kerberos_credentials_manager_);
}

void KerberosAccountsHandler::RegisterMessages() {
  web_ui()->RegisterDeprecatedMessageCallback(
      "getKerberosAccounts",
      base::BindRepeating(&KerberosAccountsHandler::HandleGetKerberosAccounts,
                          weak_factory_.GetWeakPtr()));
  web_ui()->RegisterDeprecatedMessageCallback(
      "addKerberosAccount",
      base::BindRepeating(&KerberosAccountsHandler::HandleAddKerberosAccount,
                          weak_factory_.GetWeakPtr()));
  web_ui()->RegisterDeprecatedMessageCallback(
      "removeKerberosAccount",
      base::BindRepeating(&KerberosAccountsHandler::HandleRemoveKerberosAccount,
                          weak_factory_.GetWeakPtr()));
  web_ui()->RegisterDeprecatedMessageCallback(
      "validateKerberosConfig",
      base::BindRepeating(
          &KerberosAccountsHandler::HandleValidateKerberosConfig,
          weak_factory_.GetWeakPtr()));
  web_ui()->RegisterDeprecatedMessageCallback(
      "setAsActiveKerberosAccount",
      base::BindRepeating(
          &KerberosAccountsHandler::HandleSetAsActiveKerberosAccount,
          weak_factory_.GetWeakPtr()));
}

void KerberosAccountsHandler::HandleGetKerberosAccounts(
    const base::ListValue* args) {
  AllowJavascript();

  CHECK_EQ(1U, args->GetList().size());
  const std::string& callback_id = args->GetList()[0].GetString();

  if (!kerberos_credentials_manager_->IsKerberosEnabled()) {
    ResolveJavascriptCallback(base::Value(callback_id), base::Value());
    return;
  }

  kerberos_credentials_manager_->ListAccounts(
      base::BindOnce(&KerberosAccountsHandler::OnListAccounts,
                     weak_factory_.GetWeakPtr(), callback_id));
}

void KerberosAccountsHandler::OnListAccounts(
    const std::string& callback_id,
    const kerberos::ListAccountsResponse& response) {
  base::ListValue accounts;

  // Ticket icon is a key.
  gfx::ImageSkia skia_ticket_icon =
      *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          IDR_KERBEROS_ICON_KEY);
  std::string ticket_icon = webui::GetBitmapDataUrl(
      skia_ticket_icon.GetRepresentation(1.0f).GetBitmap());

  const std::string& active_principal =
      kerberos_credentials_manager_->GetActiveAccount();

  for (int n = 0; n < response.accounts_size(); ++n) {
    const kerberos::Account& account = response.accounts(n);

    // Format validity time as 'xx hours yy minutes' for validity < 1 day and
    // 'nn days' otherwise.
    base::TimeDelta tgt_validity =
        base::Seconds(account.tgt_validity_seconds());
    const std::u16string valid_for_duration = ui::TimeFormat::Detailed(
        ui::TimeFormat::FORMAT_DURATION, ui::TimeFormat::LENGTH_LONG,
        tgt_validity < base::Days(1) ? -1 : 0, tgt_validity);

    base::DictionaryValue account_dict;
    account_dict.SetString("principalName", account.principal_name());
    account_dict.SetString("config", account.krb5conf());
    account_dict.SetBoolean("isSignedIn", account.tgt_validity_seconds() > 0);
    account_dict.SetString("validForDuration", valid_for_duration);
    account_dict.SetBoolean("isActive",
                            account.principal_name() == active_principal);
    account_dict.SetBoolean("isManaged", account.is_managed());
    account_dict.SetBoolean("passwordWasRemembered",
                            account.password_was_remembered());
    account_dict.SetString("pic", ticket_icon);
    accounts.Append(std::move(account_dict));
  }

  ResolveJavascriptCallback(base::Value(callback_id), std::move(accounts));
}

void KerberosAccountsHandler::HandleAddKerberosAccount(
    const base::ListValue* args) {
  AllowJavascript();

  // TODO(https://crbug.com/961246):
  //   - Prevent account changes when Kerberos is disabled.
  //   - Remove all accounts when Kerberos is disabled.

  CHECK_EQ(6U, args->GetList().size());
  const std::string& callback_id = args->GetList()[0].GetString();
  const std::string& principal_name = args->GetList()[1].GetString();
  const std::string& password = args->GetList()[2].GetString();
  const bool remember_password = args->GetList()[3].GetBool();
  const std::string& config = args->GetList()[4].GetString();
  const bool allow_existing = args->GetList()[5].GetBool();

  if (!kerberos_credentials_manager_->IsKerberosEnabled()) {
    ResolveJavascriptCallback(base::Value(callback_id),
                              base::Value(kerberos::ERROR_KERBEROS_DISABLED));
    return;
  }

  kerberos_credentials_manager_->AddAccountAndAuthenticate(
      principal_name, false /* is_managed */, password, remember_password,
      config, allow_existing,
      base::BindOnce(&KerberosAccountsHandler::OnAddAccountAndAuthenticate,
                     weak_factory_.GetWeakPtr(), callback_id));
}

void KerberosAccountsHandler::OnAddAccountAndAuthenticate(
    const std::string& callback_id,
    kerberos::ErrorType error) {
  ResolveJavascriptCallback(base::Value(callback_id),
                            base::Value(static_cast<int>(error)));
}

void KerberosAccountsHandler::HandleRemoveKerberosAccount(
    const base::ListValue* args) {
  AllowJavascript();

  CHECK_EQ(2U, args->GetList().size());
  const std::string& callback_id = args->GetList()[0].GetString();
  const std::string& principal_name = args->GetList()[1].GetString();

  if (!kerberos_credentials_manager_->IsKerberosEnabled()) {
    ResolveJavascriptCallback(base::Value(callback_id),
                              base::Value(kerberos::ERROR_KERBEROS_DISABLED));
    return;
  }

  kerberos_credentials_manager_->RemoveAccount(
      principal_name, base::BindOnce(&KerberosAccountsHandler::OnRemoveAccount,
                                     weak_factory_.GetWeakPtr(), callback_id));
}

void KerberosAccountsHandler::OnRemoveAccount(const std::string& callback_id,
                                              kerberos::ErrorType error) {
  ResolveJavascriptCallback(base::Value(callback_id),
                            base::Value(static_cast<int>(error)));
}

void KerberosAccountsHandler::HandleValidateKerberosConfig(
    const base::ListValue* args) {
  AllowJavascript();

  CHECK_EQ(2U, args->GetList().size());
  const std::string& callback_id = args->GetList()[0].GetString();
  const std::string& krb5conf = args->GetList()[1].GetString();

  if (!kerberos_credentials_manager_->IsKerberosEnabled()) {
    ResolveJavascriptCallback(base::Value(callback_id),
                              base::Value(kerberos::ERROR_KERBEROS_DISABLED));
    return;
  }

  kerberos_credentials_manager_->ValidateConfig(
      krb5conf, base::BindOnce(&KerberosAccountsHandler::OnValidateConfig,
                               weak_factory_.GetWeakPtr(), callback_id));
}

void KerberosAccountsHandler::OnValidateConfig(
    const std::string& callback_id,
    const kerberos::ValidateConfigResponse& response) {
  base::Value error_info(base::Value::Type::DICTIONARY);
  error_info.SetKey("code", base::Value(response.error_info().code()));
  if (response.error_info().has_line_index()) {
    error_info.SetKey("lineIndex",
                      base::Value(response.error_info().line_index()));
  }

  base::Value value(base::Value::Type::DICTIONARY);
  value.SetKey("error", base::Value(static_cast<int>(response.error())));
  value.SetKey("errorInfo", std::move(error_info));
  ResolveJavascriptCallback(base::Value(callback_id), std::move(value));
}

void KerberosAccountsHandler::HandleSetAsActiveKerberosAccount(
    const base::ListValue* args) {
  AllowJavascript();

  CHECK_EQ(1U, args->GetList().size());
  const std::string& principal_name = args->GetList()[0].GetString();

  kerberos_credentials_manager_->SetActiveAccount(principal_name);
}

void KerberosAccountsHandler::OnJavascriptAllowed() {
  credentials_manager_observation_.Observe(kerberos_credentials_manager_);
}

void KerberosAccountsHandler::OnJavascriptDisallowed() {
  credentials_manager_observation_.Reset();
}

void KerberosAccountsHandler::OnAccountsChanged() {
  RefreshUI();
}

void KerberosAccountsHandler::RefreshUI() {
  FireWebUIListener("kerberos-accounts-changed");
}

}  // namespace settings
}  // namespace chromeos
