// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/edu_account_login_handler_chromeos.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/image_fetcher/image_fetcher_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/webui/signin/inline_login_handler_dialog_chromeos.h"
#include "chromeos/components/account_manager/account_manager.h"
#include "chromeos/components/account_manager/account_manager_factory.h"
#include "components/image_fetcher/core/image_fetcher_service.h"
#include "components/image_fetcher/core/request_metadata.h"
#include "components/signin/public/base/avatar_icon_util.h"
#include "content/public/browser/storage_partition.h"
#include "google_apis/gaia/gaia_constants.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/chromeos/resources/grit/ui_chromeos_resources.h"
#include "ui/gfx/image/image.h"

namespace chromeos {

namespace {
constexpr char kImageFetcherUmaClientName[] =
    "EduAccountLoginProfileImageFetcher";
constexpr char kObfuscatedGaiaIdKey[] = "obfuscatedGaiaId";
constexpr net::NetworkTrafficAnnotationTag traffic_annotation =
    net::DefineNetworkTrafficAnnotation(
        "edu_account_login_profile_image_fetcher",
        R"(
        semantics {
          sender: "Profile image fetcher for EDU account login flow"
          description:
            "Retrieves profile images for user's parent accounts in EDU account"
            "login flow."
          trigger: "Triggered when child user opens account addition flow."
          data: "Account picture URL of GAIA accounts."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting: "This feature cannot be disabled by settings."
          policy_exception_justification: "Not implemented."
        })");
constexpr char kFetchParentsListResultHistogram[] =
    "AccountManager.EduCoexistence.FetchParentsListResult";
constexpr char kFetchAccessTokenResultHistogram[] =
    "AccountManager.EduCoexistence.FetchAccessTokenResult";
constexpr char kCreateRaptResultHistogram[] =
    "AccountManager.EduCoexistence.CreateRaptResult";
}  // namespace

EduAccountLoginHandler::EduAccountLoginHandler(
    const base::RepeatingClosure& close_dialog_closure)
    : close_dialog_closure_(close_dialog_closure) {}

EduAccountLoginHandler::~EduAccountLoginHandler() {
  close_dialog_closure_.Run();
}

EduAccountLoginHandler::ProfileImageFetcher::ProfileImageFetcher(
    image_fetcher::ImageFetcher* image_fetcher,
    const std::map<std::string, GURL>& profile_image_urls,
    base::OnceCallback<void(std::map<std::string, gfx::Image> profile_images)>
        callback)
    : image_fetcher_(image_fetcher),
      profile_image_urls_(profile_image_urls),
      callback_(std::move(callback)) {}

EduAccountLoginHandler::ProfileImageFetcher::~ProfileImageFetcher() = default;

void EduAccountLoginHandler::ProfileImageFetcher::FetchProfileImages() {
  for (const auto& profile_image_url : profile_image_urls_) {
    FetchProfileImage(profile_image_url.first, profile_image_url.second);
  }
}

void EduAccountLoginHandler::ProfileImageFetcher::FetchProfileImage(
    const std::string& obfuscated_gaia_id,
    const GURL& profile_image_url) {
  if (!profile_image_url.is_valid()) {
    OnImageFetched(obfuscated_gaia_id, gfx::Image(),
                   image_fetcher::RequestMetadata());
    return;
  }

  image_fetcher::ImageFetcherParams params(traffic_annotation,
                                           kImageFetcherUmaClientName);
  GURL image_url_with_size(signin::GetAvatarImageURLWithOptions(
      profile_image_url, signin::kAccountInfoImageSize,
      true /* no_silhouette */));
  image_fetcher_->FetchImage(
      image_url_with_size,
      base::BindOnce(
          &EduAccountLoginHandler::ProfileImageFetcher::OnImageFetched,
          weak_ptr_factory_.GetWeakPtr(), obfuscated_gaia_id),
      std::move(params));
}

void EduAccountLoginHandler::ProfileImageFetcher::OnImageFetched(
    const std::string& obfuscated_gaia_id,
    const gfx::Image& image,
    const image_fetcher::RequestMetadata& metadata) {
  fetched_profile_images_[obfuscated_gaia_id] = std::move(image);
  if (fetched_profile_images_.size() == profile_image_urls_.size()) {
    std::move(callback_).Run(std::move(fetched_profile_images_));
  }
}

void EduAccountLoginHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getParents",
      base::BindRepeating(&EduAccountLoginHandler::HandleGetParents,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "parentSignin",
      base::BindRepeating(&EduAccountLoginHandler::HandleParentSignin,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "updateEduCoexistenceFlowResult",
      base::BindRepeating(
          &EduAccountLoginHandler::HandleUpdateEduCoexistenceFlowResult,
          base::Unretained(this)));
}

void EduAccountLoginHandler::OnJavascriptDisallowed() {
  family_fetcher_.reset();
  access_token_fetcher_.reset();
  gaia_auth_fetcher_.reset();
  profile_image_fetcher_.reset();
  get_parents_callback_id_.clear();
  parent_signin_callback_id_.clear();
}

void EduAccountLoginHandler::HandleGetParents(const base::ListValue* args) {
  AllowJavascript();

  CHECK_EQ(args->GetList().size(), 1u);

  if (!get_parents_callback_id_.empty()) {
    // HandleGetParents call is already in progress, reject the callback.
    RejectJavascriptCallback(args->GetList()[0], base::Value());
    return;
  }
  get_parents_callback_id_ = args->GetList()[0].GetString();

  FetchFamilyMembers();
}

void EduAccountLoginHandler::HandleParentSignin(const base::ListValue* args) {
  const base::Value::ConstListView& args_list = args->GetList();
  CHECK_EQ(args_list.size(), 3u);
  CHECK(args_list[0].is_string());

  if (!parent_signin_callback_id_.empty()) {
    // HandleParentSignin call is already in progress, reject the callback.
    RejectJavascriptCallback(args_list[0], base::Value());
    return;
  }
  parent_signin_callback_id_ = args_list[0].GetString();

  const base::DictionaryValue* parent = nullptr;
  args_list[1].GetAsDictionary(&parent);
  CHECK(parent);
  const base::Value* obfuscated_gaia_id_value =
      parent->FindKey(kObfuscatedGaiaIdKey);
  DCHECK(obfuscated_gaia_id_value);
  std::string obfuscated_gaia_id = obfuscated_gaia_id_value->GetString();

  std::string password;
  args_list[2].GetAsString(&password);

  FetchAccessToken(obfuscated_gaia_id, password);
}

void EduAccountLoginHandler::HandleUpdateEduCoexistenceFlowResult(
    const base::ListValue* args) {
  AllowJavascript();

  const base::Value::ConstListView& args_list = args->GetList();
  CHECK_EQ(args_list.size(), 1u);
  int result = args_list[0].GetInt();
  DCHECK(result <= static_cast<int>(InlineLoginHandlerDialogChromeOS::
                                        EduCoexistenceFlowResult::kMaxValue));
  InlineLoginHandlerDialogChromeOS::UpdateEduCoexistenceFlowResult(
      static_cast<InlineLoginHandlerDialogChromeOS::EduCoexistenceFlowResult>(
          result));
}

void EduAccountLoginHandler::FetchFamilyMembers() {
  DCHECK(!family_fetcher_);
  Profile* profile = Profile::FromWebUI(web_ui());
  chromeos::AccountManager* account_manager =
      g_browser_process->platform_part()
          ->GetAccountManagerFactory()
          ->GetAccountManager(profile->GetPath().value());
  DCHECK(account_manager);

  family_fetcher_ = std::make_unique<FamilyInfoFetcher>(
      this, IdentityManagerFactory::GetForProfile(profile),
      account_manager->GetUrlLoaderFactory());
  family_fetcher_->StartGetFamilyMembers();
}

void EduAccountLoginHandler::FetchParentImages(
    base::ListValue parents,
    std::map<std::string, GURL> profile_image_urls) {
  DCHECK(!profile_image_fetcher_);
  image_fetcher::ImageFetcher* fetcher =
      ImageFetcherServiceFactory::GetForKey(
          Profile::FromWebUI(web_ui())->GetProfileKey())
          ->GetImageFetcher(image_fetcher::ImageFetcherConfig::kNetworkOnly);
  profile_image_fetcher_ = std::make_unique<ProfileImageFetcher>(
      fetcher, profile_image_urls,
      base::BindOnce(&EduAccountLoginHandler::OnParentProfileImagesFetched,
                     base::Unretained(this), std::move(parents)));
  profile_image_fetcher_->FetchProfileImages();
}

void EduAccountLoginHandler::FetchAccessToken(
    const std::string& obfuscated_gaia_id,
    const std::string& password) {
  DCHECK(!access_token_fetcher_);
  Profile* profile = Profile::FromWebUI(web_ui());
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  OAuth2AccessTokenManager::ScopeSet scopes;
  scopes.insert(GaiaConstants::kAccountsReauthOAuth2Scope);
  access_token_fetcher_ =
      std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
          "EduAccountLoginHandler", identity_manager, scopes,
          base::BindOnce(
              &EduAccountLoginHandler::CreateReAuthProofTokenForParent,
              base::Unretained(this), std::move(obfuscated_gaia_id),
              std::move(password)),
          signin::PrimaryAccountAccessTokenFetcher::Mode::kImmediate);
}

