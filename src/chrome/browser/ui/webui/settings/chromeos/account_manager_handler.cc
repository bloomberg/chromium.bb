// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/account_manager_handler.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/values.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "chrome/browser/ui/webui/signin/inline_login_handler_dialog_chromeos.h"
#include "chromeos/account_manager/account_manager.h"
#include "chromeos/account_manager/account_manager_factory.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/user_manager/user.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/webui/web_ui_util.h"

namespace chromeos {
namespace settings {

namespace {

AccountManager::AccountKey GetAccountKeyFromJsCallback(
    const base::DictionaryValue* const dictionary) {
  const base::Value* id_value = dictionary->FindKey("id");
  DCHECK(id_value);
  const std::string id = id_value->GetString();
  DCHECK(!id.empty());

  const base::Value* account_type_value = dictionary->FindKey("accountType");
  DCHECK(account_type_value);
  const int account_type_int = account_type_value->GetInt();
  DCHECK((account_type_int >=
          account_manager::AccountType::ACCOUNT_TYPE_UNSPECIFIED) &&
         (account_type_int <=
          account_manager::AccountType::ACCOUNT_TYPE_ACTIVE_DIRECTORY));
  const account_manager::AccountType account_type =
      static_cast<account_manager::AccountType>(account_type_int);

  return AccountManager::AccountKey{id, account_type};
}

}  // namespace

AccountManagerUIHandler::AccountManagerUIHandler(
    AccountManager* account_manager,
    AccountTrackerService* account_tracker_service)
    : account_manager_(account_manager),
      account_tracker_service_(account_tracker_service),
      account_mapper_util_(account_tracker_service_),
      account_manager_observer_(this),
      account_tracker_service_observer_(this),
      weak_factory_(this) {
  DCHECK(account_manager_);
  DCHECK(account_tracker_service_);
}

AccountManagerUIHandler::~AccountManagerUIHandler() = default;

void AccountManagerUIHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getAccounts",
      base::BindRepeating(&AccountManagerUIHandler::HandleGetAccounts,
                          weak_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "addAccount",
      base::BindRepeating(&AccountManagerUIHandler::HandleAddAccount,
                          weak_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "removeAccount",
      base::BindRepeating(&AccountManagerUIHandler::HandleRemoveAccount,
                          weak_factory_.GetWeakPtr()));
}

void AccountManagerUIHandler::HandleGetAccounts(const base::ListValue* args) {
  AllowJavascript();
  CHECK(!args->GetList().empty());
  base::Value callback_id = args->GetList()[0].Clone();

  account_manager_->GetAccounts(
      base::BindOnce(&AccountManagerUIHandler::GetAccountsCallbackHandler,
                     weak_factory_.GetWeakPtr(), std::move(callback_id)));
}

void AccountManagerUIHandler::GetAccountsCallbackHandler(
    base::Value callback_id,
    std::vector<AccountManager::AccountKey> account_keys) {
  base::ListValue accounts;

  const AccountId device_account_id =
      ProfileHelper::Get()
          ->GetUserByProfile(Profile::FromWebUI(web_ui()))
          ->GetAccountId();

  base::DictionaryValue device_account;
  for (const auto& account_key : account_keys) {
    // We are only interested in listing GAIA accounts.
    if (account_key.account_type !=
        account_manager::AccountType::ACCOUNT_TYPE_GAIA) {
      continue;
    }
    AccountInfo account_info =
        account_tracker_service_->FindAccountInfoByGaiaId(account_key.id);
    DCHECK(!account_info.IsEmpty());

    if (account_info.full_name.empty()) {
      // Account info has not been fully fetched yet from GAIA. Ignore this
      // account.
      continue;
    }

    base::DictionaryValue account;
    account.SetString("id", account_key.id);
    account.SetInteger("accountType", account_key.account_type);
    account.SetBoolean("isDeviceAccount", false);
    account.SetString("fullName", account_info.full_name);
    account.SetString("email", account_info.email);
    gfx::Image icon =
        account_tracker_service_->GetAccountImage(account_info.account_id);
    account.SetString("pic", webui::GetBitmapDataUrl(icon.AsBitmap()));

    if (account_mapper_util_.IsEqual(account_key, device_account_id)) {
      device_account = std::move(account);
    } else {
      accounts.GetList().push_back(std::move(account));
    }
  }

  // Device account must show up at the top.
  if (!device_account.empty()) {
    device_account.SetBoolean("isDeviceAccount", true);
    accounts.GetList().insert(accounts.GetList().begin(),
                              std::move(device_account));
  }

  ResolveJavascriptCallback(callback_id, accounts);
}

void AccountManagerUIHandler::HandleAddAccount(const base::ListValue* args) {
  AllowJavascript();
  InlineLoginHandlerDialogChromeOS::Show();
}

void AccountManagerUIHandler::HandleRemoveAccount(const base::ListValue* args) {
  AllowJavascript();

  const base::DictionaryValue* dictionary = nullptr;
  args->GetList()[0].GetAsDictionary(&dictionary);
  DCHECK(dictionary);

  const AccountId device_account_id =
      ProfileHelper::Get()
          ->GetUserByProfile(Profile::FromWebUI(web_ui()))
          ->GetAccountId();
  const AccountManager::AccountKey account_key =
      GetAccountKeyFromJsCallback(dictionary);
  if (account_mapper_util_.IsEqual(account_key, device_account_id)) {
    // It should not be possible to remove a device account.
    return;
  }

  account_manager_->RemoveAccount(account_key);
}

void AccountManagerUIHandler::OnJavascriptAllowed() {
  account_manager_observer_.Add(account_manager_);
  account_tracker_service_observer_.Add(account_tracker_service_);
}

void AccountManagerUIHandler::OnJavascriptDisallowed() {
  account_manager_observer_.RemoveAll();
  account_tracker_service_observer_.RemoveAll();
}

// |AccountManager::Observer| overrides.
// Note: We need to listen on |AccountManager| in addition to
// |AccountTrackerService| because there is no guarantee that |AccountManager|
// (our source of truth) will have a newly added account by the time
// |AccountTrackerService| has it.
void AccountManagerUIHandler::OnTokenUpserted(
    const AccountManager::AccountKey& account_key) {
  RefreshUI();
}

void AccountManagerUIHandler::OnAccountRemoved(
    const AccountManager::AccountKey& account_key) {
  RefreshUI();
}

// |AccountTrackerService::Observer| overrides.
// For newly added accounts, |AccountTrackerService| may take some time to
// fetch user's full name and account image. Whenever that is completed, we
// may need to update the UI with this new set of information.
// Note that we may be listening to |AccountTrackerService| but we still
// consider |AccountManager| to be the source of truth for account list.
void AccountManagerUIHandler::OnAccountUpdated(const AccountInfo& info) {
  RefreshUI();
}

void AccountManagerUIHandler::OnAccountImageUpdated(
    const std::string& account_id,
    const gfx::Image& image) {
  RefreshUI();
}

void AccountManagerUIHandler::OnAccountRemoved(const AccountInfo& account_key) {
}

void AccountManagerUIHandler::RefreshUI() {
  FireWebUIListener("accounts-changed");
}

}  // namespace settings
}  // namespace chromeos
