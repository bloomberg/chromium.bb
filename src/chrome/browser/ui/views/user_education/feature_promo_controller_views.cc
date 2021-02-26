// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/user_education/feature_promo_controller_views.h"

#include <utility>

#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/optional.h"
#include "base/token.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/user_education/feature_promo_snooze_service.h"
#include "chrome/browser/ui/views/chrome_view_class_properties.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/user_education/feature_promo_bubble_params.h"
#include "chrome/browser/ui/views/user_education/feature_promo_bubble_view.h"
#include "chrome/browser/ui/views/user_education/feature_promo_registry.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "ui/views/view.h"

FeaturePromoControllerViews::FeaturePromoControllerViews(
    BrowserView* browser_view)
    : browser_view_(browser_view),
      snooze_service_(std::make_unique<FeaturePromoSnoozeService>(
          browser_view->browser()->profile())),
      tracker_(feature_engagement::TrackerFactory::GetForBrowserContext(
          browser_view->browser()->profile())) {
  DCHECK(tracker_);
}

FeaturePromoControllerViews::~FeaturePromoControllerViews() {
  if (!promo_bubble_) {
    DCHECK_EQ(current_iph_feature_, nullptr);
    return;
  }

  DCHECK(current_iph_feature_);

  promo_bubble_->GetWidget()->Close();
}

// static
FeaturePromoControllerViews* FeaturePromoControllerViews::GetForView(
    views::View* view) {
  views::Widget* widget = view->GetWidget();
  if (!widget)
    return nullptr;

  BrowserView* browser_view =
      BrowserView::GetBrowserViewForNativeWindow(widget->GetNativeWindow());
  if (!browser_view)
    return nullptr;

  return browser_view->feature_promo_controller();
}

bool FeaturePromoControllerViews::MaybeShowPromoWithParams(
    const base::Feature& iph_feature,
    const FeaturePromoBubbleParams& params,
    BubbleCloseCallback close_callback) {
  if (promos_blocked_for_testing_)
    return false;

  // A normal promo cannot show if a critical promo is displayed. These
  // are not registered with |tracker_| so check here.
  if (current_critical_promo_)
    return false;

  // Temporarily turn off IPH in incognito as a concern was raised that
  // the IPH backend ignores incognito and writes to the parent profile.
  // See https://bugs.chromium.org/p/chromium/issues/detail?id=1128728#c30
  if (browser_view_->GetProfile()->IsIncognitoProfile())
    return false;

  if (snooze_service_->IsBlocked(iph_feature))
    return false;

  if (!tracker_->ShouldTriggerHelpUI(iph_feature))
    return false;

  // If the tracker says we should trigger, but we have a promo
  // currently showing, there is a bug somewhere in here.
  DCHECK(!current_iph_feature_);
  current_iph_feature_ = &iph_feature;

  // Record count of previous snoozes when an IPH triggers.
  int snooze_count = snooze_service_->GetSnoozeCount(iph_feature);
  base::UmaHistogramExactLinear("InProductHelp.Promos.SnoozeCountAtTrigger." +
                                    std::string(iph_feature.name),
                                snooze_count,
                                snooze_service_->kUmaMaxSnoozeCount);

  ShowPromoBubbleImpl(params);
  close_callback_ = std::move(close_callback);

  return true;
}

base::Optional<base::Token> FeaturePromoControllerViews::ShowCriticalPromo(
    const FeaturePromoBubbleParams& params) {
  if (promos_blocked_for_testing_)
    return base::nullopt;

  // Don't preempt an existing critical promo.
  if (current_critical_promo_)
    return base::nullopt;

  // If a normal bubble is showing, close it. If the promo is has
  // continued after a CloseBubbleAndContinuePromo() call, we can't stop
  // it. However we will show the critical promo anyway.
  if (current_iph_feature_ && promo_bubble_)
    CloseBubble(*current_iph_feature_);

  // Snooze is not supported for critical promos.
  DCHECK(!params.allow_snooze);

  DCHECK(!promo_bubble_);

  current_critical_promo_ = base::Token::CreateRandom();
  ShowPromoBubbleImpl(params);

  return current_critical_promo_;
}

void FeaturePromoControllerViews::CloseBubbleForCriticalPromo(
    const base::Token& critical_promo_id) {
  if (current_critical_promo_ != critical_promo_id)
    return;

  DCHECK(promo_bubble_);
  promo_bubble_->GetWidget()->Close();
}

bool FeaturePromoControllerViews::MaybeShowPromo(
    const base::Feature& iph_feature,
    BubbleCloseCallback close_callback) {
  base::Optional<FeaturePromoBubbleParams> params =
      FeaturePromoRegistry::GetInstance()->GetParamsForFeature(iph_feature,
                                                               browser_view_);
  if (!params)
    return false;
  return MaybeShowPromoWithParams(iph_feature, *params,
                                  std::move(close_callback));
}