void EduAccountLoginHandler::FetchReAuthProofTokenForParent(
    const std::string& child_oauth_access_token,
    const std::string& parent_obfuscated_gaia_id,
    const std::string& parent_credential) {
  DCHECK(!gaia_auth_fetcher_);
  Profile* profile = Profile::FromWebUI(web_ui());
  chromeos::AccountManager* account_manager =
      g_browser_process->platform_part()
          ->GetAccountManagerFactory()
          ->GetAccountManager(profile->GetPath().value());
  DCHECK(account_manager);

  gaia_auth_fetcher_ = std::make_unique<GaiaAuthFetcher>(
      this, gaia::GaiaSource::kChrome, account_manager->GetUrlLoaderFactory());
  gaia_auth_fetcher_->StartCreateReAuthProofTokenForParent(
      child_oauth_access_token, parent_obfuscated_gaia_id, parent_credential);
}

void EduAccountLoginHandler::OnGetFamilyMembersSuccess(
    const std::vector<FamilyInfoFetcher::FamilyMember>& members) {
  base::UmaHistogramEnumeration(kFetchParentsListResultHistogram,
                                FamilyInfoFetcher::ErrorCode::kSuccess);
  family_fetcher_.reset();
  base::ListValue parents;
  std::map<std::string, GURL> profile_image_urls;

  for (const auto& member : members) {
    if (member.role != FamilyInfoFetcher::HEAD_OF_HOUSEHOLD &&
        member.role != FamilyInfoFetcher::PARENT) {
      continue;
    }

    base::DictionaryValue parent;
    parent.SetStringKey("email", member.email);
    parent.SetStringKey("displayName", member.display_name);
    parent.SetStringKey(kObfuscatedGaiaIdKey, member.obfuscated_gaia_id);

    parents.Append(std::move(parent));
    profile_image_urls[member.obfuscated_gaia_id] =
        GURL(member.profile_image_url);
  }

  FetchParentImages(std::move(parents), profile_image_urls);
}

