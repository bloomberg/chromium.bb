// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_accessory_controller_impl.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/android/preferences/preferences_launcher.h"
#include "chrome/browser/autofill/manual_filling_controller.h"
#include "chrome/browser/autofill/manual_filling_utils.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/password_manager/password_accessory_metrics_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/passwords/manage_passwords_view_utils.h"
#include "chrome/browser/vr/vr_tab_helper.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/password_form.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "components/favicon/core/favicon_service.h"
#include "components/favicon_base/favicon_types.h"
#include "components/password_manager/content/browser/content_password_manager_driver.h"
#include "components/password_manager/content/browser/content_password_manager_driver_factory.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

using autofill::AccessorySheetData;
using autofill::FooterCommand;
using autofill::PasswordForm;
using autofill::UserInfo;
using autofill::mojom::FocusedFieldType;
using FillingSource = ManualFillingController::FillingSource;

namespace {

autofill::UserInfo TranslateCredentials(
    bool current_field_is_password,
    const PasswordAccessorySuggestion& data) {
  UserInfo user_info;

  user_info.add_field(UserInfo::Field(
      data.username, data.username, /*is_password=*/false,
      /*selectable=*/data.username_selectable && !current_field_is_password));

  user_info.add_field(UserInfo::Field(
      data.password,
      l10n_util::GetStringFUTF16(
          IDS_PASSWORD_MANAGER_ACCESSORY_PASSWORD_DESCRIPTION, data.username),
      /*is_password=*/true, /*selectable=*/current_field_is_password));

  return user_info;
}

base::string16 GetTitle(bool has_suggestions, const url::Origin& origin) {
  const base::string16 elided_url =
      url_formatter::FormatOriginForSecurityDisplay(
          origin, url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC);
  return l10n_util::GetStringFUTF16(
      has_suggestions
          ? IDS_PASSWORD_MANAGER_ACCESSORY_PASSWORD_LIST_TITLE
          : IDS_PASSWORD_MANAGER_ACCESSORY_PASSWORD_LIST_EMPTY_MESSAGE,
      elided_url);
}

}  // namespace

PasswordAccessoryControllerImpl::~PasswordAccessoryControllerImpl() = default;

void PasswordAccessoryControllerImpl::OnFillingTriggered(
    const autofill::UserInfo::Field& selection) {
  if (!AppearsInSuggestions(selection.display_text(), selection.is_obfuscated(),
                            GetFocusedFrameOrigin())) {
    NOTREACHED() << "Tried to fill '" << selection.display_text() << "' into "
                 << GetFocusedFrameOrigin();
    return;  // Never fill across different origins!
  }

  password_manager::ContentPasswordManagerDriverFactory* factory =
      password_manager::ContentPasswordManagerDriverFactory::FromWebContents(
          web_contents_);
  DCHECK(factory);
  password_manager::ContentPasswordManagerDriver* driver =
      factory->GetDriverForFrame(web_contents_->GetFocusedFrame());
  if (!driver) {
    return;
  }  // |driver| can be NULL if the tab is being closed.
  driver->FillIntoFocusedField(
      selection.is_obfuscated(), selection.display_text(),
      base::BindOnce(&PasswordAccessoryControllerImpl::OnFilledIntoFocusedField,
                     base::AsWeakPtr<PasswordAccessoryControllerImpl>(this)));
}

// static
bool PasswordAccessoryController::AllowedForWebContents(
    content::WebContents* web_contents) {
  DCHECK(web_contents) << "Need valid WebContents to attach controller to!";
  if (vr::VrTabHelper::IsInVr(web_contents)) {
    return false;  // TODO(crbug.com/902305): Re-enable if possible.
  }
  return base::FeatureList::IsEnabled(
      password_manager::features::kPasswordsKeyboardAccessory);
}

// static
PasswordAccessoryController* PasswordAccessoryController::GetOrCreate(
    content::WebContents* web_contents) {
  DCHECK(PasswordAccessoryController::AllowedForWebContents(web_contents));

  PasswordAccessoryControllerImpl::CreateForWebContents(web_contents);
  return PasswordAccessoryControllerImpl::FromWebContents(web_contents);
}

// static
PasswordAccessoryController* PasswordAccessoryController::GetIfExisting(
    content::WebContents* web_contents) {
  return PasswordAccessoryControllerImpl::FromWebContents(web_contents);
}

struct PasswordAccessoryControllerImpl::FaviconRequestData {
  // List of requests waiting for favicons to be available.
  std::vector<base::OnceCallback<void(const gfx::Image&)>> pending_requests;

  // Cached image for this origin. |IsEmpty()| unless a favicon was found.
  gfx::Image cached_icon;
};

