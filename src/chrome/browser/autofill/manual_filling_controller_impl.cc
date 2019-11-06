// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/manual_filling_controller_impl.h"

#include <utility>

#include "base/callback.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/autofill/address_accessory_controller.h"
#include "chrome/browser/password_manager/password_accessory_controller.h"
#include "chrome/browser/password_manager/password_accessory_metrics_util.h"
#include "chrome/browser/password_manager/password_generation_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "content/public/browser/web_contents.h"

using autofill::AccessoryAction;
using autofill::AccessorySheetData;
using autofill::AddressAccessoryController;
using autofill::mojom::FocusedFieldType;

ManualFillingControllerImpl::~ManualFillingControllerImpl() = default;

// static
base::WeakPtr<ManualFillingController> ManualFillingController::GetOrCreate(
    content::WebContents* contents) {
  ManualFillingControllerImpl* mf_controller =
      ManualFillingControllerImpl::FromWebContents(contents);
  if (!mf_controller) {
    ManualFillingControllerImpl::CreateForWebContents(contents);
    mf_controller = ManualFillingControllerImpl::FromWebContents(contents);
    mf_controller->Initialize();
  }
  return mf_controller->AsWeakPtr();
}

// static
void ManualFillingControllerImpl::CreateForWebContentsForTesting(
    content::WebContents* web_contents,
    base::WeakPtr<PasswordAccessoryController> pwd_controller,
    base::WeakPtr<AddressAccessoryController> address_controller,
    PasswordGenerationController* pwd_generation_controller_for_testing,
    std::unique_ptr<ManualFillingViewInterface> view) {
  DCHECK(web_contents) << "Need valid WebContents to attach controller to!";
  DCHECK(!FromWebContents(web_contents)) << "Controller already attached!";
  DCHECK(pwd_controller);
  DCHECK(address_controller);
  DCHECK(pwd_generation_controller_for_testing);
  DCHECK(view);

  web_contents->SetUserData(
      UserDataKey(),
      base::WrapUnique(new ManualFillingControllerImpl(
          web_contents, std::move(pwd_controller),
          std::move(address_controller), pwd_generation_controller_for_testing,
          std::move(view))));

  FromWebContents(web_contents)->Initialize();
}

void ManualFillingControllerImpl::OnAutomaticGenerationStatusChanged(
    bool available) {
  DCHECK(view_);
  view_->OnAutomaticGenerationStatusChanged(available);
}

void ManualFillingControllerImpl::OnFilledIntoFocusedField(
    autofill::FillingStatus status) {
  if (status != autofill::FillingStatus::SUCCESS)
    return;  // TODO(crbug.com/853766): Record success rate.
  view_->SwapSheetWithKeyboard();
}

void ManualFillingControllerImpl::RefreshSuggestionsForField(
    FocusedFieldType focused_field_type,
    const AccessorySheetData& accessory_sheet_data) {
  view_->OnItemsAvailable(accessory_sheet_data);

  // TODO(crbug.com/965478): Refresh visibility for non-PWDs in V2.
  if (accessory_sheet_data.get_sheet_type() !=
      autofill::AccessoryTabType::PASSWORDS)
    return;

  // TODO(crbug.com/905669): The decision for showing the sheet or not will need
  // to take into account if Autofill suggestions are also available.
  if (autofill::IsFillable(focused_field_type))
    view_->SwapSheetWithKeyboard();
  else
    view_->CloseAccessorySheet();
}

void ManualFillingControllerImpl::ShowWhenKeyboardIsVisible(
    FillingSource source) {
  if (source == FillingSource::AUTOFILL &&
      !base::FeatureList::IsEnabled(
          autofill::features::kAutofillKeyboardAccessory)) {
    // Ignore autofill signals if the feature is disabled.
    return;
  }
  visible_sources_.insert(source);
  view_->ShowWhenKeyboardIsVisible();
}

void ManualFillingControllerImpl::ShowTouchToFillSheet() {
  if (!base::FeatureList::IsEnabled(
          password_manager::features::kTouchToFillAndroid)) {
    return;
  }

  view_->ShowTouchToFillSheet();
}

void ManualFillingControllerImpl::Hide(FillingSource source) {
  if (source == FillingSource::AUTOFILL &&
      !base::FeatureList::IsEnabled(
          autofill::features::kAutofillKeyboardAccessory)) {
    // Ignore autofill signals if the feature is disabled.
    return;
  }
  visible_sources_.erase(source);
  if (visible_sources_.empty())
    view_->Hide();
}