void EduAccountLoginHandler::OnFailure(FamilyInfoFetcher::ErrorCode error) {
  base::UmaHistogramEnumeration(kFetchParentsListResultHistogram, error);
  family_fetcher_.reset();
  RejectJavascriptCallback(base::Value(get_parents_callback_id_),
                           base::ListValue());
  get_parents_callback_id_.clear();
}

void EduAccountLoginHandler::OnParentProfileImagesFetched(
    base::ListValue parents,
    std::map<std::string, gfx::Image> profile_images) {
  profile_image_fetcher_.reset();

  for (auto& parent : parents.GetList()) {
    const std::string* obfuscated_gaia_id =
        parent.FindStringKey(kObfuscatedGaiaIdKey);
    DCHECK(obfuscated_gaia_id);
    std::string profile_image;
    if (profile_images[*obfuscated_gaia_id].IsEmpty()) {
      gfx::ImageSkia default_icon =
          *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
              IDR_LOGIN_DEFAULT_USER);

      profile_image = webui::GetBitmapDataUrl(
          default_icon.GetRepresentation(1.0f).GetBitmap());
    } else {
      profile_image = webui::GetBitmapDataUrl(
          profile_images[*obfuscated_gaia_id].AsBitmap());
    }
    parent.SetStringKey("profileImage", profile_image);
  }

  ResolveJavascriptCallback(base::Value(get_parents_callback_id_), parents);
  get_parents_callback_id_.clear();
}

void EduAccountLoginHandler::CreateReAuthProofTokenForParent(
    const std::string& parent_obfuscated_gaia_id,
    const std::string& parent_credential,
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  base::UmaHistogramEnumeration(kFetchAccessTokenResultHistogram, error.state(),
                                GoogleServiceAuthError::NUM_STATES);
  access_token_fetcher_.reset();
  if (error.state() != GoogleServiceAuthError::NONE) {
    LOG(ERROR)
        << "Could not get access token to create ReAuthProofToken for parent"
        << error.ToString();
    base::DictionaryValue result;
    result.SetBoolKey("isWrongPassword", false);
    RejectJavascriptCallback(base::Value(parent_signin_callback_id_), result);
    parent_signin_callback_id_.clear();
    return;
  }

  FetchReAuthProofTokenForParent(access_token_info.token,
                                 parent_obfuscated_gaia_id, parent_credential);
}

void EduAccountLoginHandler::OnReAuthProofTokenSuccess(
    const std::string& reauth_proof_token) {
  base::UmaHistogramEnumeration(
      kCreateRaptResultHistogram,
      GaiaAuthConsumer::ReAuthProofTokenStatus::kSuccess);
  gaia_auth_fetcher_.reset();
  ResolveJavascriptCallback(base::Value(parent_signin_callback_id_),
                            base::Value(reauth_proof_token));
  parent_signin_callback_id_.clear();
}

void EduAccountLoginHandler::OnReAuthProofTokenFailure(
    const GaiaAuthConsumer::ReAuthProofTokenStatus error) {
  base::UmaHistogramEnumeration(kCreateRaptResultHistogram, error);
  LOG(ERROR) << "Failed to fetch ReAuthProofToken for the parent, error="
             << static_cast<int>(error);
  gaia_auth_fetcher_.reset();

  base::DictionaryValue result;
  result.SetBoolKey(
      "isWrongPassword",
      error == GaiaAuthConsumer::ReAuthProofTokenStatus::kInvalidGrant);
  RejectJavascriptCallback(base::Value(parent_signin_callback_id_), result);
  parent_signin_callback_id_.clear();
}

}  // namespace chromeos
