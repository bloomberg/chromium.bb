// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/webui_tab_strip_container_view.h"

#include "base/logging.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/extensions/chrome_extension_web_contents_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/task_manager/web_contents_tags.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/background.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/view_class_properties.h"

WebUITabStripContainerView::WebUITabStripContainerView(Browser* browser)
    : browser_(browser),
      web_view_(
          AddChildView(std::make_unique<views::WebView>(browser->profile()))) {
  SetVisible(false);
  SetLayoutManager(std::make_unique<views::FillLayout>());
  web_view_->LoadInitialURL(GURL(chrome::kChromeUITabStripURL));
  extensions::ChromeExtensionWebContentsObserver::CreateForWebContents(
      web_view_->web_contents());
  task_manager::WebContentsTags::CreateForTabContents(
      web_view_->web_contents());
}

std::unique_ptr<views::View>
WebUITabStripContainerView::CreateControlButtons() {
  auto toolbar_button_container = std::make_unique<views::View>();
  toolbar_button_container
      ->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
      .SetDefault(views::kMarginsKey,
                  gfx::Insets(0, GetLayoutConstant(TOOLBAR_ELEMENT_PADDING)))
      .SetInteriorMargin(
          gfx::Insets(0, GetLayoutConstant(TOOLBAR_STANDARD_SPACING)))
      .SetCollapseMargins(true);
  toolbar_button_container->SetBackground(
      views::CreateSolidBackground(gfx::kGoogleGrey300));

  ToolbarButton* const new_tab_button = toolbar_button_container->AddChildView(
      std::make_unique<ToolbarButton>(this));
  new_tab_button->SetID(VIEW_ID_WEBUI_TAB_STRIP_NEW_TAB_BUTTON);
  new_tab_button->SetImage(
      views::Button::STATE_NORMAL,
      gfx::CreateVectorIcon(kAddIcon, gfx::kGoogleGrey700));
  new_tab_button->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_TOOLTIP_NEW_TAB));

  // TODO(pbos): Replace this button with tab counter. Remember to add a
  // tooltip.
  ToolbarButton* const toggle_button = toolbar_button_container->AddChildView(
      std::make_unique<ToolbarButton>(this));
  toggle_button->SetID(VIEW_ID_WEBUI_TAB_STRIP_TOGGLE_BUTTON);
  toggle_button->SetImage(
      views::Button::STATE_NORMAL,
      gfx::CreateVectorIcon(kCaretUpIcon, gfx::kGoogleGrey700));

  return toolbar_button_container;
}

void WebUITabStripContainerView::ButtonPressed(views::Button* sender,
                                               const ui::Event& event) {
  if (sender->GetID() == VIEW_ID_WEBUI_TAB_STRIP_TOGGLE_BUTTON) {
    // TODO(pbos): Trigger a slide animation here.
    SetVisible(!GetVisible());
    InvalidateLayout();
  } else if (sender->GetID() == VIEW_ID_WEBUI_TAB_STRIP_NEW_TAB_BUTTON) {
    chrome::ExecuteCommand(browser_, IDC_NEW_TAB);
  } else {
    NOTREACHED();
  }
}
