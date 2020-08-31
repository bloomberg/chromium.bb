// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/webauthn_dialog_model.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/autofill/payments/webauthn_dialog_model_observer.h"
#include "chrome/browser/ui/autofill/payments/webauthn_dialog_state.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"

namespace autofill {

WebauthnDialogModel::WebauthnDialogModel(WebauthnDialogState dialog_state)
    : state_(dialog_state) {}

WebauthnDialogModel::~WebauthnDialogModel() = default;

void WebauthnDialogModel::SetDialogState(WebauthnDialogState state) {
  state_ = state;
  for (WebauthnDialogModelObserver& observer : observers_)
    observer.OnDialogStateChanged();
}

void WebauthnDialogModel::AddObserver(WebauthnDialogModelObserver* observer) {
  observers_.AddObserver(observer);
}

void WebauthnDialogModel::RemoveObserver(
    WebauthnDialogModelObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool WebauthnDialogModel::IsActivityIndicatorVisible() const {
  return state_ == WebauthnDialogState::kOfferPending ||
         state_ == WebauthnDialogState::kVerifyPending;
}

bool WebauthnDialogModel::IsBackButtonVisible() const {
  return false;
}

bool WebauthnDialogModel::IsCancelButtonVisible() const {
  return true;
}

base::string16 WebauthnDialogModel::GetCancelButtonLabel() const {
  switch (state_) {
    case WebauthnDialogState::kOffer:
    case WebauthnDialogState::kOfferPending:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_WEBAUTHN_OPT_IN_DIALOG_CANCEL_BUTTON_LABEL);
    case WebauthnDialogState::kOfferError:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_WEBAUTHN_OPT_IN_DIALOG_CANCEL_BUTTON_LABEL_ERROR);
    case WebauthnDialogState::kVerifyPending:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_WEBAUTHN_VERIFY_PENDING_DIALOG_CANCEL_BUTTON_LABEL);
    case WebauthnDialogState::kInactive:
    case WebauthnDialogState::kUnknown:
      break;
  }
  return base::string16();
}

bool WebauthnDialogModel::IsAcceptButtonVisible() const {
  return state_ == WebauthnDialogState::kOffer ||
         state_ == WebauthnDialogState::kOfferPending;
}

bool WebauthnDialogModel::IsAcceptButtonEnabled() const {
  return state_ != WebauthnDialogState::kOfferPending;
}

base::string16 WebauthnDialogModel::GetAcceptButtonLabel() const {
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_WEBAUTHN_OPT_IN_DIALOG_OK_BUTTON_LABEL);
}

const gfx::VectorIcon& WebauthnDialogModel::GetStepIllustration(
    ImageColorScheme color_scheme) const {
  switch (state_) {
    case WebauthnDialogState::kOffer:
    case WebauthnDialogState::kOfferPending:
    case WebauthnDialogState::kVerifyPending:
      return color_scheme == ImageColorScheme::kDark
                 ? kWebauthnDialogHeaderDarkIcon
                 : kWebauthnDialogHeaderIcon;
    case WebauthnDialogState::kOfferError:
      return color_scheme == ImageColorScheme::kDark ? kWebauthnErrorDarkIcon
                                                     : kWebauthnErrorIcon;
    case WebauthnDialogState::kInactive:
    case WebauthnDialogState::kUnknown:
      break;
  }
  NOTREACHED();
  return gfx::kNoneIcon;
}

base::string16 WebauthnDialogModel::GetStepTitle() const {
  switch (state_) {
    case WebauthnDialogState::kOffer:
    case WebauthnDialogState::kOfferPending:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_WEBAUTHN_OPT_IN_DIALOG_TITLE);
    case WebauthnDialogState::kOfferError:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_WEBAUTHN_OPT_IN_DIALOG_TITLE_ERROR);
    case WebauthnDialogState::kVerifyPending:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_WEBAUTHN_VERIFY_PENDING_DIALOG_TITLE);
    case WebauthnDialogState::kInactive:
    case WebauthnDialogState::kUnknown:
      break;
  }
  NOTREACHED();
  return base::string16();
}

base::string16 WebauthnDialogModel::GetStepDescription() const {
  switch (state_) {
    case WebauthnDialogState::kOffer:
    case WebauthnDialogState::kOfferPending:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_WEBAUTHN_OPT_IN_DIALOG_INSTRUCTION);
    case WebauthnDialogState::kOfferError:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_WEBAUTHN_OPT_IN_DIALOG_INSTRUCTION_ERROR);
    case WebauthnDialogState::kVerifyPending:
      return base::string16();
    case WebauthnDialogState::kInactive:
    case WebauthnDialogState::kUnknown:
      break;
  }
  NOTREACHED();
  return base::string16();
}

}  // namespace autofill
