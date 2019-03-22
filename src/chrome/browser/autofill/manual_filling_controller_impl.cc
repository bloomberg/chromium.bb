// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/manual_filling_controller_impl.h"

#include <utility>

#include "base/callback.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/password_manager/password_accessory_controller.h"
#include "chrome/browser/password_manager/password_accessory_metrics_util.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_contents.h"

using autofill::AccessorySheetData;

ManualFillingControllerImpl::~ManualFillingControllerImpl() = default;

// static
base::WeakPtr<ManualFillingController> ManualFillingController::GetOrCreate(
    content::WebContents* contents) {
  ManualFillingControllerImpl::CreateForWebContents(contents);
  return ManualFillingControllerImpl::FromWebContents(contents)->AsWeakPtr();
}

// static
void ManualFillingControllerImpl::CreateForWebContentsForTesting(
    content::WebContents* web_contents,
    base::WeakPtr<PasswordAccessoryController> pwd_controller,
    std::unique_ptr<ManualFillingViewInterface> view) {
  DCHECK(web_contents) << "Need valid WebContents to attach controller to!";
  DCHECK(!FromWebContents(web_contents)) << "Controller already attached!";
  DCHECK(pwd_controller);
  DCHECK(view);

  web_contents->SetUserData(
      UserDataKey(),
      base::WrapUnique(new ManualFillingControllerImpl(
          web_contents, std::move(pwd_controller), std::move(view))));
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
    bool is_fillable,
    const AccessorySheetData& accessory_sheet_data) {
  view_->OnItemsAvailable(accessory_sheet_data);

  // TODO(crbug.com/905669): The decision for showing the sheet or not will need
  // to take into account if Autofill suggestions are also available.
  if (is_fillable) {
    view_->SwapSheetWithKeyboard();
  } else {
    view_->CloseAccessorySheet();
  }
}

void ManualFillingControllerImpl::ShowWhenKeyboardIsVisible() {
  view_->ShowWhenKeyboardIsVisible();
}

void ManualFillingControllerImpl::Hide() {
  view_->Hide();
}

void ManualFillingControllerImpl::OnFillingTriggered(
    bool is_password,
    const base::string16& text_to_fill) {
  DCHECK(pwd_controller_);
  pwd_controller_->OnFillingTriggered(is_password, text_to_fill);
}

void ManualFillingControllerImpl::OnOptionSelected(
    const base::string16& selected_option) const {
  DCHECK(pwd_controller_);
  pwd_controller_->OnOptionSelected(selected_option);
}

void ManualFillingControllerImpl::OnGenerationRequested() {
  DCHECK(pwd_controller_);
  pwd_controller_->OnGenerationRequested();
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
}

ManualFillingControllerImpl::ManualFillingControllerImpl(
    content::WebContents* web_contents,
    base::WeakPtr<PasswordAccessoryController> pwd_controller,
    std::unique_ptr<ManualFillingViewInterface> view)
    : web_contents_(web_contents),
      pwd_controller_(std::move(pwd_controller)),
      view_(std::move(view)),
      weak_factory_(this) {}
