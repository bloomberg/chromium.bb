// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/main_stage/ui_element_container_view.h"

#include <string>

#include "ash/assistant/assistant_controller.h"
#include "ash/assistant/assistant_interaction_controller.h"
#include "ash/assistant/model/assistant_response.h"
#include "ash/assistant/model/assistant_ui_element.h"
#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/assistant/ui/main_stage/assistant_card_element_view.h"
#include "ash/assistant/ui/main_stage/assistant_text_element_view.h"
#include "ash/assistant/util/animation_util.h"
#include "base/callback.h"
#include "base/time/time.h"
#include "ui/aura/window.h"
#include "ui/compositor/callback_layer_animation_observer.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/compositor/layer_animator.h"
#include "ui/views/border.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

// Appearance.
constexpr int kFirstCardMarginTopDip = 40;
constexpr int kPaddingBottomDip = 24;

// Card element animation.
constexpr float kCardElementAnimationFadeOutOpacity = 0.26f;

// Text element animation.
constexpr float kTextElementAnimationFadeOutOpacity = 0.f;

// UI element animation.
constexpr base::TimeDelta kUiElementAnimationFadeInDelay =
    base::TimeDelta::FromMilliseconds(83);
constexpr base::TimeDelta kUiElementAnimationFadeInDuration =
    base::TimeDelta::FromMilliseconds(250);
constexpr base::TimeDelta kUiElementAnimationFadeOutDuration =
    base::TimeDelta::FromMilliseconds(167);

}  // namespace

// UiElementContainerView ------------------------------------------------------

UiElementContainerView::UiElementContainerView(
    AssistantController* assistant_controller)
    : assistant_controller_(assistant_controller),
      ui_elements_exit_animation_observer_(
          std::make_unique<ui::CallbackLayerAnimationObserver>(
              /*animation_ended_callback=*/base::BindRepeating(
                  &UiElementContainerView::OnAllUiElementsExitAnimationEnded,
                  base::Unretained(this)))) {
  InitLayout();

  // The Assistant controller indirectly owns the view hierarchy to which
  // UiElementContainerView belongs so is guaranteed to outlive it.
  assistant_controller_->interaction_controller()->AddModelObserver(this);
}

UiElementContainerView::~UiElementContainerView() {
  assistant_controller_->interaction_controller()->RemoveModelObserver(this);
}

const char* UiElementContainerView::GetClassName() const {
  return "UiElementContainerView";
}

gfx::Size UiElementContainerView::CalculatePreferredSize() const {
  return gfx::Size(INT_MAX, GetHeightForWidth(INT_MAX));
}

int UiElementContainerView::GetHeightForWidth(int width) const {
  return content_view()->GetHeightForWidth(width);
}

gfx::Size UiElementContainerView::GetMinimumSize() const {
  // AssistantMainStage uses BoxLayout's flex property to grow/shrink
  // UiElementContainerView to fill available space as needed. When height is
  // shrunk to zero, as is temporarily the case during the initial container
  // growth animation for the first Assistant response, UiElementContainerView
  // will be laid out with zero width. We do not recover from this state until
  // the next layout pass, which causes Assistant cards for the first response
  // to be laid out with zero width. We work around this by imposing a minimum
  // height restriction of 1 dip that is factored into BoxLayout's flex
  // calculations to make sure that our width is never being set to zero.
  return gfx::Size(INT_MAX, 1);
}

void UiElementContainerView::OnContentsPreferredSizeChanged(
    views::View* content_view) {
  const int preferred_height = content_view->GetHeightForWidth(width());
  content_view->SetSize(gfx::Size(width(), preferred_height));
}

void UiElementContainerView::PreferredSizeChanged() {
  // Because views are added/removed in batches, we attempt to prevent over-
  // propagation of the PreferredSizeChanged event during batched view hierarchy
  // add/remove operations. This helps to reduce layout passes.
  if (propagate_preferred_size_changed_)
    AssistantScrollView::PreferredSizeChanged();
}

void UiElementContainerView::InitLayout() {
  content_view()->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      gfx::Insets(0, kUiElementHorizontalMarginDip, kPaddingBottomDip,
                  kUiElementHorizontalMarginDip),
      kSpacingDip));
}

