// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_CRYPTOHOME_UI_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_CRYPTOHOME_UI_H_

#include "base/macros.h"
#include "content/public/browser/web_ui_controller.h"

namespace chromeos {

// WebUIController for chrome://cryptohome.
class CryptohomeUI : public content::WebUIController {
 public:
  explicit CryptohomeUI(content::WebUI* web_ui);
  ~CryptohomeUI() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(CryptohomeUI);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_CRYPTOHOME_UI_H_
