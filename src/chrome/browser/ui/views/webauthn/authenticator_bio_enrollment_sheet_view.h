// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_BIO_ENROLLMENT_SHEET_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_BIO_ENROLLMENT_SHEET_VIEW_H_

#include "chrome/browser/ui/views/webauthn/authenticator_request_sheet_view.h"
#include "chrome/browser/ui/webauthn/sheet_models.h"

class RingProgressBar;

// Represents a sheet in the Web Authentication request dialog that allows the
// user to input pin code used to connect to BLE security key.
class AuthenticatorBioEnrollmentSheetView
    : public AuthenticatorRequestSheetView {
 public:
  explicit AuthenticatorBioEnrollmentSheetView(
      std::unique_ptr<AuthenticatorBioEnrollmentSheetModel> sheet_model);
  ~AuthenticatorBioEnrollmentSheetView() override;

 private:
  // AuthenticatorRequestSheetView:
  std::unique_ptr<views::View> BuildStepSpecificContent() override;

  RingProgressBar* ring_progress_bar_;

  DISALLOW_COPY_AND_ASSIGN(AuthenticatorBioEnrollmentSheetView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_BIO_ENROLLMENT_SHEET_VIEW_H_