void UiElementContainerView::OnCommittedQueryChanged(
    const AssistantQuery& query) {
  using assistant::util::CreateLayerAnimationSequence;
  using assistant::util::CreateOpacityElement;

  // We don't allow processing of events while waiting for the next query
  // response. The contents will be faded out, so it should not be interactive.
  // We also scroll to the top to play nice with the transition animation.
  set_can_process_events_within_subtree(false);
  ScrollToPosition(vertical_scroll_bar(), 0);

  // When a query is committed, we fade out the views for the previous response
  // until the next Assistant response has been received.
  for (const std::pair<ui::LayerOwner*, float>& pair : ui_element_views_) {
    pair.first->layer()->GetAnimator()->StartAnimation(
        CreateLayerAnimationSequence(CreateOpacityElement(
            /*opacity=*/pair.second, kUiElementAnimationFadeOutDuration)));
  }
}

void UiElementContainerView::OnResponseChanged(
    const std::shared_ptr<AssistantResponse>& response) {
  // We may have to pend the response while we animate the previous response off
  // stage. We use a shared pointer to ensure that any views we add to the view
  // hierarchy can be removed before the underlying UI elements are destroyed.
  pending_response_ = std::shared_ptr<const AssistantResponse>(response);

  // If we don't have any pre-existing content, there is nothing to animate off
  // stage so we we can proceed to add the new response.
  if (!content_view()->has_children()) {
    OnResponseAdded(std::move(pending_response_));
    return;
  }

  using assistant::util::CreateLayerAnimationSequence;
  using assistant::util::CreateOpacityElement;
  using assistant::util::StartLayerAnimationSequence;

  // There is a previous response on stage, so we'll animate it off before
  // adding the new response. The new response will be added upon invocation of
  // the exit animation ended callback.
  for (const std::pair<ui::LayerOwner*, float>& pair : ui_element_views_) {
    StartLayerAnimationSequence(
        pair.first->layer()->GetAnimator(),
        // Fade out the opacity to 0%. Note that we approximate 0% by actually
        // using 0.01%. We do this to workaround a DCHECK that requires
        // aura::Windows to have a target opacity > 0% when shown. Because our
        // window will be removed after it reaches this value, it should be safe
        // to circumnavigate this DCHECK.
        CreateLayerAnimationSequence(
            CreateOpacityElement(0.0001f, kUiElementAnimationFadeOutDuration)),
        // Observe the animation.
        ui_elements_exit_animation_observer_.get());
  }

  // Set the observer to active so that we receive callback events.
  ui_elements_exit_animation_observer_->SetActive();
}

void UiElementContainerView::OnResponseCleared() {
  // We can prevent over-propagation of the PreferredSizeChanged event by
  // stopping propagation during batched view hierarchy add/remove operations.
  SetPropagatePreferredSizeChanged(false);
  content_view()->RemoveAllChildViews(/*delete_children=*/true);
  ui_element_views_.clear();
  SetPropagatePreferredSizeChanged(true);

  // Once the response has been cleared from the stage, we can are free to
  // release our shared pointer. This allows resources associated with the
  // underlying UI elements to be freed, provided there are no other usages.
  response_.reset();

  // Reset state for the next response.
  is_first_card_ = true;
}

void UiElementContainerView::OnResponseAdded(
    std::shared_ptr<const AssistantResponse> response) {
  // The response should be fully processed before it is presented.
  DCHECK_EQ(AssistantResponse::ProcessingState::kProcessed,
            response->processing_state());

  // We cache a reference to the |response| to ensure that the instance is not
  // destroyed before we have removed associated views from the view hierarchy.
  response_ = std::move(response);

  // Because the views for the response are animated in together, we can stop
  // propagation of PreferredSizeChanged events until all views have been added
  // to the view hierarchy to reduce layout passes.
  SetPropagatePreferredSizeChanged(false);

  for (const auto& ui_element : response_->GetUiElements()) {
    switch (ui_element->GetType()) {
      case AssistantUiElementType::kCard:
        OnCardElementAdded(
            static_cast<const AssistantCardElement*>(ui_element.get()));
        break;
      case AssistantUiElementType::kText:
        OnTextElementAdded(
            static_cast<const AssistantTextElement*>(ui_element.get()));
        break;
    }
  }

  OnAllUiElementsAdded();
}

