// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/account_manager_handler.h"

#include <utility>

#include "ash/components/account_manager/account_manager_factory.h"
#include "ash/public/cpp/system/toast_catalog.h"
#include "ash/public/cpp/system/toast_data.h"
#include "ash/public/cpp/system/toast_manager.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/ash/account_manager/account_apps_availability.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chromeos/account_manager/account_manager_welcome_dialog.h"
#include "chrome/browser/ui/webui/chromeos/account_manager/account_migration_welcome_dialog.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "chrome/browser/ui/webui/signin/inline_login_dialog_chromeos.h"
#include "chrome/grit/generated_resources.h"
#include "components/account_manager_core/account_manager_facade.h"
#include "components/account_manager_core/chromeos/account_manager_facade_factory.h"
#include "components/signin/public/base/consent_level.h"
#include "components/user_manager/user.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/chromeos/resources/grit/ui_chromeos_resources.h"
#include "ui/gfx/image/image_skia.h"

namespace chromeos {
namespace settings {

namespace {

constexpr char kFamilyLink[] = "Family Link";
constexpr char kAccountRemovedToastId[] =
    "settings_account_manager_account_removed";

::account_manager::AccountKey GetAccountKeyFromJsCallback(
    const base::Value& dictionary) {
  const base::Value* id_value = dictionary.FindKey("id");
  DCHECK(id_value);
  const std::string id = id_value->GetString();
  DCHECK(!id.empty());

  const base::Value* account_type_value = dictionary.FindKey("accountType");
  DCHECK(account_type_value);
  const int account_type_int = account_type_value->GetInt();
  DCHECK((account_type_int >=
          static_cast<int>(account_manager::AccountType::kGaia)) &&
         (account_type_int <=
          static_cast<int>(account_manager::AccountType::kActiveDirectory)));
  const account_manager::AccountType account_type =
      static_cast<account_manager::AccountType>(account_type_int);

  return ::account_manager::AccountKey{id, account_type};
}

::account_manager::Account GetAccountFromJsCallback(
    const base::Value& dictionary) {
  ::account_manager::AccountKey key = GetAccountKeyFromJsCallback(dictionary);
  const std::string* email = dictionary.FindStringKey("email");
  DCHECK(email);
  return ::account_manager::Account{key, *email};
}

bool IsSameAccount(const ::account_manager::AccountKey& account_key,
                   const AccountId& account_id) {
  switch (account_key.account_type()) {
    case account_manager::AccountType::kGaia:
      return (account_id.GetAccountType() == AccountType::GOOGLE) &&
             (account_id.GetGaiaId() == account_key.id());
    case account_manager::AccountType::kActiveDirectory:
      return (account_id.GetAccountType() == AccountType::ACTIVE_DIRECTORY) &&
             (account_id.GetObjGuid() == account_key.id());
  }
}

void ShowToast(const std::string& id,
               ash::ToastCatalogName catalog_name,
               const std::u16string& message) {
  ash::ToastManager::Get()->Show(ash::ToastData(id, catalog_name, message));
}

class AccountBuilder {
 public:
  AccountBuilder() = default;

  AccountBuilder(const AccountBuilder&) = delete;
  AccountBuilder& operator=(const AccountBuilder&) = delete;

  ~AccountBuilder() = default;

  void PopulateFrom(base::DictionaryValue account) {
    account_ = std::move(account);
  }

  bool IsEmpty() const { return account_.DictEmpty(); }

  AccountBuilder& SetId(const std::string& value) {
    account_.SetStringKey("id", value);
    return *this;
  }

  AccountBuilder& SetEmail(const std::string& value) {
    account_.SetStringKey("email", value);
    return *this;
  }

  AccountBuilder& SetFullName(const std::string& value) {
    account_.SetStringKey("fullName", value);
    return *this;
  }

  AccountBuilder& SetAccountType(const int& value) {
    account_.SetIntKey("accountType", value);
    return *this;
  }

