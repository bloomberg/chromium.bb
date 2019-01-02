// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/autofill/save_card_ui.h"
#include "chrome/browser/ui/cocoa/browser_dialogs_views_mac.h"
#include "chrome/browser/ui/cocoa/browser_window_cocoa.h"
#include "chrome/browser/ui/cocoa/browser_window_controller.h"
#include "chrome/browser/ui/cocoa/bubble_anchor_helper_views.h"
#include "chrome/browser/ui/cocoa/location_bar/location_bar_view_mac.h"
#include "chrome/browser/ui/cocoa/location_bar/save_credit_card_decoration.h"
#include "chrome/browser/ui/views/autofill/save_card_bubble_views.h"
#include "chrome/browser/ui/views/autofill/save_card_manage_cards_bubble_views.h"
#include "chrome/browser/ui/views/autofill/save_card_offer_bubble_views.h"
#include "chrome/browser/ui/views/autofill/save_card_sign_in_promo_bubble_views.h"
#include "ui/base/cocoa/cocoa_base_utils.h"
#include "ui/gfx/mac/coordinate_conversion.h"

namespace autofill {

SaveCardBubbleView* CreateSaveCardBubbleView(
    content::WebContents* web_contents,
    autofill::SaveCardBubbleController* controller,
    BrowserWindowController* browser_window_controller,
    bool user_gesture) {
  NSWindow* parent_window = [browser_window_controller window];
  LocationBarViewMac* location_bar =
      [browser_window_controller locationBarBridge];
  gfx::Point anchor =
      gfx::ScreenPointFromNSPoint(ui::ConvertPointFromWindowToScreen(
          parent_window, location_bar->GetSaveCreditCardBubblePoint()));

  autofill::BubbleType bubble_type = controller->GetBubbleType();
  autofill::SaveCardBubbleViews* bubble = nullptr;

  switch (bubble_type) {
    case autofill::BubbleType::LOCAL_SAVE:
    case autofill::BubbleType::UPLOAD_SAVE:
      bubble = new autofill::SaveCardOfferBubbleViews(nullptr, anchor,
                                                      web_contents, controller);
      break;
    case autofill::BubbleType::SIGN_IN_PROMO:
      bubble = new autofill::SaveCardSignInPromoBubbleViews(
          nullptr, anchor, web_contents, controller);
      break;
    case autofill::BubbleType::MANAGE_CARDS:
      bubble = new autofill::SaveCardManageCardsBubbleViews(
          nullptr, anchor, web_contents, controller);
      break;
    case autofill::BubbleType::INACTIVE:
      break;
  }

  DCHECK(bubble);

  // Usually the anchor view determines the arrow type, but there is none in
  // MacViews. So always use TOP_RIGHT, even in fullscreen.
  bubble->set_arrow(views::BubbleBorder::TOP_RIGHT);
  bubble->set_parent_window(platform_util::GetViewForWindow(parent_window));
  views::BubbleDialogDelegateView::CreateBubble(bubble);
  bubble->Show(user_gesture ? autofill::SaveCardBubbleViews::USER_GESTURE
                            : autofill::SaveCardBubbleViews::AUTOMATIC);
  KeepBubbleAnchored(bubble, location_bar->save_credit_card_decoration());
  return bubble;
}

}  // namespace autofill
