// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/media/MediaRemotingElements.h"

#include "core/dom/ShadowRoot.h"
#include "core/events/MouseEvent.h"
#include "core/geometry/DOMRect.h"
#include "core/html/media/HTMLVideoElement.h"
#include "core/input/EventHandler.h"
#include "platform/text/PlatformLocale.h"
#include "public/platform/WebLocalizedString.h"

namespace blink {

using namespace HTMLNames;

// ----------------------------

class MediaRemotingExitButtonElement::MouseEventsListener final
    : public EventListener {
 public:
  explicit MouseEventsListener(MediaRemotingExitButtonElement& element)
      : EventListener(kCPPEventListenerType), element_(element) {}

  bool operator==(const EventListener& other) const override {
    return this == &other;
  }

  void Trace(blink::Visitor* visitor) {
    visitor->Trace(element_);
    EventListener::Trace(visitor);
  }

 private:
  void handleEvent(ExecutionContext* context, Event* event) override {
    DCHECK_EQ(event->type(), EventTypeNames::click);

    MouseEvent* mouse_event = ToMouseEvent(event);
    DOMRect* client_rect = element_->getBoundingClientRect();
    const double x = mouse_event->x();
    const double y = mouse_event->y();
    if (x < client_rect->left() || x > client_rect->right() ||
        y < client_rect->top() || y > client_rect->bottom())
      return;

    element_->GetVideoElement().DisableMediaRemoting();
    event->SetDefaultHandled();
    event->stopPropagation();
  }

  Member<MediaRemotingExitButtonElement> element_;
};

MediaRemotingExitButtonElement::MediaRemotingExitButtonElement(
    MediaRemotingInterstitial& interstitial)
    : HTMLDivElement(interstitial.GetDocument()), interstitial_(interstitial) {
  listener_ = new MouseEventsListener(*this);
  SetShadowPseudoId(AtomicString("-internal-media-remoting-disable-button"));
  setInnerText(interstitial.GetVideoElement().GetLocale().QueryString(
                   WebLocalizedString::kMediaRemotingDisableText),
               ASSERT_NO_EXCEPTION);
}

void MediaRemotingExitButtonElement::OnShown() {
  GetDocument().addEventListener(EventTypeNames::click, listener_, true);
}

void MediaRemotingExitButtonElement::OnHidden() {
  GetDocument().removeEventListener(EventTypeNames::click, listener_, true);
}

HTMLVideoElement& MediaRemotingExitButtonElement::GetVideoElement() const {
  return interstitial_->GetVideoElement();
}

void MediaRemotingExitButtonElement::Trace(blink::Visitor* visitor) {
  visitor->Trace(interstitial_);
  visitor->Trace(listener_);
  HTMLDivElement::Trace(visitor);
}

// ----------------------------

MediaRemotingCastMessageElement::MediaRemotingCastMessageElement(
    MediaRemotingInterstitial& interstitial)
    : HTMLDivElement(interstitial.GetDocument()) {
  SetShadowPseudoId(AtomicString("-internal-media-remoting-cast-text-message"));
}

// ----------------------------

MediaRemotingCastIconElement::MediaRemotingCastIconElement(
    MediaRemotingInterstitial& interstitial)
    : HTMLDivElement(interstitial.GetDocument()) {
  SetShadowPseudoId(AtomicString("-internal-media-remoting-cast-icon"));
}

}  // namespace blink
