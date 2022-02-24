// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/user_education/browser_feature_promo_controller.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/user_education/help_bubble_factory_views.h"
#include "chrome/browser/ui/views/user_education/help_bubble_view.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/view.h"

BrowserFeaturePromoController::BrowserFeaturePromoController(
    BrowserView* browser_view,
    feature_engagement::Tracker* feature_engagement_tracker,
    FeaturePromoRegistry* registry,
    HelpBubbleFactoryRegistry* help_bubble_registry,
    FeaturePromoSnoozeService* snooze_service,
    TutorialService* tutorial_service)
    : FeaturePromoControllerCommon(feature_engagement_tracker,
                                   registry,
                                   help_bubble_registry,
                                   snooze_service,
                                   tutorial_service),
      browser_view_(browser_view) {}

BrowserFeaturePromoController::~BrowserFeaturePromoController() = default;

// static
BrowserFeaturePromoController* BrowserFeaturePromoController::GetForView(
    views::View* view) {
  if (!view)
    return nullptr;
  views::Widget* widget = view->GetWidget();
  if (!widget)
    return nullptr;

  BrowserView* browser_view = BrowserView::GetBrowserViewForNativeWindow(
      widget->GetPrimaryWindowWidget()->GetNativeWindow());
  if (!browser_view)
    return nullptr;

  return browser_view->GetFeaturePromoController();
}

ui::ElementContext BrowserFeaturePromoController::GetAnchorContext() const {
  return views::ElementTrackerViews::GetContextForView(browser_view_);
}

bool BrowserFeaturePromoController::CanShowPromo(
    ui::TrackedElement* anchor_element) const {
  // Temporarily turn off IPH in incognito as a concern was raised that
  // the IPH backend ignores incognito and writes to the parent profile.
  // See https://bugs.chromium.org/p/chromium/issues/detail?id=1128728#c30
  if (browser_view_->GetProfile()->IsIncognitoProfile())
    return false;

  // Don't show IPH if the anchor view is in an inactive window.
  auto* const anchor_view = anchor_element->AsA<views::TrackedElementViews>();
  auto* const anchor_widget = anchor_view ? anchor_view->view()->GetWidget()
                                          : browser_view_->GetWidget();
  if (!anchor_widget)
    return false;

  if (!active_window_check_blocked() && !anchor_widget->ShouldPaintAsActive())
    return false;

  return true;
}

const ui::AcceleratorProvider*
BrowserFeaturePromoController::GetAcceleratorProvider() const {
  return browser_view_;
}

std::u16string BrowserFeaturePromoController::GetSnoozeButtonText() const {
  return l10n_util::GetStringUTF16(IDS_PROMO_SNOOZE_BUTTON);
}

std::u16string BrowserFeaturePromoController::GetDismissButtonText() const {
  return l10n_util::GetStringUTF16(IDS_PROMO_DISMISS_BUTTON);
}

bool BrowserFeaturePromoController::IsOkButtonLeading() const {
  return views::PlatformStyle::kIsOkButtonLeading;
}

std::u16string
BrowserFeaturePromoController::GetFocusHelpBubbleScreenReaderHint(
    FeaturePromoSpecification::PromoType promo_type,
    const ui::TrackedElement* anchor_element,
    bool is_critical_promo) const {
  // No message is required as this is a background bubble with a
  // screen reader-specific prompt and will dismiss itself.
  if (promo_type == FeaturePromoSpecification::PromoType::kToast)
    return std::u16string();

  ui::Accelerator accelerator;
  std::u16string accelerator_text;
  if (browser_view_->GetAccelerator(IDC_FOCUS_NEXT_PANE, &accelerator)) {
    accelerator_text = accelerator.GetShortcutText();
  } else {
    NOTREACHED();
  }

  // Present the user with the full help bubble navigation shortcut.
  auto* const anchor_view = anchor_element->AsA<views::TrackedElementViews>();
  if (anchor_view && anchor_view->view()->IsAccessibilityFocusable()) {
    return l10n_util::GetStringFUTF16(IDS_FOCUS_HELP_BUBBLE_TOGGLE_DESCRIPTION,
                                      accelerator_text);
  }

  // If the bubble starts focused and focus cannot traverse to the anchor view,
  // do not use a promo.
  if (is_critical_promo)
    return std::u16string();

  // Present the user with an abridged help bubble navigation shortcut.
  return l10n_util::GetStringFUTF16(IDS_FOCUS_HELP_BUBBLE_DESCRIPTION,
                                    accelerator_text);
}
