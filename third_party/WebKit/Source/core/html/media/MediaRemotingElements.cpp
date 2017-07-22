// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/media/MediaRemotingElements.h"

#include "core/dom/ShadowRoot.h"
#include "core/events/PointerEvent.h"
#include "core/geometry/DOMRect.h"
#include "core/html/HTMLVideoElement.h"
#include "core/input/EventHandler.h"
#include "platform/text/PlatformLocale.h"
#include "public/platform/WebLocalizedString.h"

namespace blink {

using namespace HTMLNames;

// ----------------------------

class MediaRemotingExitButtonElement::PointerEventsListener final
    : public EventListener {
 public:
  explicit PointerEventsListener(MediaRemotingExitButtonElement& element)
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

    PointerEvent* pointer_event = ToPointerEvent(event);
    DOMRect* client_rect = element_->getBoundingClientRect();
    const double x = pointer_event->x();
    const double y = pointer_event->y();
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
  listener_ = new PointerEventsListener(*this);
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

DEFINE_TRACE(MediaRemotingExitButtonElement) {
  visitor->Trace(interstitial_);
  visitor->Trace(listener_);
  HTMLDivElement::Trace(visitor);
}

// ----------------------------

MediaRemotingCastMessageElement::MediaRemotingCastMessageElement(
    MediaRemotingInterstitial& interstitial)
    : HTMLDivElement(interstitial.GetDocument()) {
  SetShadowPseudoId(AtomicString("-internal-media-remoting-cast-text-message"));
  setInnerText(interstitial.GetVideoElement().GetLocale().QueryString(
                   WebLocalizedString::kMediaRemotingCastText),
               ASSERT_NO_EXCEPTION);
}

// ----------------------------

MediaRemotingCastIconElement::MediaRemotingCastIconElement(
    MediaRemotingInterstitial& interstitial)
    : HTMLDivElement(interstitial.GetDocument()) {
  SetShadowPseudoId(AtomicString("-internal-media-remoting-cast-icon"));
}

}  // namespace blink