  AccountBuilder& SetIsDeviceAccount(const bool& value) {
    account_.SetBoolKey("isDeviceAccount", value);
    return *this;
  }

  AccountBuilder& SetIsSignedIn(const bool& value) {
    account_.SetBoolKey("isSignedIn", value);
    return *this;
  }

  AccountBuilder& SetUnmigrated(const bool& value) {
    account_.SetBoolKey("unmigrated", value);
    return *this;
  }

  AccountBuilder& SetPic(const std::string& value) {
    account_.SetStringKey("pic", value);
    return *this;
  }

  AccountBuilder& SetOrganization(const std::string& value) {
    account_.SetStringKey("organization", value);
    return *this;
  }

  AccountBuilder& SetIsAvailableInArc(bool value) {
    account_.SetBoolKey("isAvailableInArc", value);
    return *this;
  }

  // Should be called only once.
  base::DictionaryValue Build() {
    // Check that values were set.
    DCHECK(account_.FindStringKey("id"));
    DCHECK(account_.FindStringKey("email"));
    DCHECK(account_.FindStringKey("fullName"));
    DCHECK(account_.FindIntKey("accountType"));
    DCHECK(account_.FindBoolKey("isDeviceAccount"));
    DCHECK(account_.FindBoolKey("isSignedIn"));
    DCHECK(account_.FindBoolKey("unmigrated"));
    DCHECK(account_.FindStringKey("pic"));
    if (ash::AccountAppsAvailability::IsArcAccountRestrictionsEnabled()) {
      DCHECK(account_.FindBoolKey("isAvailableInArc"));
    }
    // "organization" is an optional field.

    return std::move(account_);
  }

