/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "core/page/PointerLockController.h"

#include "core/dom/Element.h"
#include "core/dom/events/Event.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/page/ChromeClient.h"
#include "core/page/Page.h"
#include "public/platform/WebMouseEvent.h"

namespace blink {

PointerLockController::PointerLockController(Page* page)
    : page_(page), lock_pending_(false) {}

PointerLockController* PointerLockController::Create(Page* page) {
  return new PointerLockController(page);
}

void PointerLockController::RequestPointerLock(Element* target) {
  if (!target || !target->isConnected() ||
      document_of_removed_element_while_waiting_for_unlock_) {
    EnqueueEvent(EventTypeNames::pointerlockerror, target);
    return;
  }

  UseCounter::CountCrossOriginIframe(
      target->GetDocument(), WebFeature::kElementRequestPointerLockIframe);
  if (target->IsInShadowTree()) {
    UseCounter::Count(target->GetDocument(),
                      WebFeature::kElementRequestPointerLockInShadow);
  }

  if (target->GetDocument().IsSandboxed(kSandboxPointerLock)) {
    // FIXME: This message should be moved off the console once a solution to
    // https://bugs.webkit.org/show_bug.cgi?id=103274 exists.
    target->GetDocument().AddConsoleMessage(ConsoleMessage::Create(
        kSecurityMessageSource, kErrorMessageLevel,
        "Blocked pointer lock on an element because the element's frame is "
        "sandboxed and the 'allow-pointer-lock' permission is not set."));
    EnqueueEvent(EventTypeNames::pointerlockerror, target);
    return;
  }

  if (element_) {
    if (element_->GetDocument() != target->GetDocument()) {
      EnqueueEvent(EventTypeNames::pointerlockerror, target);
      return;
    }
    EnqueueEvent(EventTypeNames::pointerlockchange, target);
    element_ = target;
  } else if (page_->GetChromeClient().RequestPointerLock(
                 target->GetDocument().GetFrame())) {
    lock_pending_ = true;
    element_ = target;
  } else {
    EnqueueEvent(EventTypeNames::pointerlockerror, target);
  }
}

void PointerLockController::RequestPointerUnlock() {
  return page_->GetChromeClient().RequestPointerUnlock(
      element_->GetDocument().GetFrame());
}

void PointerLockController::ElementRemoved(Element* element) {
  if (element_ == element) {
    document_of_removed_element_while_waiting_for_unlock_ =
        &element_->GetDocument();
    RequestPointerUnlock();
    // Set element null immediately to block any future interaction with it
    // including mouse events received before the unlock completes.
    ClearElement();
  }
}

void PointerLockController::DocumentDetached(Document* document) {
  if (element_ && element_->GetDocument() == document) {
    RequestPointerUnlock();
    ClearElement();
  }
}

bool PointerLockController::LockPending() const {
  return lock_pending_;
}

Element* PointerLockController::GetElement() const {
  return element_.Get();
}

void PointerLockController::DidAcquirePointerLock() {
  EnqueueEvent(EventTypeNames::pointerlockchange, element_.Get());
  lock_pending_ = false;
}

void PointerLockController::DidNotAcquirePointerLock() {
  EnqueueEvent(EventTypeNames::pointerlockerror, element_.Get());
  ClearElement();
}

void PointerLockController::DidLosePointerLock() {
  EnqueueEvent(
      EventTypeNames::pointerlockchange,
      element_ ? &element_->GetDocument()
               : document_of_removed_element_while_waiting_for_unlock_.Get());
  ClearElement();
  document_of_removed_element_while_waiting_for_unlock_ = nullptr;
}

void PointerLockController::DispatchLockedMouseEvent(
    const WebMouseEvent& event,
    const AtomicString& event_type) {
  if (!element_ || !element_->GetDocument().GetFrame())
    return;

  element_->DispatchMouseEvent(event, event_type, event.click_count);

  // Event handlers may remove element.
  if (!element_)
    return;

  // Create click events
  if (event_type == EventTypeNames::mouseup) {
    element_->DispatchMouseEvent(event, EventTypeNames::click,
                                 event.click_count);
  }
}

void PointerLockController::ClearElement() {
  lock_pending_ = false;
  element_ = nullptr;
}

void PointerLockController::EnqueueEvent(const AtomicString& type,
                                         Element* element) {
  if (element)
    EnqueueEvent(type, &element->GetDocument());
}

void PointerLockController::EnqueueEvent(const AtomicString& type,
                                         Document* document) {
  if (document && document->domWindow())
    document->domWindow()->EnqueueDocumentEvent(Event::Create(type));
}

void PointerLockController::Trace(blink::Visitor* visitor) {
  visitor->Trace(page_);
  visitor->Trace(element_);
  visitor->Trace(document_of_removed_element_while_waiting_for_unlock_);
}

}  // namespace blink