void FeaturePromoControllerViews::OnUserSnooze(
    const base::Feature& iph_feature) {
  snooze_service_->OnUserSnooze(iph_feature);
}

void FeaturePromoControllerViews::OnUserDismiss(
    const base::Feature& iph_feature) {
  snooze_service_->OnUserDismiss(iph_feature);
}

bool FeaturePromoControllerViews::BubbleIsShowing(
    const base::Feature& iph_feature) const {
  return promo_bubble_ && current_iph_feature_ == &iph_feature;
}

bool FeaturePromoControllerViews::CloseBubble(
    const base::Feature& iph_feature) {
  if (!BubbleIsShowing(iph_feature))
    return false;
  promo_bubble_->GetWidget()->Close();
  return true;
}

void FeaturePromoControllerViews::UpdateBubbleForAnchorBoundsChange() {
  if (!promo_bubble_)
    return;
  promo_bubble_->OnAnchorBoundsChanged();
}

FeaturePromoController::PromoHandle
FeaturePromoControllerViews::CloseBubbleAndContinuePromo(
    const base::Feature& iph_feature) {
  DCHECK_EQ(&iph_feature, current_iph_feature_);
  DCHECK(promo_bubble_);

  widget_observer_.Remove(promo_bubble_->GetWidget());
  promo_bubble_->GetWidget()->Close();
  promo_bubble_ = nullptr;

  if (anchor_view_tracker_.view())
    anchor_view_tracker_.view()->SetProperty(kHasInProductHelpPromoKey, false);

  if (close_callback_)
    std::move(close_callback_).Run();

  // Record count of previous snoozes when the IPH gets dismissed by user
  // following the promo. e.g. clicking on relevant controls.
  int snooze_count = snooze_service_->GetSnoozeCount(iph_feature);
  base::UmaHistogramExactLinear("InProductHelp.Promos.SnoozeCountAtFollow." +
                                    std::string(iph_feature.name),
                                snooze_count,
                                snooze_service_->kUmaMaxSnoozeCount);

  return PromoHandle(weak_ptr_factory_.GetWeakPtr());
}

void FeaturePromoControllerViews::BlockPromosForTesting() {
  promos_blocked_for_testing_ = true;

  // If we own a bubble, stop the current promo.
  if (promo_bubble_)
    CloseBubble(*current_iph_feature_);
}

void FeaturePromoControllerViews::OnWidgetClosing(views::Widget* widget) {
  DCHECK(promo_bubble_);
  DCHECK_EQ(widget, promo_bubble_->GetWidget());
  HandleBubbleClosed();
}

void FeaturePromoControllerViews::OnWidgetDestroying(views::Widget* widget) {
  DCHECK(promo_bubble_);
  DCHECK_EQ(widget, promo_bubble_->GetWidget());
  HandleBubbleClosed();
}

void FeaturePromoControllerViews::FinishContinuedPromo() {
  DCHECK(current_iph_feature_);
  DCHECK(!promo_bubble_);
  tracker_->Dismissed(*current_iph_feature_);
  current_iph_feature_ = nullptr;
}

void FeaturePromoControllerViews::ShowPromoBubbleImpl(
    const FeaturePromoBubbleParams& params) {
  params.anchor_view->SetProperty(kHasInProductHelpPromoKey, true);
  anchor_view_tracker_.SetView(params.anchor_view);

  if (current_iph_feature_) {
    // For normal promos:
    promo_bubble_ = FeaturePromoBubbleView::Create(
        params,
        base::BindRepeating(&FeaturePromoControllerViews::OnUserSnooze,
                            base::Unretained(this), *current_iph_feature_),
        base::BindRepeating(&FeaturePromoControllerViews::OnUserDismiss,
                            base::Unretained(this), *current_iph_feature_));
  } else {
    // For critical promos, since they aren't snoozable:
    promo_bubble_ = FeaturePromoBubbleView::Create(params);
  }

  widget_observer_.Add(promo_bubble_->GetWidget());
}

void FeaturePromoControllerViews::HandleBubbleClosed() {
  // A bubble should be showing.
  DCHECK(promo_bubble_);

  // Exactly one of current_iph_feature_ or current_critical_promo_ should have
  // a value.
  DCHECK_NE(current_iph_feature_ != nullptr,
            current_critical_promo_.has_value());

  widget_observer_.Remove(promo_bubble_->GetWidget());
  promo_bubble_ = nullptr;

  if (anchor_view_tracker_.view())
    anchor_view_tracker_.view()->SetProperty(kHasInProductHelpPromoKey, false);

  if (close_callback_)
    std::move(close_callback_).Run();

  if (current_iph_feature_) {
    tracker_->Dismissed(*current_iph_feature_);
    current_iph_feature_ = nullptr;
  } else {
    current_critical_promo_ = base::nullopt;
  }
}
