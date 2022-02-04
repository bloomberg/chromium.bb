// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_SYSTEM_EXTENSIONS_INTERNALS_UI_SYSTEM_EXTENSIONS_INTERNALS_UI_H_
#define ASH_WEBUI_SYSTEM_EXTENSIONS_INTERNALS_UI_SYSTEM_EXTENSIONS_INTERNALS_UI_H_

#include "ui/webui/mojo_web_ui_controller.h"

namespace ash {

// WebUIController for chrome://system-extensions-internals/.
class SystemExtensionsInternalsUI : public ui::MojoWebUIController {
 public:
  explicit SystemExtensionsInternalsUI(content::WebUI* web_ui);
  SystemExtensionsInternalsUI(const SystemExtensionsInternalsUI&) = delete;
  SystemExtensionsInternalsUI& operator=(const SystemExtensionsInternalsUI&) =
      delete;
  ~SystemExtensionsInternalsUI() override;

 private:
  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace ash

#endif  // ASH_WEBUI_SYSTEM_EXTENSIONS_INTERNALS_UI_SYSTEM_EXTENSIONS_INTERNALS_UI_H_
