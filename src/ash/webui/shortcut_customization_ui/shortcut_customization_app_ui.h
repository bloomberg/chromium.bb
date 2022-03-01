// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_SHORTCUT_CUSTOMIZATION_UI_SHORTCUT_CUSTOMIZATION_APP_UI_H_
#define ASH_WEBUI_SHORTCUT_CUSTOMIZATION_UI_SHORTCUT_CUSTOMIZATION_APP_UI_H_

#include <memory>

#include "ash/webui/shortcut_customization_ui/backend/accelerator_configuration_provider.h"
#include "ash/webui/shortcut_customization_ui/mojom/shortcut_customization.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace content {
class WebUI;
}  // namespace content

namespace ash {

class ShortcutCustomizationAppUI : public ui::MojoWebUIController {
 public:
  explicit ShortcutCustomizationAppUI(content::WebUI* web_ui);
  ShortcutCustomizationAppUI(const ShortcutCustomizationAppUI&) = delete;
  ShortcutCustomizationAppUI& operator=(const ShortcutCustomizationAppUI&) =
      delete;
  ~ShortcutCustomizationAppUI() override;

 private:
  void BindInterface(
      mojo::PendingReceiver<
          shortcut_customization::mojom::AcceleratorConfigurationProvider>
          receiver);
  std::unique_ptr<shortcut_ui::AcceleratorConfigurationProvider> provider_;
};

}  // namespace ash

#endif  // ASH_WEBUI_SHORTCUT_CUSTOMIZATION_UI_SHORTCUT_CUSTOMIZATION_APP_UI_H_