// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/authenticator_transport_selector_sheet_view.h"

#include <utility>

#include "chrome/browser/webauthn/authenticator_transport.h"
#include "device/fido/features.h"

AuthenticatorTransportSelectorSheetView::
    AuthenticatorTransportSelectorSheetView(
        std::unique_ptr<AuthenticatorTransportSelectorSheetModel> model)
    : AuthenticatorRequestSheetView(std::move(model)) {}

AuthenticatorTransportSelectorSheetView::
    ~AuthenticatorTransportSelectorSheetView() = default;

std::unique_ptr<views::View>
AuthenticatorTransportSelectorSheetView::BuildStepSpecificContent() {
  AuthenticatorRequestDialogModel* const dialog_model = model()->dialog_model();
  base::flat_set<AuthenticatorTransport> transports =
      dialog_model->available_transports();
  // kAndroidAccessory is never shown as an option to the user. It's a fallback
  // for caBLE.
  transports.erase(AuthenticatorTransport::kAndroidAccessory);

  return std::make_unique<HoverListView>(
      std::make_unique<TransportHoverListModel>(
          transports, dialog_model->win_native_api_enabled(), model()));
}
