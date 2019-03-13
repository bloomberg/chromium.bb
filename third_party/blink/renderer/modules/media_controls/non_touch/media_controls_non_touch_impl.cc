// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_controls/non_touch/media_controls_non_touch_impl.h"

#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/modules/media_controls/non_touch/media_controls_non_touch_media_event_listener.h"
#include "third_party/blink/renderer/platform/keyboard_codes.h"

namespace blink {

MediaControlsNonTouchImpl::MediaControlsNonTouchImpl(
    HTMLMediaElement& media_element)
    : HTMLDivElement(media_element.GetDocument()),
      MediaControls(media_element),
      media_event_listener_(
          MakeGarbageCollected<MediaControlsNonTouchMediaEventListener>(
              media_element)) {
  SetShadowPseudoId(AtomicString("-internal-media-controls-non-touch"));
  media_event_listener_->AddObserver(this);
}

MediaControlsNonTouchImpl* MediaControlsNonTouchImpl::Create(
    HTMLMediaElement& media_element,
    ShadowRoot& shadow_root) {
  MediaControlsNonTouchImpl* controls =
      MakeGarbageCollected<MediaControlsNonTouchImpl>(media_element);
  shadow_root.ParserAppendChild(controls);
  return controls;
}

Node::InsertionNotificationRequest MediaControlsNonTouchImpl::InsertedInto(
    ContainerNode& root) {
  media_event_listener_->Attach();
  return HTMLDivElement::InsertedInto(root);
}

void MediaControlsNonTouchImpl::RemovedFrom(ContainerNode& insertion_point) {
  HTMLDivElement::RemovedFrom(insertion_point);
  Hide();
  media_event_listener_->Detach();
}

void MediaControlsNonTouchImpl::MaybeShow() {
  // show controls
}

void MediaControlsNonTouchImpl::Hide() {
  // hide controls
}

void MediaControlsNonTouchImpl::OnFocusIn() {
  if (MediaElement().ShouldShowControls())
    MaybeShow();
}

void MediaControlsNonTouchImpl::OnKeyDown(KeyboardEvent* event) {
  bool handled = true;
  switch (event->keyCode()) {
    case VKEY_RETURN:
      MediaElement().TogglePlayState();
      break;
    case VKEY_LEFT:
    case VKEY_RIGHT:
    case VKEY_UP:
    case VKEY_DOWN:
      // do something
      break;
    default:
      handled = false;
      break;
  }

  if (handled)
    event->SetDefaultHandled();
}

void MediaControlsNonTouchImpl::Trace(blink::Visitor* visitor) {
  visitor->Trace(media_event_listener_);
  MediaControls::Trace(visitor);
  HTMLDivElement::Trace(visitor);
}

}  // namespace blink