// static
void PasswordAccessoryControllerImpl::CreateForWebContentsForTesting(
    content::WebContents* web_contents,
    base::WeakPtr<ManualFillingController> mf_controller,
    favicon::FaviconService* favicon_service) {
  DCHECK(web_contents) << "Need valid WebContents to attach controller to!";
  DCHECK(!FromWebContents(web_contents)) << "Controller already attached!";
  DCHECK(mf_controller);

  web_contents->SetUserData(
      UserDataKey(),
      base::WrapUnique(new PasswordAccessoryControllerImpl(
          web_contents, std::move(mf_controller), favicon_service)));
}

void PasswordAccessoryControllerImpl::SavePasswordsForOrigin(
    const std::map<base::string16, const PasswordForm*>& best_matches,
    const url::Origin& origin) {
  std::vector<PasswordAccessorySuggestion>* suggestions =
      &origin_suggestions_[origin];
  suggestions->clear();
  for (const auto& pair : best_matches) {
    const PasswordForm* form = pair.second;
    if (!url::IsSameOriginWith(origin.GetURL(), form->origin) &&
        !base::FeatureList::IsEnabled(
            autofill::features::kAutofillKeyboardAccessory))
      continue;  // Skip matches for PSL origins in V1 which has no UI for that.
    suggestions->emplace_back(form->password_value, GetDisplayUsername(*form),
                              /*selectable=*/!form->username_value.empty());
  }
}

void PasswordAccessoryControllerImpl::OnFilledIntoFocusedField(
    autofill::FillingStatus status) {
  GetManualFillingController()->OnFilledIntoFocusedField(status);
}

void PasswordAccessoryControllerImpl::OnOptionSelected(
    autofill::AccessoryAction selected_action) {
  if (selected_action == autofill::AccessoryAction::MANAGE_PASSWORDS) {
    chrome::android::PreferencesLauncher::ShowPasswordSettings(
        web_contents_,
        password_manager::ManagePasswordsReferrer::kPasswordsAccessorySheet);
    return;
  }
  if (selected_action == autofill::AccessoryAction::GENERATE_PASSWORD_MANUAL) {
    // TODO(https://crbug.com/835234): Invoke manual generation.
    return;
  }
  NOTREACHED() << "Unhandled selected action: "
               << static_cast<int>(selected_action);
}

void PasswordAccessoryControllerImpl::RefreshSuggestionsForField(
    FocusedFieldType focused_field_type,
    bool is_manual_generation_available) {
  // Prevent crashing by not acting at all if frame became unfocused at any
  // point. The next time a focus event happens, this will be called again and
  // ensure we show correct data.
  if (web_contents_->GetFocusedFrame() == nullptr)
    return;
  url::Origin origin = GetFocusedFrameOrigin();
  if (origin.opaque())
    return;  // Don't proceed for invalid origins.
  std::vector<UserInfo> info_to_add;
  std::vector<FooterCommand> footer_commands_to_add;

  const bool is_password_field =
      focused_field_type == FocusedFieldType::kFillablePasswordField;

  if (autofill::IsFillable(focused_field_type)) {
    const std::vector<PasswordAccessorySuggestion> suggestions =
        GetSuggestions();
    info_to_add.resize(suggestions.size());
    std::transform(suggestions.begin(), suggestions.end(), info_to_add.begin(),
                   [is_password_field](const auto& x) {
                     return TranslateCredentials(is_password_field, x);
                   });
  }

  if (is_password_field && is_manual_generation_available) {
    base::string16 generate_password_title = l10n_util::GetStringUTF16(
        IDS_PASSWORD_MANAGER_ACCESSORY_GENERATE_PASSWORD_BUTTON_TITLE);
    footer_commands_to_add.push_back(
        FooterCommand(generate_password_title,
                      autofill::AccessoryAction::GENERATE_PASSWORD_MANUAL));
  }

  base::string16 manage_passwords_title = l10n_util::GetStringUTF16(
      IDS_PASSWORD_MANAGER_ACCESSORY_ALL_PASSWORDS_LINK);
  footer_commands_to_add.push_back(FooterCommand(
      manage_passwords_title, autofill::AccessoryAction::MANAGE_PASSWORDS));

  bool has_suggestions = !info_to_add.empty();

  GetManualFillingController()->RefreshSuggestionsForField(
      focused_field_type,
      autofill::CreateAccessorySheetData(autofill::AccessoryTabType::PASSWORDS,
                                         GetTitle(has_suggestions, origin),
                                         std::move(info_to_add),
                                         std::move(footer_commands_to_add)));

  if (is_password_field) {
    GetManualFillingController()->ShowWhenKeyboardIsVisible(
        FillingSource::PASSWORD_FALLBACKS);
  } else {
    GetManualFillingController()->Hide(FillingSource::PASSWORD_FALLBACKS);
  }
}

void PasswordAccessoryControllerImpl::DidNavigateMainFrame() {
  favicon_tracker_.TryCancelAll();  // If there is a request pending, cancel it.
  icons_request_data_.clear();
  origin_suggestions_.clear();
}

