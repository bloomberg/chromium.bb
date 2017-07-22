// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/media_controls/elements/MediaControlVolumeSliderElement.h"

#include "core/HTMLNames.h"
#include "core/InputTypeNames.h"
#include "core/events/Event.h"
#include "core/html/HTMLMediaElement.h"
#include "core/layout/LayoutObject.h"
#include "modules/media_controls/MediaControlsImpl.h"
#include "modules/media_controls/elements/MediaControlElementsHelper.h"
#include "public/platform/Platform.h"

namespace blink {

MediaControlVolumeSliderElement::MediaControlVolumeSliderElement(
    MediaControlsImpl& media_controls)
    : MediaControlInputElement(media_controls, kMediaVolumeSlider) {
  EnsureUserAgentShadowRoot();
  setType(InputTypeNames::range);
  setAttribute(HTMLNames::stepAttr, "any");
  setAttribute(HTMLNames::maxAttr, "1");
  SetShadowPseudoId(AtomicString("-webkit-media-controls-volume-slider"));
}

void MediaControlVolumeSliderElement::SetVolume(double volume) {
  if (value().ToDouble() == volume)
    return;

  setValue(String::Number(volume));
  if (LayoutObject* layout_object = this->GetLayoutObject())
    layout_object->SetShouldDoFullPaintInvalidation();
}

bool MediaControlVolumeSliderElement::WillRespondToMouseMoveEvents() {
  if (!isConnected() || !GetDocument().IsActive())
    return false;

  return MediaControlInputElement::WillRespondToMouseMoveEvents();
}

bool MediaControlVolumeSliderElement::WillRespondToMouseClickEvents() {
  if (!isConnected() || !GetDocument().IsActive())
    return false;

  return MediaControlInputElement::WillRespondToMouseClickEvents();
}

const char* MediaControlVolumeSliderElement::GetNameForHistograms() const {
  return "VolumeSlider";
}

void MediaControlVolumeSliderElement::DefaultEventHandler(Event* event) {
  if (!isConnected() || !GetDocument().IsActive())
    return;

  MediaControlInputElement::DefaultEventHandler(event);

  if (event->IsMouseEvent() || event->IsKeyboardEvent() ||
      event->IsGestureEvent() || event->IsPointerEvent()) {
    MaybeRecordInteracted();
  }

  if (event->type() == EventTypeNames::pointerdown) {
    Platform::Current()->RecordAction(
        UserMetricsAction("Media.Controls.VolumeChangeBegin"));
  }

  if (event->type() == EventTypeNames::pointerup) {
    Platform::Current()->RecordAction(
        UserMetricsAction("Media.Controls.VolumeChangeEnd"));
  }

  if (event->type() == EventTypeNames::input) {
    double volume = value().ToDouble();
    MediaElement().setVolume(volume);
    MediaElement().setMuted(false);
    if (LayoutObject* layout_object = this->GetLayoutObject())
      layout_object->SetShouldDoFullPaintInvalidation();
  }
}

bool MediaControlVolumeSliderElement::KeepEventInNode(Event* event) {
  return MediaControlElementsHelper::IsUserInteractionEventForSlider(
      event, GetLayoutObject());
}

}  // namespace blink
