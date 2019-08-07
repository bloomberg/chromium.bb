// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_ADD_SUPERVISION_ADD_SUPERVISION_UI_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_ADD_SUPERVISION_ADD_SUPERVISION_UI_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/ui/webui/chromeos/add_supervision/add_supervision.mojom.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace chromeos {

// Controller for chrome://add-supervision
class AddSupervisionUI : public ui::MojoWebUIController {
 public:
  explicit AddSupervisionUI(content::WebUI* web_ui);
  ~AddSupervisionUI() override;

 private:
  void BindAddSupervisionHandler(
      add_supervision::mojom::AddSupervisionHandlerRequest request);
  void SetupResources();

  std::unique_ptr<add_supervision::mojom::AddSupervisionHandler>
      mojo_api_handler_;

  DISALLOW_COPY_AND_ASSIGN(AddSupervisionUI);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_ADD_SUPERVISION_ADD_SUPERVISION_UI_H_
