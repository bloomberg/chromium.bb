// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/feature_promos/reopen_tab_promo_controller.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/in_product_help/reopen_tab_in_product_help.h"
#include "chrome/browser/ui/in_product_help/reopen_tab_in_product_help_factory.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/views/feature_promos/feature_promo_bubble_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/app_menu.h"
#include "chrome/browser/ui/views/toolbar/browser_app_menu_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/grit/generated_resources.h"
#include "ui/views/controls/menu/menu_item_view.h"

ReopenTabPromoController::ReopenTabPromoController(BrowserView* browser_view)
    : iph_service_(ReopenTabInProductHelpFactory::GetForProfile(
          browser_view->browser()->profile())),
      browser_view_(browser_view) {
  // Check that the app menu button exists. It should only not exist when there
  // is no tab strip, in which case this shouldn't trigger in the first place.
  BrowserAppMenuButton* app_menu_button =
      browser_view_->toolbar()->app_menu_button();
  DCHECK(app_menu_button);
}

void ReopenTabPromoController::ShowPromo() {
  // Here, we start the promo display. We highlight the app menu button and open
  // the promo bubble.
  BrowserAppMenuButton* app_menu_button =
      browser_view_->toolbar()->app_menu_button();
  app_menu_button->AddMenuListener(this);
  app_menu_button->SetHighlighted(true);

  promo_bubble_ = FeaturePromoBubbleView::CreateOwned(
      app_menu_button, views::BubbleBorder::Arrow::TOP_RIGHT,
      IDS_REOPEN_TAB_PROMO, FeaturePromoBubbleView::ActivationAction::ACTIVATE);
  promo_bubble_->set_close_on_deactivate(false);
  promo_bubble_->GetWidget()->AddObserver(this);
}

void ReopenTabPromoController::OnMenuOpened() {
  // The user followed the promo and opened the menu. Now, we highlight the
  // history item and observe for the history submenu opening.
  BrowserAppMenuButton* app_menu_button =
      browser_view_->toolbar()->app_menu_button();
  app_menu_button->RemoveMenuListener(this);

  AppMenu* app_menu = app_menu_button->app_menu();
  app_menu->AddObserver(this);

  views::MenuItemView* recent_tabs_menu_item =
      app_menu->root_menu_item()->GetMenuItemByID(IDC_RECENT_TABS_MENU);
  recent_tabs_menu_item->SetForcedVisualSelection(true);
}

void ReopenTabPromoController::OnWidgetDestroying(views::Widget* widget) {
  DCHECK(promo_bubble_);

  // If the menu isn't showing, that means the promo bubble timed out. We should
  // notify our IPH service that help was dismissed.
  if (!browser_view_->toolbar()->app_menu_button()->IsMenuShowing()) {
    BrowserAppMenuButton* app_menu_button =
        browser_view_->toolbar()->app_menu_button();
    app_menu_button->RemoveMenuListener(this);
    app_menu_button->SetHighlighted(false);

    iph_service_->HelpDismissed();
  }
}

void ReopenTabPromoController::AppMenuClosed() {
  // The menu was opened then closed, whether by clicking away or by clicking a
  // menu item. We notify the service regardless of whether IPH succeeded.
  // Success is determined by whether the reopen tab event was sent.
  iph_service_->HelpDismissed();

  AppMenu* app_menu = browser_view_->toolbar()->app_menu_button()->app_menu();
  app_menu->RemoveObserver(this);
}

void ReopenTabPromoController::OnShowSubmenu() {
  // Check if the last opened tab menu item exists (it will if the history
  // submenu was opened). If so, highlight it.
  views::MenuItemView* root_menu_item =
      browser_view_->toolbar()->app_menu_button()->app_menu()->root_menu_item();
  views::MenuItemView* last_tab_menu_item =
      root_menu_item->GetMenuItemByID(AppMenuModel::kMinRecentTabsCommandId);
  if (last_tab_menu_item) {
    // The history submenu was shown. Highlight the last-closed tab item.
    last_tab_menu_item->SetForcedVisualSelection(true);
  }
}
