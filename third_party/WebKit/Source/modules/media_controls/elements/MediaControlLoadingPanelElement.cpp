// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/media_controls/elements/MediaControlLoadingPanelElement.h"

#include "core/dom/ElementShadow.h"
#include "core/dom/events/Event.h"
#include "core/dom/events/EventListener.h"
#include "core/html/HTMLDivElement.h"
#include "core/html/HTMLStyleElement.h"
#include "core/html/media/HTMLMediaElement.h"
#include "core/style/ComputedStyle.h"
#include "modules/media_controls/MediaControlsImpl.h"
#include "modules/media_controls/MediaControlsResourceLoader.h"
#include "modules/media_controls/elements/MediaControlElementsHelper.h"

namespace {

static const char kAnimationIterationCountName[] = "animation-iteration-count";
static const char kInfinite[] = "infinite";

}  // namespace

namespace blink {

// Listens for animationend and animationiteration DOM events on a HTML element
// provided by the loading panel. When the events are called it calls the
// OnAnimation* methods on the loading panel.
//
// This exists because we need to know when the animation ends so we can reset
// the element and we also need to keep track of how many iterations the
// animation has gone through so we can nicely stop the animation at the end of
// the current one.
class MediaControlLoadingPanelElement::AnimationEventListener final
    : public EventListener {
 public:
  AnimationEventListener(MediaControlLoadingPanelElement* panel)
      : EventListener(EventListener::kCPPEventListenerType), panel_(panel) {
    Object().addEventListener(EventTypeNames::animationend, this, false);
    Object().addEventListener(EventTypeNames::animationiteration, this, false);
  };

  void Detach() {
    Object().removeEventListener(EventTypeNames::animationend, this, false);
    Object().removeEventListener(EventTypeNames::animationiteration, this,
                                 false);
  };

  bool operator==(const EventListener& other) const override {
    return this == &other;
  };

  virtual void Trace(blink::Visitor* visitor) {
    visitor->Trace(panel_);
    EventListener::Trace(visitor);
  };

 private:
  void handleEvent(ExecutionContext* execution_context, Event* event) {
    if (event->type() == EventTypeNames::animationend) {
      panel_->OnAnimationEnd();
      return;
    }
    if (event->type() == EventTypeNames::animationiteration) {
      panel_->OnAnimationIteration();
      return;
    }

    NOTREACHED();
  };

  HTMLDivElement& Object() { return *panel_->mask1_background_; };

  Member<MediaControlLoadingPanelElement> panel_;
};

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
  ShadowRoot* shadow_root = YoungestShadowRoot();
  DCHECK(!shadow_root->HasChildren());

  // This stylesheet element and will contain rules that are specific to the
  // loading panel. The shadow DOM protects these rules and rules from the
  // parent DOM from bleeding across the shadow DOM boundary.
  HTMLStyleElement* style = HTMLStyleElement::Create(GetDocument(), false);
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
  HTMLDivElement* spinner =
      MediaControlElementsHelper::CreateDivWithId("spinner", spinner_frame);

  // The layer performs a secondary "fill-unfill-rotate" animation.
  HTMLDivElement* layer =
      MediaControlElementsHelper::CreateDivWithId("layer", spinner);

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

  event_listener_ =
      new MediaControlLoadingPanelElement::AnimationEventListener(this);

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
  ShadowRoot* shadow_root = YoungestShadowRoot();
  DCHECK(shadow_root->HasChildren());
  event_listener_->Detach();
  shadow_root->RemoveChildren();

  mask1_background_.Clear();
  mask2_background_.Clear();
  event_listener_.Clear();
}

void MediaControlLoadingPanelElement::SetAnimationIterationCount(
    const String& count_value) {
  mask1_background_->style()->setProperty(kAnimationIterationCountName,
                                          count_value, "", ASSERT_NO_EXCEPTION);
  mask2_background_->style()->setProperty(kAnimationIterationCountName,
                                          count_value, "", ASSERT_NO_EXCEPTION);
}

void MediaControlLoadingPanelElement::UpdateDisplayState() {
  switch (state_) {
    case State::kHidden:
      // If the media controls are loading metadata then we should show the
      // loading panel and insert it into the DOM.
      if (GetMediaControls().State() == MediaControlsImpl::kLoadingMetadata) {
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
  SetIsWanted(false);
  state_ = State::kHidden;
  animation_count_ = 0;
  CleanupShadowDOM();
}

void MediaControlLoadingPanelElement::OnAnimationIteration() {
  animation_count_ += 1;
}

void MediaControlLoadingPanelElement::Trace(blink::Visitor* visitor) {
  MediaControlDivElement::Trace(visitor);
  visitor->Trace(event_listener_);
  visitor->Trace(mask1_background_);
  visitor->Trace(mask2_background_);
}

}  // namespace blink
