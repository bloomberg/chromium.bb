// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/media_controls/elements/MediaControlLoadingPanelElement.h"

#include "core/css/CSSStyleDeclaration.h"
#include "core/dom/ShadowRoot.h"
#include "core/dom/events/Event.h"
#include "core/dom/events/EventListener.h"
#include "core/html/HTMLDivElement.h"
#include "core/html/HTMLStyleElement.h"
#include "core/html/media/HTMLMediaElement.h"
#include "core/html/media/HTMLVideoElement.h"
#include "core/style/ComputedStyle.h"
#include "modules/media_controls/MediaControlsImpl.h"
#include "modules/media_controls/MediaControlsResourceLoader.h"
#include "modules/media_controls/elements/MediaControlElementsHelper.h"

namespace {

static const char kAnimationIterationCountName[] = "animation-iteration-count";
static const char kClassAttributeName[] = "class";
static const char kInfinite[] = "infinite";
static const char kNoFrameAvailableSpinnerClass[] = "dark";

}  // namespace

namespace blink {

MediaControlLoadingPanelElement::MediaControlLoadingPanelElement(
    MediaControlsImpl& media_controls)
    : MediaControlDivElement(media_controls, kMediaControlsPanel) {
  SetShadowPseudoId(AtomicString("-internal-media-controls-loading-panel"));
  CreateShadowRootInternal();

  // The loading panel should always start hidden.
  SetIsWanted(false);
}

// The shadow DOM structure looks like:
//
// #root
// +- #spinner-frame
//   +- #spinner
//     +- #layer
//     | +- #spinner-mask-1
//     | | +- #spinner-mask-1-background
//     \ +- #spinner-mask-2
//         +- #spinner-mask-2-background
// +- #cutoff-1
// +- #cutoff-2
// +- #cutoff-3
// +- #cutoff-4
// +- #fake-timeline
void MediaControlLoadingPanelElement::PopulateShadowDOM() {
  ShadowRoot* shadow_root = GetShadowRoot();
  DCHECK(!shadow_root->HasChildren());

  // This stylesheet element and will contain rules that are specific to the
  // loading panel. The shadow DOM protects these rules and rules from the
  // parent DOM from bleeding across the shadow DOM boundary.
  auto* style = HTMLStyleElement::Create(GetDocument(), CreateElementFlags());
  style->setTextContent(
      MediaControlsResourceLoader::GetShadowLoadingStyleSheet());
  shadow_root->AppendChild(style);

  // The spinner frame is centers the spinner in the middle of the element and
  // cuts off any overflowing content. It also contains a SVG mask which will
  // overlay the spinner and cover up any rough edges created by the moving
  // elements.
  HTMLDivElement* spinner_frame =
      MediaControlElementsHelper::CreateDivWithId("spinner-frame", shadow_root);

  // The spinner is responsible for rotating the elements below. The square
  // edges will be cut off by the frame above.
  spinner_ =
      MediaControlElementsHelper::CreateDivWithId("spinner", spinner_frame);
  SetSpinnerClassIfNecessary();

  // The layer performs a secondary "fill-unfill-rotate" animation.
  HTMLDivElement* layer =
      MediaControlElementsHelper::CreateDivWithId("layer", spinner_);

  // The spinner is split into two halves, one on the left (1) and the other
  // on the right (2). The mask elements stop the background from overlapping
  // each other. The background elements rotate a SVG mask from the bottom to
  // the top. The mask contains a white background with a transparent cutout
  // that forms the look of the transparent spinner. The background should
  // always be bigger than the mask in order to ensure there are no gaps
  // created by the animation.
  HTMLDivElement* mask1 =
      MediaControlElementsHelper::CreateDivWithId("spinner-mask-1", layer);
  mask1_background_ = MediaControlElementsHelper::CreateDivWithId(
      "spinner-mask-1-background", mask1);
  HTMLDivElement* mask2 =
      MediaControlElementsHelper::CreateDivWithId("spinner-mask-2", layer);
  mask2_background_ = MediaControlElementsHelper::CreateDivWithId(
      "spinner-mask-2-background", mask2);

  event_listener_ = new MediaControlAnimationEventListener(this);

  // The four cutoffs are responsible for filling the background of the loading
  // panel with white, whilst leaving a small box in the middle that is
  // transparent. This is where the spinner will be.
  MediaControlElementsHelper::CreateDivWithId("cutoff-1", shadow_root);
  MediaControlElementsHelper::CreateDivWithId("cutoff-2", shadow_root);
  MediaControlElementsHelper::CreateDivWithId("cutoff-3", shadow_root);
  MediaControlElementsHelper::CreateDivWithId("cutoff-4", shadow_root);

  // The fake timeline creates a fake bar at the bottom to look like the
  // timeline.
  MediaControlElementsHelper::CreateDivWithId("fake-timeline", shadow_root);
}

void MediaControlLoadingPanelElement::RemovedFrom(
    ContainerNode* insertion_point) {
  if (event_listener_) {
    event_listener_->Detach();
    event_listener_.Clear();
  }

  MediaControlDivElement::RemovedFrom(insertion_point);
}

void MediaControlLoadingPanelElement::CleanupShadowDOM() {
  // Clear the shadow DOM children and all references to it.
  ShadowRoot* shadow_root = GetShadowRoot();
  DCHECK(shadow_root->HasChildren());
  if (event_listener_) {
    event_listener_->Detach();
    event_listener_.Clear();
  }
  shadow_root->RemoveChildren();

  spinner_.Clear();
  mask1_background_.Clear();
  mask2_background_.Clear();
}

void MediaControlLoadingPanelElement::SetAnimationIterationCount(
    const String& count_value) {
  mask1_background_->style()->setProperty(&GetDocument(),
                                          kAnimationIterationCountName,
                                          count_value, "", ASSERT_NO_EXCEPTION);
  mask2_background_->style()->setProperty(&GetDocument(),
                                          kAnimationIterationCountName,
                                          count_value, "", ASSERT_NO_EXCEPTION);
}

void MediaControlLoadingPanelElement::UpdateDisplayState() {
  switch (state_) {
    case State::kHidden:
      // If the media controls are loading metadata then we should show the
      // loading panel and insert it into the DOM.
      if (GetMediaControls().State() == MediaControlsImpl::kLoadingMetadata &&
          !controls_hidden_) {
        PopulateShadowDOM();
        SetIsWanted(true);
        SetAnimationIterationCount(kInfinite);
        state_ = State::kPlaying;
      }
      break;
    case State::kPlaying:
      // If the media controls are either stopped or playing then we should
      // hide the loading panel, but not until the current cycle of animations
      // is complete.
      if (GetMediaControls().State() != MediaControlsImpl::kLoadingMetadata) {
        SetAnimationIterationCount(WTF::String::Number(animation_count_ + 1));
        state_ = State::kCoolingDown;
      }
      break;
    case State::kCoolingDown:
      // Do nothing.
      break;
  }
}

void MediaControlLoadingPanelElement::OnControlsHidden() {
  controls_hidden_ = true;

  // If the animation is currently playing, clean it up.
  if (state_ != State::kHidden)
    HideAnimation();
}

void MediaControlLoadingPanelElement::HideAnimation() {
  DCHECK(state_ != State::kHidden);

  SetIsWanted(false);
  state_ = State::kHidden;
  animation_count_ = 0;
  CleanupShadowDOM();
}

void MediaControlLoadingPanelElement::OnControlsShown() {
  controls_hidden_ = false;
  UpdateDisplayState();
}

void MediaControlLoadingPanelElement::OnAnimationEnd() {
  // If we have gone back to the loading metadata state (e.g. the source
  // changed). Then we should jump back to playing.
  if (GetMediaControls().State() == MediaControlsImpl::kLoadingMetadata) {
    state_ = State::kPlaying;
    SetAnimationIterationCount(kInfinite);
    return;
  }

  // The animation has finished so we can go back to the hidden state and
  // cleanup the shadow DOM.
  HideAnimation();
}

void MediaControlLoadingPanelElement::OnAnimationIteration() {
  animation_count_ += 1;
  SetSpinnerClassIfNecessary();
}

void MediaControlLoadingPanelElement::SetSpinnerClassIfNecessary() {
  if (!spinner_)
    return;

  HTMLVideoElement& video_element =
      static_cast<HTMLVideoElement&>(MediaElement());
  if (!video_element.ShouldDisplayPosterImage() &&
      !video_element.HasAvailableVideoFrame()) {
    if (!spinner_->hasAttribute(kClassAttributeName)) {
      spinner_->setAttribute(kClassAttributeName,
                             kNoFrameAvailableSpinnerClass);
    }
  } else {
    spinner_->removeAttribute(kClassAttributeName);
  }
}

Element& MediaControlLoadingPanelElement::WatchedAnimationElement() const {
  DCHECK(mask1_background_);
  return *mask1_background_;
}

void MediaControlLoadingPanelElement::Trace(blink::Visitor* visitor) {
  MediaControlAnimationEventListener::Observer::Trace(visitor);
  MediaControlDivElement::Trace(visitor);
  visitor->Trace(event_listener_);
  visitor->Trace(spinner_);
  visitor->Trace(mask1_background_);
  visitor->Trace(mask2_background_);
}

}  // namespace blink
