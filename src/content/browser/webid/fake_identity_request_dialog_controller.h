// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_FAKE_IDENTITY_REQUEST_DIALOG_CONTROLLER_H_
#define CONTENT_BROWSER_WEBID_FAKE_IDENTITY_REQUEST_DIALOG_CONTROLLER_H_

#include <string>

#include "content/public/browser/identity_request_dialog_controller.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

// This fakes the request dialogs to always provide user consent.
// Used by tests and if the --use-fake-ui-for-fedcm command-line
// flag is provided.
class CONTENT_EXPORT FakeIdentityRequestDialogController
    : public IdentityRequestDialogController {
 public:
  explicit FakeIdentityRequestDialogController(
      absl::optional<std::string> selected_account);
  ~FakeIdentityRequestDialogController() override;

  void ShowAccountsDialog(content::WebContents* rp_web_contents,
                          const GURL& idp_signin_url,
                          base::span<const IdentityRequestAccount> accounts,
                          const IdentityProviderMetadata& idp_metadata,
                          const ClientIdData& client_id_data,
                          IdentityRequestAccount::SignInMode sign_in_mode,
                          AccountSelectionCallback on_selected) override;

 private:
  absl::optional<std::string> selected_account_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_FAKE_IDENTITY_REQUEST_DIALOG_CONTROLLER_H_
