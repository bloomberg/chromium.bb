// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_toolbar_button.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_view.h"
#include "chrome/grit/generated_resources.h"
#include "extensions/browser/extension_registry.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/button/button_controller.h"

ExtensionsToolbarButton::ExtensionsToolbarButton(
    Browser* browser,
    ExtensionsContainer* extensions_container)
    : ToolbarButton(this),
      browser_(browser),
      extensions_container_(extensions_container) {
  SetTooltipText(l10n_util::GetStringUTF16(IDS_TOOLTIP_EXTENSIONS_BUTTON));
  button_controller()->set_notify_action(
      views::ButtonController::NotifyAction::NOTIFY_ON_PRESS);
}

void ExtensionsToolbarButton::UpdateIcon() {
  const int icon_size = ui::MaterialDesignController::touch_ui()
                            ? kDefaultTouchableIconSize
                            : kDefaultIconSize;
  const SkColor normal_color =
      GetThemeProvider()->GetColor(ThemeProperties::COLOR_TOOLBAR_BUTTON_ICON);
  SetImage(views::Button::STATE_NORMAL,
           gfx::CreateVectorIcon(kExtensionIcon, icon_size, normal_color));
}

void ExtensionsToolbarButton::ButtonPressed(views::Button* sender,
                                            const ui::Event& event) {
  if (ExtensionsMenuView::IsShowing()) {
    ExtensionsMenuView::Hide();
    return;
  }
  ExtensionsMenuView::ShowBubble(this, browser_, extensions_container_);
}