 private:
  base::DictionaryValue account_;
};

}  // namespace

AccountManagerUIHandler::AccountManagerUIHandler(
    account_manager::AccountManager* account_manager,
    account_manager::AccountManagerFacade* account_manager_facade,
    signin::IdentityManager* identity_manager,
    ash::AccountAppsAvailability* account_apps_availability)
    : account_manager_(account_manager),
      account_manager_facade_(account_manager_facade),
      identity_manager_(identity_manager) {
  DCHECK(account_manager_);
  DCHECK(account_manager_facade_);
  DCHECK(identity_manager_);
  if (ash::AccountAppsAvailability::IsArcAccountRestrictionsEnabled()) {
    account_apps_availability_ = account_apps_availability;
    DCHECK(account_apps_availability_);
  }
}

AccountManagerUIHandler::~AccountManagerUIHandler() = default;

void AccountManagerUIHandler::RegisterMessages() {
  if (!profile_)
    profile_ = Profile::FromWebUI(web_ui());

  web_ui()->RegisterMessageCallback(
      "getAccounts",
      base::BindRepeating(&AccountManagerUIHandler::HandleGetAccounts,
                          weak_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "addAccount",
      base::BindRepeating(&AccountManagerUIHandler::HandleAddAccount,
                          weak_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "reauthenticateAccount",
      base::BindRepeating(&AccountManagerUIHandler::HandleReauthenticateAccount,
                          weak_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "migrateAccount",
      base::BindRepeating(&AccountManagerUIHandler::HandleMigrateAccount,
                          weak_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "removeAccount",
      base::BindRepeating(&AccountManagerUIHandler::HandleRemoveAccount,
                          weak_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "showWelcomeDialogIfRequired",
      base::BindRepeating(
          &AccountManagerUIHandler::HandleShowWelcomeDialogIfRequired,
          weak_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "changeArcAvailability",
      base::BindRepeating(&AccountManagerUIHandler::HandleChangeArcAvailability,
                          weak_factory_.GetWeakPtr()));
}

void AccountManagerUIHandler::SetProfileForTesting(Profile* profile) {
  profile_ = profile;
}

void AccountManagerUIHandler::HandleGetAccounts(
    const base::Value::ConstListView args) {
  AllowJavascript();

  CHECK_EQ(args.size(), 1u);
  CHECK(args[0].is_string());

  base::Value callback_id = args[0].Clone();

  account_manager_->CheckDummyGaiaTokenForAllAccounts(base::BindOnce(
      &AccountManagerUIHandler::OnCheckDummyGaiaTokenForAllAccounts,
      weak_factory_.GetWeakPtr(), std::move(callback_id)));
}

void AccountManagerUIHandler::OnCheckDummyGaiaTokenForAllAccounts(
    base::Value callback_id,
    const std::vector<std::pair<::account_manager::Account, bool>>&
        account_dummy_token_list) {
  if (ash::AccountAppsAvailability::IsArcAccountRestrictionsEnabled()) {
    account_apps_availability_->GetAccountsAvailableInArc(
        base::BindOnce(&AccountManagerUIHandler::FinishHandleGetAccounts,
                       weak_factory_.GetWeakPtr(), std::move(callback_id),
                       std::move(account_dummy_token_list)));
    return;
  }
  FinishHandleGetAccounts(std::move(callback_id),
                          std::move(account_dummy_token_list),
                          base::flat_set<account_manager::Account>());
}

void AccountManagerUIHandler::FinishHandleGetAccounts(
    base::Value callback_id,
    const std::vector<std::pair<::account_manager::Account, bool>>&
        account_dummy_token_list,
    const base::flat_set<account_manager::Account>& arc_accounts) {
  user_manager::User* user = ProfileHelper::Get()->GetUserByProfile(profile_);
  DCHECK(user);

  base::DictionaryValue gaia_device_account;
  base::ListValue accounts = GetSecondaryGaiaAccounts(
      account_dummy_token_list, arc_accounts, user->GetAccountId(),
      profile_->IsChild(), &gaia_device_account);

  AccountBuilder device_account;
  if (user->IsActiveDirectoryUser()) {
    device_account.SetId(user->GetAccountId().GetObjGuid())
        .SetAccountType(
            static_cast<int>(account_manager::AccountType::kActiveDirectory))
        .SetEmail(user->GetDisplayEmail())
        .SetFullName(base::UTF16ToUTF8(user->GetDisplayName()))
        .SetIsSignedIn(true)
        .SetUnmigrated(false);
    if (ash::AccountAppsAvailability::IsArcAccountRestrictionsEnabled()) {
      device_account.SetIsAvailableInArc(true);
    }
    gfx::ImageSkia default_icon =
        *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
            IDR_LOGIN_DEFAULT_USER);
    device_account.SetPic(webui::GetBitmapDataUrl(
        default_icon.GetRepresentation(1.0f).GetBitmap()));
  } else {
    device_account.PopulateFrom(std::move(gaia_device_account));
  }

  if (!device_account.IsEmpty()) {
    device_account.SetIsDeviceAccount(true);

    // Check if user is managed.
    if (profile_->IsChild()) {
      std::string organization = kFamilyLink;
      // Replace space with the non-breaking space.
      base::ReplaceSubstringsAfterOffset(&organization, 0, " ", "&nbsp;");
      device_account.SetOrganization(organization);
    } else if (user->IsActiveDirectoryUser()) {
      device_account.SetOrganization(
          chrome::enterprise_util::GetDomainFromEmail(user->GetDisplayEmail()));
    } else if (profile_->GetProfilePolicyConnector()->IsManaged()) {
      device_account.SetOrganization(
          chrome::enterprise_util::GetDomainFromEmail(
              identity_manager_
                  ->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
                  .email));
    }

    // Device account must show up at the top.
    accounts.Insert(accounts.GetListDeprecated().begin(),
                    device_account.Build());
  }

  ResolveJavascriptCallback(callback_id, accounts);
}

base::ListValue AccountManagerUIHandler::GetSecondaryGaiaAccounts(
    const std::vector<std::pair<::account_manager::Account, bool>>&
        account_dummy_token_list,
    const base::flat_set<account_manager::Account>& arc_accounts,
    const AccountId device_account_id,
    const bool is_child_user,
    base::DictionaryValue* device_account) {
  base::ListValue accounts;
  for (const auto& account_token_pair : account_dummy_token_list) {
    const ::account_manager::Account& stored_account = account_token_pair.first;
    const ::account_manager::AccountKey& account_key = stored_account.key;
    // We are only interested in listing GAIA accounts.
    if (account_key.account_type() != account_manager::AccountType::kGaia) {
      continue;
    }

    AccountInfo maybe_account_info =
        identity_manager_->FindExtendedAccountInfoByGaiaId(account_key.id());
    if (maybe_account_info.IsEmpty()) {
      // This account hasn't propagated to IdentityManager yet. When this
      // happens, `IdentityManager` will call `OnRefreshTokenUpdatedForAccount`
      // which will trigger another UI update.
      continue;
    }

    AccountBuilder account;
    account.SetId(account_key.id())
        .SetAccountType(static_cast<int>(account_key.account_type()))
        .SetIsDeviceAccount(false)
        .SetFullName(maybe_account_info.full_name)
        .SetEmail(stored_account.raw_email)
        .SetUnmigrated(!is_child_user && account_token_pair.second)
        .SetIsSignedIn(!identity_manager_
                            ->HasAccountWithRefreshTokenInPersistentErrorState(
                                maybe_account_info.account_id));
    if (ash::AccountAppsAvailability::IsArcAccountRestrictionsEnabled()) {
      account.SetIsAvailableInArc(arc_accounts.contains(stored_account));
    }

    if (!maybe_account_info.account_image.IsEmpty()) {
      account.SetPic(
          webui::GetBitmapDataUrl(maybe_account_info.account_image.AsBitmap()));
    } else {
      gfx::ImageSkia default_icon =
          *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
              IDR_LOGIN_DEFAULT_USER);
      account.SetPic(webui::GetBitmapDataUrl(
          default_icon.GetRepresentation(1.0f).GetBitmap()));
    }

    if (IsSameAccount(account_key, device_account_id)) {
      *device_account = account.Build();
    } else {
      accounts.Append(account.Build());
    }
  }
  return accounts;
}

void AccountManagerUIHandler::HandleAddAccount(
    const base::Value::ConstListView args) {
  AllowJavascript();
  ::GetAccountManagerFacade(profile_->GetPath().value())
      ->ShowAddAccountDialog(
          account_manager::AccountManagerFacade::AccountAdditionSource::
              kSettingsAddAccountButton);
}

void AccountManagerUIHandler::HandleReauthenticateAccount(
    const base::Value::ConstListView args) {
  AllowJavascript();

  CHECK(!args.empty());
  const std::string& account_email = args[0].GetString();

  ::GetAccountManagerFacade(profile_->GetPath().value())
      ->ShowReauthAccountDialog(
          account_manager::AccountManagerFacade::AccountAdditionSource::
              kSettingsReauthAccountButton,
          account_email);
}

void AccountManagerUIHandler::HandleMigrateAccount(
    const base::Value::ConstListView args) {
  AllowJavascript();

  CHECK(!args.empty());
  const std::string& account_email = args[0].GetString();

  chromeos::AccountMigrationWelcomeDialog::Show(account_email);
}

void AccountManagerUIHandler::HandleRemoveAccount(
    const base::Value::ConstListView args) {
  AllowJavascript();

  CHECK(!args.empty());
  const base::Value& dictionary = args[0];
  CHECK(dictionary.is_dict());

  const AccountId device_account_id =
      ProfileHelper::Get()->GetUserByProfile(profile_)->GetAccountId();
  const ::account_manager::AccountKey account_key =
      GetAccountKeyFromJsCallback(dictionary);
  if (IsSameAccount(account_key, device_account_id)) {
    // It should not be possible to remove a device account.
    return;
  }

  account_manager_->RemoveAccount(account_key);

  // Show toast with removal message.
  const base::Value* email_value = dictionary.FindKey("email");
  const std::string email = email_value->GetString();
  DCHECK(!email.empty());

  ShowToast(kAccountRemovedToastId, ash::ToastCatalogName::kAccountRemoved,
            l10n_util::GetStringFUTF16(
                IDS_SETTINGS_ACCOUNT_MANAGER_ACCOUNT_REMOVED_MESSAGE,
                base::UTF8ToUTF16(email)));
}

void AccountManagerUIHandler::HandleShowWelcomeDialogIfRequired(
    const base::Value::ConstListView args) {
  chromeos::AccountManagerWelcomeDialog::ShowIfRequired();
}

void AccountManagerUIHandler::HandleChangeArcAvailability(
    const base::Value::ConstListView args) {
  DCHECK(ash::AccountAppsAvailability::IsArcAccountRestrictionsEnabled());

  // 2 args: account, is_available.
  CHECK_GT(args.size(), 1u);
  const base::Value& account_dict = args[0];
  CHECK(account_dict.is_dict());
  const absl::optional<bool> is_available = args[1].GetIfBool();
  CHECK(is_available.has_value());

  const ::account_manager::Account account =
      GetAccountFromJsCallback(account_dict);
  account_apps_availability_->SetIsAccountAvailableInArc(account,
                                                         is_available.value());
  // Note: the observer call will update the UI.
}

void AccountManagerUIHandler::OnJavascriptAllowed() {
  account_manager_facade_observation_.Observe(account_manager_facade_);
  identity_manager_observation_.Observe(identity_manager_);
  if (account_apps_availability_) {
    account_apps_availability_observation_.Observe(account_apps_availability_);
  }
}

void AccountManagerUIHandler::OnJavascriptDisallowed() {
  account_manager_facade_observation_.Reset();
  identity_manager_observation_.Reset();
  if (account_apps_availability_) {
    account_apps_availability_observation_.Reset();
  }
}

// |AccountManagerFacade::Observer| overrides. Note: We need to listen on
// |AccountManagerFacade| in addition to |IdentityManager| because there is no
// guarantee that |AccountManager| (our source of truth) will have a newly added
// account by the time |IdentityManager| has it.
void AccountManagerUIHandler::OnAccountUpserted(
    const ::account_manager::Account& account) {
  RefreshUI();
}

void AccountManagerUIHandler::OnAccountRemoved(
    const ::account_manager::Account& account) {
  RefreshUI();
}

// |signin::IdentityManager::Observer| overrides.
// `GetSecondaryGaiaAccounts` skips all accounts that haven't been added to
// `IdentityManager` yet. Thus, we should trigger an updated whenever a new
// account is added into `IdentityManager`.
void AccountManagerUIHandler::OnRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& info) {
  RefreshUI();
}

// For newly added accounts, |signin::IdentityManager| may take some time to
// fetch user's full name and account image. Whenever that is completed, we may
// need to update the UI with this new set of information. Note that we may be
// listening to |signin::IdentityManager| but we still consider |AccountManager|
// to be the source of truth for account list.
void AccountManagerUIHandler::OnExtendedAccountInfoUpdated(
    const AccountInfo& info) {
  RefreshUI();
}

void AccountManagerUIHandler::OnErrorStateOfRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info,
    const GoogleServiceAuthError& error) {
  if (error.state() != GoogleServiceAuthError::NONE) {
    RefreshUI();
  }
}

void AccountManagerUIHandler::OnAccountAvailableInArc(
    const ::account_manager::Account& account) {
  RefreshUI();
}

void AccountManagerUIHandler::OnAccountUnavailableInArc(
    const ::account_manager::Account& account) {
  RefreshUI();
}

void AccountManagerUIHandler::RefreshUI() {
  FireWebUIListener("accounts-changed");
}

}  // namespace settings
}  // namespace chromeos
