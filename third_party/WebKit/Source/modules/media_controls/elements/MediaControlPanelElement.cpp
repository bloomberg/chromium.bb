// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/media_controls/elements/MediaControlPanelElement.h"

#include "core/dom/TaskRunnerHelper.h"
#include "core/dom/events/Event.h"
#include "core/html/HTMLMediaElement.h"
#include "modules/media_controls/MediaControlsImpl.h"
#include "modules/media_controls/elements/MediaControlElementsHelper.h"

namespace blink {

namespace {

// This is matching the `transition` property from mediaControls.css
const double kFadeOutDuration = 0.3;

}  // anonymous namespace

MediaControlPanelElement::MediaControlPanelElement(
    MediaControlsImpl& media_controls)
    : MediaControlDivElement(media_controls, kMediaControlsPanel),
      transition_timer_(TaskRunnerHelper::Get(TaskType::kUnspecedTimer,
                                              &media_controls.GetDocument()),
                        this,
                        &MediaControlPanelElement::TransitionTimerFired) {
  SetShadowPseudoId(AtomicString("-webkit-media-controls-panel"));
}

void MediaControlPanelElement::SetIsDisplayed(bool is_displayed) {
  if (is_displayed_ == is_displayed)
    return;

  is_displayed_ = is_displayed;
  if (is_displayed_ && opaque_)
    DidBecomeVisible();
}

bool MediaControlPanelElement::IsOpaque() const {
  return opaque_;
}

void MediaControlPanelElement::MakeOpaque() {
  if (opaque_)
    return;

  SetInlineStyleProperty(CSSPropertyOpacity, 1.0,
                         CSSPrimitiveValue::UnitType::kNumber);
  opaque_ = true;

  if (is_displayed_) {
    SetIsWanted(true);
    DidBecomeVisible();
  }
}

void MediaControlPanelElement::MakeTransparent() {
  if (!opaque_)
    return;

  SetInlineStyleProperty(CSSPropertyOpacity, 0.0,
                         CSSPrimitiveValue::UnitType::kNumber);

  opaque_ = false;
  StartTimer();
}

void MediaControlPanelElement::DefaultEventHandler(Event* event) {
  // Suppress the media element activation behavior (toggle play/pause) when
  // any part of the control panel is clicked.
  if (event->type() == EventTypeNames::click) {
    event->SetDefaultHandled();
    return;
  }
  HTMLDivElement::DefaultEventHandler(event);
}

bool MediaControlPanelElement::KeepEventInNode(Event* event) {
  return MediaControlElementsHelper::IsUserInteractionEvent(event);
}

void MediaControlPanelElement::StartTimer() {
  StopTimer();

  // The timer is required to set the property display:'none' on the panel,
  // such that captions are correctly displayed at the bottom of the video
  // at the end of the fadeout transition.
  // FIXME: Racing a transition with a setTimeout like this is wrong.
  transition_timer_.StartOneShot(kFadeOutDuration, BLINK_FROM_HERE);
}

void MediaControlPanelElement::StopTimer() {
  transition_timer_.Stop();
}

void MediaControlPanelElement::TransitionTimerFired(TimerBase*) {
  if (!opaque_)
    SetIsWanted(false);

  StopTimer();
}

void MediaControlPanelElement::DidBecomeVisible() {
  DCHECK(is_displayed_ && opaque_);
  MediaElement().MediaControlsDidBecomeVisible();
}

}  // namespace blink
