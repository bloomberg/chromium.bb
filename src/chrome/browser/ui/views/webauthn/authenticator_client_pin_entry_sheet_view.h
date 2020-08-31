// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_CLIENT_PIN_ENTRY_SHEET_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_CLIENT_PIN_ENTRY_SHEET_VIEW_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/ui/views/webauthn/authenticator_client_pin_entry_view.h"
#include "chrome/browser/ui/views/webauthn/authenticator_request_sheet_view.h"
#include "chrome/browser/ui/webauthn/sheet_models.h"

// Web Authentication request dialog sheet view for entering an authenticator
// PIN.
class AuthenticatorClientPinEntrySheetView
    : public AuthenticatorRequestSheetView,
      public AuthenticatorClientPinEntryView::Delegate {
 public:
  explicit AuthenticatorClientPinEntrySheetView(
      std::unique_ptr<AuthenticatorClientPinEntrySheetModel> model);
  ~AuthenticatorClientPinEntrySheetView() override;

 private:
  AuthenticatorClientPinEntrySheetModel* pin_entry_sheet_model();

  // AuthenticatorRequestSheetView:
  std::unique_ptr<views::View> BuildStepSpecificContent() override;

  // AuthenticatorClientPinEntryView::Delegate:
  void OnPincodeChanged(base::string16 pincode) override;
  void OnConfirmationChanged(base::string16 pincode) override;

  DISALLOW_COPY_AND_ASSIGN(AuthenticatorClientPinEntrySheetView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_CLIENT_PIN_ENTRY_SHEET_VIEW_H_
