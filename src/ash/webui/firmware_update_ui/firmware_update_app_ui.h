// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_FIRMWARE_UPDATE_UI_FIRMWARE_UPDATE_APP_UI_H_
#define ASH_WEBUI_FIRMWARE_UPDATE_UI_FIRMWARE_UPDATE_APP_UI_H_

#include "ash/webui/firmware_update_ui/mojom/firmware_update.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/web_dialogs/web_dialog_ui.h"

namespace content {
class WebUI;
}  // namespace content

namespace ash {

class FirmwareUpdateAppUI : public ui::MojoWebDialogUI {
 public:
  explicit FirmwareUpdateAppUI(content::WebUI* web_ui);
  FirmwareUpdateAppUI(const FirmwareUpdateAppUI&) = delete;
  FirmwareUpdateAppUI& operator=(const FirmwareUpdateAppUI&) = delete;
  ~FirmwareUpdateAppUI() override;
  void BindInterface(
      mojo::PendingReceiver<firmware_update::mojom::UpdateProvider> receiver);

 private:
  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace ash

#endif  // ASH_WEBUI_FIRMWARE_UPDATE_UI_FIRMWARE_UPDATE_APP_UI_H_