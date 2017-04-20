/*
 * Copyright (C) 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/html/shadow/MediaControlElements.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/InputTypeNames.h"
#include "core/dom/ClientRect.h"
#include "core/dom/TaskRunnerHelper.h"
#include "core/dom/Text.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/events/KeyboardEvent.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/UseCounter.h"
#include "core/html/HTMLVideoElement.h"
#include "core/html/media/MediaControls.h"
#include "core/html/shadow/ShadowElementNames.h"
#include "core/html/track/TextTrackList.h"
#include "core/input/EventHandler.h"
#include "core/layout/LayoutBoxModelObject.h"
#include "core/layout/api/LayoutSliderItem.h"
#include "platform/Histogram.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "public/platform/Platform.h"
#include "public/platform/UserMetricsAction.h"

namespace blink {

using namespace HTMLNames;

namespace {

bool IsUserInteractionEvent(Event* event) {
  const AtomicString& type = event->type();
  return type == EventTypeNames::mousedown || type == EventTypeNames::mouseup ||
         type == EventTypeNames::click || type == EventTypeNames::dblclick ||
         event->IsKeyboardEvent() || event->IsTouchEvent();
}

// Sliders (the volume control and timeline) need to capture some additional
// events used when dragging the thumb.
bool IsUserInteractionEventForSlider(Event* event,
                                     LayoutObject* layout_object) {
  // It is unclear if this can be converted to isUserInteractionEvent(), since
  // mouse* events seem to be eaten during a drag anyway.  crbug.com/516416 .
  if (IsUserInteractionEvent(event))
    return true;

  // Some events are only captured during a slider drag.
  LayoutSliderItem slider = LayoutSliderItem(ToLayoutSlider(layout_object));
  // TODO(crbug.com/695459#c1): LayoutSliderItem::inDragMode is incorrectly
  // false for drags that start from the track instead of the thumb.
  // Use SliderThumbElement::m_inDragMode and
  // SliderContainerElement::m_touchStarted instead.
  if (!slider.IsNull() && !slider.InDragMode())
    return false;

  const AtomicString& type = event->type();
  return type == EventTypeNames::mouseover ||
         type == EventTypeNames::mouseout ||
         type == EventTypeNames::mousemove ||
         type == EventTypeNames::pointerover ||
         type == EventTypeNames::pointerout ||
         type == EventTypeNames::pointermove;
}

}  // anonymous namespace

MediaControlVolumeSliderElement::MediaControlVolumeSliderElement(
    MediaControls& media_controls)
    : MediaControlInputElement(media_controls, kMediaVolumeSlider) {}

MediaControlVolumeSliderElement* MediaControlVolumeSliderElement::Create(
    MediaControls& media_controls) {
  MediaControlVolumeSliderElement* slider =
      new MediaControlVolumeSliderElement(media_controls);
  slider->EnsureUserAgentShadowRoot();
  slider->setType(InputTypeNames::range);
  slider->setAttribute(stepAttr, "any");
  slider->setAttribute(maxAttr, "1");
  slider->SetShadowPseudoId(
      AtomicString("-webkit-media-controls-volume-slider"));
  return slider;
}

void MediaControlVolumeSliderElement::DefaultEventHandler(Event* event) {
  if (!isConnected() || !GetDocument().IsActive())
    return;

  MediaControlInputElement::DefaultEventHandler(event);

  if (event->type() == EventTypeNames::mousedown)
    Platform::Current()->RecordAction(
        UserMetricsAction("Media.Controls.VolumeChangeBegin"));

  if (event->type() == EventTypeNames::mouseup)
    Platform::Current()->RecordAction(
        UserMetricsAction("Media.Controls.VolumeChangeEnd"));

  if (event->type() == EventTypeNames::input) {
    double volume = value().ToDouble();
    MediaElement().setVolume(volume);
    MediaElement().setMuted(false);
    if (LayoutObject* layout_object = this->GetLayoutObject())
      layout_object->SetShouldDoFullPaintInvalidation();
  }
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

void MediaControlVolumeSliderElement::SetVolume(double volume) {
  if (value().ToDouble() == volume)
    return;

  setValue(String::Number(volume));
  if (LayoutObject* layout_object = this->GetLayoutObject())
    layout_object->SetShouldDoFullPaintInvalidation();
}

bool MediaControlVolumeSliderElement::KeepEventInNode(Event* event) {
  return IsUserInteractionEventForSlider(event, GetLayoutObject());
}

}  // namespace blink
