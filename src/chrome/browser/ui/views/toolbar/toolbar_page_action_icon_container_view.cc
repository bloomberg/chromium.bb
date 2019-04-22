// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/toolbar_page_action_icon_container_view.h"

#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/autofill/payments/local_card_migration_icon_view.h"
#include "chrome/browser/ui/views/autofill/payments/save_card_icon_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/profiles/avatar_toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_ink_drop_util.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/widget/widget.h"

ToolbarPageActionIconContainerView::ToolbarPageActionIconContainerView(
    CommandUpdater* command_updater,
    Browser* browser)
    : ToolbarIconContainerView(), browser_(browser) {
  local_card_migration_icon_view_ = new autofill::LocalCardMigrationIconView(
      command_updater, browser, this,
      // TODO(crbug.com/932818): The font list and the icon color may not be
      // what we want here. Put placeholders for now.
      views::style::GetFont(CONTEXT_TOOLBAR_BUTTON,
                            views::style::STYLE_PRIMARY));
  page_action_icons_.push_back(local_card_migration_icon_view_);

  save_card_icon_view_ = new autofill::SaveCardIconView(
      command_updater, browser, this,
      // TODO(crbug.com/932818): The font list and the icon color may not be
      // what we want here. Put placeholders for now.
      views::style::GetFont(CONTEXT_TOOLBAR_BUTTON,
                            views::style::STYLE_PRIMARY));
  page_action_icons_.push_back(save_card_icon_view_);

  for (PageActionIconView* icon_view : page_action_icons_) {
    icon_view->SetVisible(false);
    AddChildView(icon_view);
  }

  avatar_ = new AvatarToolbarButton(browser);
  AddChildView(avatar_);
}

ToolbarPageActionIconContainerView::~ToolbarPageActionIconContainerView() =
    default;

void ToolbarPageActionIconContainerView::UpdateAllIcons() {
  const ui::ThemeProvider* theme_provider = GetThemeProvider();
  const SkColor icon_color =
      theme_provider->GetColor(ThemeProperties::COLOR_TOOLBAR_BUTTON_ICON);

  for (PageActionIconView* icon_view : page_action_icons_) {
    icon_view->SetIconColor(icon_color);
    icon_view->Update();
  }

  if (avatar_)
    avatar_->UpdateIcon();
}

PageActionIconView* ToolbarPageActionIconContainerView::GetIconView(
    PageActionIconType icon_type) {
  switch (icon_type) {
    case PageActionIconType::kLocalCardMigration:
      return local_card_migration_icon_view_;
    case PageActionIconType::kSaveCard:
      return save_card_icon_view_;
    default:
      NOTREACHED();
      return nullptr;
  }
}

bool ToolbarPageActionIconContainerView::
    ActivateFirstInactiveBubbleForAccessibility() {
  for (PageActionIconView* icon_view : page_action_icons_) {
    if (FocusInactiveBubbleForIcon(icon_view))
      return true;
  }
  return false;
}

void ToolbarPageActionIconContainerView::UpdatePageActionIcon(
    PageActionIconType icon_type) {
  PageActionIconView* icon = GetIconView(icon_type);
  if (icon)
    icon->Update();
}

SkColor ToolbarPageActionIconContainerView::GetPageActionInkDropColor() const {
  return GetToolbarInkDropBaseColor(this);
}

content::WebContents*
ToolbarPageActionIconContainerView::GetWebContentsForPageActionIconView() {
  return browser_->tab_strip_model()->GetActiveWebContents();
}

void ToolbarPageActionIconContainerView::OnNativeThemeChanged(
    const ui::NativeTheme* theme) {
  // Update icon color.
  UpdateAllIcons();
}

bool ToolbarPageActionIconContainerView::FocusInactiveBubbleForIcon(
    PageActionIconView* icon_view) {
  if (!icon_view->visible() || !icon_view->GetBubble())
    return false;

  views::Widget* widget = icon_view->GetBubble()->GetWidget();
  if (widget && widget->IsVisible() && !widget->IsActive()) {
    widget->Show();
    return true;
  }
  return false;
}
