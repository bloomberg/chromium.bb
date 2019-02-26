// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/password_generation_popup_controller_impl.h"

#include <math.h>
#include <stddef.h>

#include <algorithm>

#include "base/i18n/rtl.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/ui/autofill/popup_constants.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/passwords/password_generation_popup_observer.h"
#include "chrome/browser/ui/passwords/password_generation_popup_view.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/core/browser/suggestion.h"
#include "components/password_manager/core/browser/password_bubble_experiment.h"
#include "components/password_manager/core/browser/password_generation_manager.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/range/range.h"
#include "ui/gfx/text_utils.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/preferences/preferences_launcher.h"
#endif

base::WeakPtr<PasswordGenerationPopupControllerImpl>
PasswordGenerationPopupControllerImpl::GetOrCreate(
    base::WeakPtr<PasswordGenerationPopupControllerImpl> previous,
    const gfx::RectF& bounds,
    const autofill::PasswordForm& form,
    const base::string16& generation_element,
    uint32_t max_length,
    password_manager::PasswordManager* password_manager,
    const base::WeakPtr<password_manager::PasswordManagerDriver>& driver,
    PasswordGenerationPopupObserver* observer,
    content::WebContents* web_contents,
    gfx::NativeView container_view) {
  if (previous.get() && previous->element_bounds() == bounds &&
      previous->web_contents_ == web_contents &&
      previous->container_view() == container_view) {
    return previous;
  }

  if (previous.get())
    previous->Hide();

  PasswordGenerationPopupControllerImpl* controller =
      new PasswordGenerationPopupControllerImpl(
          bounds, form, generation_element, max_length, driver, observer,
          web_contents, container_view);
  return controller->GetWeakPtr();
}

PasswordGenerationPopupControllerImpl::PasswordGenerationPopupControllerImpl(
    const gfx::RectF& bounds,
    const autofill::PasswordForm& form,
    const base::string16& generation_element,
    uint32_t max_length,
    const base::WeakPtr<password_manager::PasswordManagerDriver>& driver,
    PasswordGenerationPopupObserver* observer,
    content::WebContents* web_contents,
    gfx::NativeView container_view)
    : view_(nullptr),
      form_(form),
      driver_(driver),
      observer_(observer),
      form_signature_(autofill::CalculateFormSignature(form.form_data)),
      field_signature_(
          autofill::CalculateFieldSignatureByNameAndType(generation_element,
                                                         "password")),
      max_length_(max_length),
      // TODO(estade): use correct text direction.
      controller_common_(bounds, base::i18n::LEFT_TO_RIGHT, container_view),
      password_selected_(false),
      state_(kOfferGeneration),
      web_contents_(web_contents),
      weak_ptr_factory_(this) {
  help_text_ = l10n_util::GetStringUTF16(IDS_PASSWORD_GENERATION_PROMPT);
}

PasswordGenerationPopupControllerImpl::
    ~PasswordGenerationPopupControllerImpl() {}

base::WeakPtr<PasswordGenerationPopupControllerImpl>
PasswordGenerationPopupControllerImpl::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

bool PasswordGenerationPopupControllerImpl::HandleKeyPressEvent(
    const content::NativeWebKeyboardEvent& event) {
  switch (event.windows_key_code) {
    case ui::VKEY_UP:
    case ui::VKEY_DOWN:
      PasswordSelected(true);
      return true;
    case ui::VKEY_ESCAPE:
      Hide();
      return true;
    case ui::VKEY_RETURN:
    case ui::VKEY_TAB:
      // We suppress tab if the password is selected because we will
      // automatically advance focus anyway.
      return PossiblyAcceptPassword();
    default:
      return false;
  }
}

bool PasswordGenerationPopupControllerImpl::PossiblyAcceptPassword() {
  if (password_selected_) {
    PasswordAccepted();  // This will delete |this|.
    return true;
  }

  return false;
}

void PasswordGenerationPopupControllerImpl::PasswordSelected(bool selected) {
  if (state_ == kEditGeneratedPassword || selected == password_selected_)
    return;

  password_selected_ = selected;
  view_->PasswordSelectionUpdated();
}

void PasswordGenerationPopupControllerImpl::PasswordAccepted() {
  if (state_ != kOfferGeneration)
    return;

  driver_->GeneratedPasswordAccepted(current_password_);
  Hide();
}

