// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_SHIMLESS_RMA_SHIMLESS_RMA_H_
#define ASH_WEBUI_SHIMLESS_RMA_SHIMLESS_RMA_H_

#include <memory>

#include "ash/webui/shimless_rma/backend/shimless_rma_service.h"
#include "ash/webui/shimless_rma/mojom/shimless_rma.mojom-forward.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom-forward.h"  // nogncheck
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/web_dialogs/web_dialog_ui.h"

namespace content {
class WebUI;
}  // namespace content

namespace ash {

// The WebUI for ShimlessRMA or chrome://shimless-rma.
class ShimlessRMADialogUI : public ui::MojoWebDialogUI {
 public:
  explicit ShimlessRMADialogUI(content::WebUI* web_ui);
  ~ShimlessRMADialogUI() override;

  ShimlessRMADialogUI(const ShimlessRMADialogUI&) = delete;
  ShimlessRMADialogUI& operator=(const ShimlessRMADialogUI&) = delete;

  void BindInterface(
      mojo::PendingReceiver<chromeos::network_config::mojom::CrosNetworkConfig>
          receiver);
  void BindInterface(
      mojo::PendingReceiver<shimless_rma::mojom::ShimlessRmaService> receiver);

 private:
  std::unique_ptr<shimless_rma::ShimlessRmaService> shimless_rma_manager_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace ash

#endif  // ASH_WEBUI_SHIMLESS_RMA_SHIMLESS_RMA_H_
