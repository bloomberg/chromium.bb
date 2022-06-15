// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_request_access_button_hover_card.h"

#include "base/bind.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "chrome/browser/ui/views/extensions/extensions_dialogs_utils.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/view.h"

namespace {

views::BubbleDialogModelHost* request_access_bubble = nullptr;

}  // namespace

// static
void ExtensionsRequestAccessButtonHoverCard::ShowBubble(
    content::WebContents* web_contents,
    views::View* anchor_view,
    std::vector<ToolbarActionViewController*> actions) {
  DCHECK(!request_access_bubble);
  DCHECK(web_contents);
  DCHECK(!actions.empty());

  ui::DialogModel::Builder dialog_builder =
      ui::DialogModel::Builder(std::make_unique<ui::DialogModelDelegate>());
  dialog_builder
      .OverrideShowCloseButton(false)
      // Make sure the widget is closed if the dialog gets destroyed while the
      // mouse is still on hover.
      .SetDialogDestroyingCallback(
          base::BindOnce(&ExtensionsRequestAccessButtonHoverCard::HideBubble));

  const std::u16string url = GetCurrentHost(web_contents);
  if (actions.size() == 1) {
    dialog_builder.SetIcon(GetIcon(actions[0], web_contents))
        .AddBodyText(ui::DialogModelLabel(l10n_util::GetStringFUTF16(
            IDS_EXTENSIONS_REQUEST_ACCESS_BUTTON_TOOLTIP_SINGLE_EXTENSION,
            actions[0]->GetActionName(), url)));
  } else {
    dialog_builder.AddBodyText(ui::DialogModelLabel(l10n_util::GetStringFUTF16(
        IDS_EXTENSIONS_REQUEST_ACCESS_BUTTON_TOOLTIP_MULTIPLE_EXTENSIONS,
        url)));
    for (auto* action : actions) {
      dialog_builder.AddMenuItem(
          GetIcon(action, web_contents), action->GetActionName(),
          base::DoNothing(),
          ui::DialogModelMenuItem::Params().set_is_enabled(false));
    }
  }

  auto bubble = std::make_unique<views::BubbleDialogModelHost>(
      dialog_builder.Build(), anchor_view, views::BubbleBorder::TOP_RIGHT);
  // Hover card should not become active window when hovering over request
  // button in an inactive window. Setting this to false creates the need to
  // explicitly hide the hovercard.
  bubble->SetCanActivate(false);
  request_access_bubble = bubble.get();

  auto* widget = views::BubbleDialogDelegate::CreateBubble(std::move(bubble));
  // Ensure the hover card Widget assumes the highest z-order to avoid occlusion
  // by other secondary UI Widgets
  widget->StackAtTop();
  widget->Show();
}

// static
void ExtensionsRequestAccessButtonHoverCard::HideBubble() {
  if (ExtensionsRequestAccessButtonHoverCard::IsShowing()) {
    request_access_bubble->GetWidget()->Close();
    request_access_bubble = nullptr;
  }
}

// static
bool ExtensionsRequestAccessButtonHoverCard::IsShowing() {
  return request_access_bubble != nullptr;
}
