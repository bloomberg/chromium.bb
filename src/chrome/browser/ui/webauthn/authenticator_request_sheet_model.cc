// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webauthn/authenticator_request_sheet_model.h"

base::string16 AuthenticatorRequestSheetModel::GetAdditionalDescription()
    const {
  return base::string16();
}

base::string16 AuthenticatorRequestSheetModel::GetError() const {
  return base::string16();
}

ui::MenuModel* AuthenticatorRequestSheetModel::GetOtherTransportsMenuModel() {
  return nullptr;
}