void PasswordGenerationPopupControllerImpl::Show(GenerationState state) {
  // When switching from editing to generation state, regenerate the password.
  if (state == kOfferGeneration &&
      (state_ != state || current_password_.empty())) {
    uint32_t spec_priority = 0;
    current_password_ =
        driver_->GetPasswordGenerationManager()->GeneratePassword(
            web_contents_->GetLastCommittedURL().GetOrigin(), form_signature_,
            field_signature_, max_length_, &spec_priority);
    if (driver_ && driver_->GetPasswordManager()) {
      driver_->GetPasswordManager()->ReportSpecPriorityForGeneratedPassword(
          form_, spec_priority);
    }
  }
  state_ = state;

  if (!view_) {
    view_ = PasswordGenerationPopupView::Create(this);

    // Treat popup as being hidden if creation fails.
    if (!view_) {
      Hide();
      return;
    }

    view_->Show();
  } else {
    view_->UpdateState();
    view_->UpdateBoundsAndRedrawPopup();
  }

  static_cast<autofill::ContentAutofillDriver*>(driver_->GetAutofillDriver())
      ->RegisterKeyPressHandler(base::BindRepeating(
          &PasswordGenerationPopupControllerImpl::HandleKeyPressEvent,
          base::Unretained(this)));

  if (observer_)
    observer_->OnPopupShown(state_);
}

void PasswordGenerationPopupControllerImpl::UpdatePassword(
    base::string16 new_password) {
  current_password_ = std::move(new_password);
  if (view_)
    view_->UpdatePasswordValue();
}

void PasswordGenerationPopupControllerImpl::HideAndDestroy() {
  Hide();
}

void PasswordGenerationPopupControllerImpl::Hide() {
  if (driver_) {
    static_cast<autofill::ContentAutofillDriver*>(driver_->GetAutofillDriver())
        ->RemoveKeyPressHandler();
  }

  if (view_)
    view_->Hide();

  if (observer_)
    observer_->OnPopupHidden();

  delete this;
}

void PasswordGenerationPopupControllerImpl::ViewDestroyed() {
  view_ = NULL;

  Hide();
}

void PasswordGenerationPopupControllerImpl::OnSavedPasswordsLinkClicked() {
  NOTREACHED();
}

void PasswordGenerationPopupControllerImpl::SetSelectionAtPoint(
    const gfx::Point& point) {
  PasswordSelected(view_->IsPointInPasswordBounds(point));
}

bool PasswordGenerationPopupControllerImpl::AcceptSelectedLine() {
  if (!password_selected_)
    return false;

  PasswordAccepted();
  return true;
}

void PasswordGenerationPopupControllerImpl::SelectionCleared() {
  PasswordSelected(false);
}

bool PasswordGenerationPopupControllerImpl::HasSelection() const {
  return password_selected();
}

gfx::NativeView PasswordGenerationPopupControllerImpl::container_view() const {
  return controller_common_.container_view;
}

gfx::Rect PasswordGenerationPopupControllerImpl::popup_bounds() const {
  NOTREACHED();
  return gfx::Rect();
}

const gfx::RectF& PasswordGenerationPopupControllerImpl::element_bounds()
    const {
  return controller_common_.element_bounds;
}

bool PasswordGenerationPopupControllerImpl::IsRTL() const {
  return base::i18n::IsRTL();
}

const std::vector<autofill::Suggestion>
PasswordGenerationPopupControllerImpl::GetSuggestions() {
  return std::vector<autofill::Suggestion>();
}

#if !defined(OS_ANDROID)
void PasswordGenerationPopupControllerImpl::SetTypesetter(
    gfx::Typesetter typesetter) {}

int PasswordGenerationPopupControllerImpl::GetElidedValueWidthForRow(int row) {
  return 0;
}

int PasswordGenerationPopupControllerImpl::GetElidedLabelWidthForRow(int row) {
  return 0;
}
#endif

PasswordGenerationPopupController::GenerationState
PasswordGenerationPopupControllerImpl::state() const {
  return state_;
}

bool PasswordGenerationPopupControllerImpl::password_selected() const {
  return password_selected_;
}

const base::string16& PasswordGenerationPopupControllerImpl::password() const {
  return current_password_;
}

base::string16 PasswordGenerationPopupControllerImpl::SuggestedText() {
  return l10n_util::GetStringUTF16(
      state_ == kOfferGeneration ? IDS_PASSWORD_GENERATION_SUGGESTION
                                 : IDS_PASSWORD_GENERATION_EDITING_SUGGESTION);
}

const base::string16& PasswordGenerationPopupControllerImpl::HelpText() {
  return help_text_;
}

gfx::Range PasswordGenerationPopupControllerImpl::HelpTextLinkRange() {
  return gfx::Range();
}