void ManualFillingControllerImpl::OnFillingTriggered(
    autofill::AccessoryTabType type,
    const autofill::UserInfo::Field& selection) {
  GetControllerForTab(type)->OnFillingTriggered(selection);
}

void ManualFillingControllerImpl::OnOptionSelected(
    AccessoryAction selected_action) const {
  UMA_HISTOGRAM_ENUMERATION("KeyboardAccessory.AccessoryActionSelected",
                            selected_action, AccessoryAction::COUNT);
  GetControllerForAction(selected_action)->OnOptionSelected(selected_action);
}

void ManualFillingControllerImpl::OnGenerationRequested() {
  PasswordGenerationController* pwd_generation_controller =
      pwd_generation_controller_for_testing_
          ? pwd_generation_controller_for_testing_
          : PasswordGenerationController::GetIfExisting(web_contents_);

  DCHECK(pwd_generation_controller);
  pwd_generation_controller->OnGenerationRequested();
}

void ManualFillingControllerImpl::GetFavicon(
    int desired_size_in_pixel,
    base::OnceCallback<void(const gfx::Image&)> icon_callback) {
  DCHECK(pwd_controller_);
  pwd_controller_->GetFavicon(desired_size_in_pixel, std::move(icon_callback));
}

gfx::NativeView ManualFillingControllerImpl::container_view() const {
  return web_contents_->GetNativeView();
}

// Returns a weak pointer for this object.
base::WeakPtr<ManualFillingController>
ManualFillingControllerImpl::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void ManualFillingControllerImpl::Initialize() {
  DCHECK(FromWebContents(web_contents_)) << "Don't call from constructor!";
  if (address_controller_)
    address_controller_->RefreshSuggestions();
}

ManualFillingControllerImpl::ManualFillingControllerImpl(
    content::WebContents* web_contents)
    : web_contents_(web_contents),
      view_(ManualFillingViewInterface::Create(this)),
      weak_factory_(this) {
  if (PasswordAccessoryController::AllowedForWebContents(web_contents)) {
    pwd_controller_ =
        PasswordAccessoryController::GetOrCreate(web_contents)->AsWeakPtr();
    DCHECK(pwd_controller_);
  }
  if (AddressAccessoryController::AllowedForWebContents(web_contents)) {
    address_controller_ =
        AddressAccessoryController::GetOrCreate(web_contents)->AsWeakPtr();
    DCHECK(address_controller_);
  }
}

ManualFillingControllerImpl::ManualFillingControllerImpl(
    content::WebContents* web_contents,
    base::WeakPtr<PasswordAccessoryController> pwd_controller,
    base::WeakPtr<AddressAccessoryController> address_controller,
    PasswordGenerationController* pwd_generation_controller_for_testing,
    std::unique_ptr<ManualFillingViewInterface> view)
    : web_contents_(web_contents),
      pwd_controller_(std::move(pwd_controller)),
      address_controller_(std::move(address_controller)),
      pwd_generation_controller_for_testing_(
          pwd_generation_controller_for_testing),
      view_(std::move(view)),
      weak_factory_(this) {}

AccessoryController* ManualFillingControllerImpl::GetControllerForTab(
    autofill::AccessoryTabType type) {
  switch (type) {
    case autofill::AccessoryTabType::ADDRESSES:
      return address_controller_.get();
    case autofill::AccessoryTabType::PASSWORDS:
      return pwd_controller_.get();
    case autofill::AccessoryTabType::CREDIT_CARDS:
      // TODO(crbug.com/902425): return credit card controller.
    case autofill::AccessoryTabType::ALL:
    case autofill::AccessoryTabType::COUNT:
      break;  // Intentional failure.
  }
  NOTREACHED() << "Controller not defined for tab: " << static_cast<int>(type);
  return nullptr;
}

AccessoryController* ManualFillingControllerImpl::GetControllerForAction(
    AccessoryAction action) const {
  switch (action) {
    case AccessoryAction::GENERATE_PASSWORD_MANUAL:
    case AccessoryAction::MANAGE_PASSWORDS:
      return pwd_controller_.get();
    case AccessoryAction::MANAGE_ADDRESSES:
      return address_controller_.get();
    case AccessoryAction::MANAGE_CREDIT_CARDS:
      // TODO(crbug.com/902425): Return credit card controller.
    case AccessoryAction::AUTOFILL_SUGGESTION:
    case AccessoryAction::COUNT:
    case AccessoryAction::GENERATE_PASSWORD_AUTOMATIC:
      break;  // Intentional failure;
  }
  NOTREACHED() << "Controller not defined for action: "
               << static_cast<int>(action);
  return nullptr;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ManualFillingControllerImpl)
