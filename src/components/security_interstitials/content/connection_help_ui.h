// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_CONNECTION_HELP_UI_H_
#define COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_CONNECTION_HELP_UI_H_

#include "base/macros.h"
#include "content/public/browser/web_ui_controller.h"

namespace security_interstitials {

// The WebUI for chrome://connection-help, which provides help content to users
// with network configuration problems that prevent them from making secure
// connections.
class ConnectionHelpUI : public content::WebUIController {
 public:
  explicit ConnectionHelpUI(content::WebUI* web_ui);
  ~ConnectionHelpUI() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ConnectionHelpUI);
};

}  // namespace security_interstitials

#endif  // COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_CONNECTION_HELP_UI_H_