void PasswordAccessoryControllerImpl::GetFavicon(
    int desired_size_in_pixel,
    base::OnceCallback<void(const gfx::Image&)> icon_callback) {
  url::Origin origin =
      GetFocusedFrameOrigin();  // Copy origin in case it changes.
  if (origin.opaque())
    return;  // Don't proceed for invalid origins.
  // Check whether this request can be immediately answered with a cached icon.
  // It is empty if there wasn't at least one request that found an icon yet.
  FaviconRequestData* icon_request = &icons_request_data_[origin];
  if (!icon_request->cached_icon.IsEmpty()) {
    std::move(icon_callback).Run(icon_request->cached_icon);
    return;
  }
  if (!favicon_service_) {  // This might happen in tests.
    std::move(icon_callback).Run(gfx::Image());
    return;
  }

  // The cache is empty. Queue the callback.
  icon_request->pending_requests.emplace_back(std::move(icon_callback));
  if (icon_request->pending_requests.size() > 1)
    return;  // The favicon for this origin was already requested.

  favicon_service_->GetRawFaviconForPageURL(
      origin.GetURL(),
      {favicon_base::IconType::kFavicon, favicon_base::IconType::kTouchIcon,
       favicon_base::IconType::kTouchPrecomposedIcon,
       favicon_base::IconType::kWebManifestIcon},
      desired_size_in_pixel,
      /* fallback_to_host = */ true,
      base::BindRepeating(  // FaviconService doesn't support BindOnce yet.
          &PasswordAccessoryControllerImpl::OnImageFetched,
          base::AsWeakPtr<PasswordAccessoryControllerImpl>(this), origin),
      &favicon_tracker_);
}

PasswordAccessoryControllerImpl::PasswordAccessoryControllerImpl(
    content::WebContents* web_contents)
    : web_contents_(web_contents),
      favicon_service_(FaviconServiceFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()),
          ServiceAccessType::EXPLICIT_ACCESS)) {
}

// Additional creation functions in unit tests only:
PasswordAccessoryControllerImpl::PasswordAccessoryControllerImpl(
    content::WebContents* web_contents,
    base::WeakPtr<ManualFillingController> mf_controller,
    favicon::FaviconService* favicon_service)
    : web_contents_(web_contents),
      mf_controller_(std::move(mf_controller)),
      favicon_service_(favicon_service) {}

void PasswordAccessoryControllerImpl::OnImageFetched(
    url::Origin origin,
    const favicon_base::FaviconRawBitmapResult& bitmap_result) {
  FaviconRequestData* icon_request = &icons_request_data_[origin];

  favicon_base::FaviconImageResult image_result;
  if (bitmap_result.is_valid()) {
    image_result.image = gfx::Image::CreateFrom1xPNGBytes(
        bitmap_result.bitmap_data->front(), bitmap_result.bitmap_data->size());
  }
  icon_request->cached_icon = image_result.image;
  // Only trigger all the callbacks if they still affect a displayed origin.
  if (origin == GetFocusedFrameOrigin()) {
    for (auto& callback : icon_request->pending_requests) {
      std::move(callback).Run(icon_request->cached_icon);
    }
  }
  icon_request->pending_requests.clear();
}

bool PasswordAccessoryControllerImpl::AppearsInSuggestions(
    const base::string16& suggestion,
    bool is_password,
    const url::Origin& origin) const {
  if (origin.opaque())
    return false;  // Don't proceed for invalid origins.
  for (const PasswordAccessorySuggestion& element :
       origin_suggestions_.at(origin)) {
    const base::string16& candidate =
        is_password ? element.password : element.username;
    if (candidate == suggestion)
      return true;
  }
  return false;
}

base::WeakPtr<ManualFillingController>
PasswordAccessoryControllerImpl::GetManualFillingController() {
  if (!mf_controller_)
    mf_controller_ = ManualFillingController::GetOrCreate(web_contents_);
  DCHECK(mf_controller_);
  return mf_controller_;
}

url::Origin PasswordAccessoryControllerImpl::GetFocusedFrameOrigin() const {
  if (web_contents_->GetFocusedFrame() == nullptr) {
    LOG(DFATAL) << "Tried to get retrieve origin without focused "
                   "frame.";
    return url::Origin();  // Nonce!
  }
  return web_contents_->GetFocusedFrame()->GetLastCommittedOrigin();
}

std::vector<PasswordAccessorySuggestion>
PasswordAccessoryControllerImpl::GetSuggestions() const {
  auto it = origin_suggestions_.find(GetFocusedFrameOrigin());
  return it == origin_suggestions_.end()
             ? std::vector<PasswordAccessorySuggestion>()
             : it->second;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PasswordAccessoryControllerImpl)