void UiElementContainerView::OnCardElementAdded(
    const AssistantCardElement* card_element) {
  // The card, for some reason, is not embeddable so we'll have to ignore it.
  if (!card_element->contents())
    return;

  auto* card_element_view =
      new AssistantCardElementView(assistant_controller_, card_element);

  if (is_first_card_) {
    is_first_card_ = false;

    // The first card requires a top margin of |kFirstCardMarginTopDip|, but
    // we need to account for child spacing because the first card is not
    // necessarily the first UI element.
    const int top_margin_dip = child_count() == 0
                                   ? kFirstCardMarginTopDip
                                   : kFirstCardMarginTopDip - kSpacingDip;

    // We effectively create a top margin by applying an empty border.
    card_element_view->SetBorder(
        views::CreateEmptyBorder(top_margin_dip, 0, 0, 0));
  }

  content_view()->AddChildView(card_element_view);

  // The view will be animated on its own layer, so we need to do some initial
  // layer setup. We're going to fade the view in, so hide it. Note that we
  // approximate 0% opacity by actually using 0.01%. We do this to workaround
  // a DCHECK that requires aura::Windows to have a target opacity > 0% when
  // shown. Because our window will be animated to full opacity from this
  // value, it should be safe to circumnavigate this DCHECK.
  card_element_view->native_view()->layer()->SetFillsBoundsOpaquely(false);
  card_element_view->native_view()->layer()->SetOpacity(0.0001f);

  // We cache the native view for use during animations and its desired
  // opacity that we'll animate to while processing the next query response.
  ui_element_views_.push_back(std::pair<ui::LayerOwner*, float>(
      card_element_view->native_view(), kCardElementAnimationFadeOutOpacity));
}

void UiElementContainerView::OnTextElementAdded(
    const AssistantTextElement* text_element) {
  views::View* text_element_view = new AssistantTextElementView(text_element);

  // The view will be animated on its own layer, so we need to do some initial
  // layer setup. We're going to fade the view in, so hide it.
  text_element_view->SetPaintToLayer();
  text_element_view->layer()->SetFillsBoundsOpaquely(false);
  text_element_view->layer()->SetOpacity(0.f);

  // We cache the view for use during animations and its desired opacity that
  // we'll animate to while processing the next query response.
  ui_element_views_.push_back(std::pair<ui::LayerOwner*, float>(
      text_element_view, kTextElementAnimationFadeOutOpacity));

  content_view()->AddChildView(text_element_view);
}

void UiElementContainerView::OnAllUiElementsAdded() {
  using assistant::util::CreateLayerAnimationSequence;
  using assistant::util::CreateOpacityElement;

  // Now that the response for the current query has been added to the view
  // hierarchy, we can re-enable processing of events. We can also restart
  // propagation of PreferredSizeChanged events since all views have been added
  // to the view hierarchy.
  set_can_process_events_within_subtree(true);
  SetPropagatePreferredSizeChanged(true);

  // Now that we've received and added all UI elements for the current query
  // response, we can animate them in.
  for (const std::pair<ui::LayerOwner*, float>& pair : ui_element_views_) {
    // We fade in the views to full opacity after a slight delay.
    pair.first->layer()->GetAnimator()->StartAnimation(
        CreateLayerAnimationSequence(
            ui::LayerAnimationElement::CreatePauseElement(
                ui::LayerAnimationElement::AnimatableProperty::OPACITY,
                kUiElementAnimationFadeInDelay),
            CreateOpacityElement(1.f, kUiElementAnimationFadeInDuration)));
  }

  // Let screen reader read the query result. This includes the text response
  // and the card fallback text, but webview result is not included.
  // We don't read when there is TTS to avoid speaking over the server response.
  const AssistantResponse* response =
      assistant_controller_->interaction_controller()->model()->response();
  if (!response->has_tts())
    NotifyAccessibilityEvent(ax::mojom::Event::kAlert, true);
}

bool UiElementContainerView::OnAllUiElementsExitAnimationEnded(
    const ui::CallbackLayerAnimationObserver& observer) {
  // All UI elements have finished their exit animations so its safe to perform
  // clearing of their views and managed resources.
  OnResponseCleared();

  // It is safe to add our pending response to the view hierarchy now that we've
  // cleared the previous response from the stage.
  OnResponseAdded(std::move(pending_response_));

  // Return false to prevent the observer from destroying itself.
  return false;
}

void UiElementContainerView::SetPropagatePreferredSizeChanged(bool propagate) {
  if (propagate == propagate_preferred_size_changed_)
    return;

  propagate_preferred_size_changed_ = propagate;

  // When we are no longer stopping propagation of PreferredSizeChanged events,
  // we fire an event off to ensure the view hierarchy is properly laid out.
  if (propagate_preferred_size_changed_)
    PreferredSizeChanged();
}

}  // namespace ash
