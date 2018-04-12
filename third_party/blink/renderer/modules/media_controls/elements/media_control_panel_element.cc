// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_controls/elements/media_control_panel_element.h"

#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_elements_helper.h"
#include "third_party/blink/renderer/modules/media_controls/media_controls_impl.h"

namespace blink {

namespace {

// This is matching the `transition` property from mediaControls.css
const double kFadeOutDuration = 0.3;

// This is the class name to hide the panel.
const char kTransparentClassName[] = "transparent";

}  // anonymous namespace

MediaControlPanelElement::MediaControlPanelElement(
    MediaControlsImpl& media_controls)
    : MediaControlDivElement(media_controls, kMediaControlsPanel),
      transition_timer_(
          media_controls.GetDocument().GetTaskRunner(TaskType::kInternalMedia),
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

  removeAttribute("class");
  opaque_ = true;

  if (is_displayed_) {
    SetIsWanted(true);
    DidBecomeVisible();
  }
}

void MediaControlPanelElement::MakeTransparent() {
  if (!opaque_)
    return;

  setAttribute("class", kTransparentClassName);

  opaque_ = false;
  StartTimer();
}

void MediaControlPanelElement::SetKeepDisplayedForAccessibility(bool value) {
  keep_displayed_for_accessibility_ = value;
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
  transition_timer_.StartOneShot(kFadeOutDuration, FROM_HERE);
}

void MediaControlPanelElement::StopTimer() {
  transition_timer_.Stop();
}

void MediaControlPanelElement::TransitionTimerFired(TimerBase*) {
  if (!opaque_ && !keep_displayed_for_accessibility_)
    SetIsWanted(false);

  StopTimer();
}

void MediaControlPanelElement::DidBecomeVisible() {
  DCHECK(is_displayed_ && opaque_);
  MediaElement().MediaControlsDidBecomeVisible();
}

}  // namespace blink
