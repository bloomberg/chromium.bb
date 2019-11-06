// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBAUTHN_AUTHENTICATOR_REQUEST_DIALOG_H_
#define CHROME_BROWSER_UI_WEBAUTHN_AUTHENTICATOR_REQUEST_DIALOG_H_

#include <memory>

class AuthenticatorRequestDialogModel;

namespace content {
class WebContents;
}

// Creates and shows the dialog for a given WebContents.
void ShowAuthenticatorRequestDialog(
    content::WebContents* web_contents,
    std::unique_ptr<AuthenticatorRequestDialogModel> model);

#endif  // CHROME_BROWSER_UI_WEBAUTHN_AUTHENTICATOR_REQUEST_DIALOG_H_
