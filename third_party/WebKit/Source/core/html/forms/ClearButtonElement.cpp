/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "core/html/forms/ClearButtonElement.h"

#include "core/events/MouseEvent.h"
#include "core/frame/LocalFrame.h"
#include "core/html/shadow/ShadowElementNames.h"
#include "core/input/EventHandler.h"
#include "core/layout/LayoutObject.h"

namespace blink {

using namespace HTMLNames;

inline ClearButtonElement::ClearButtonElement(
    Document& document,
    ClearButtonOwner& clear_button_owner)
    : HTMLDivElement(document), clear_button_owner_(&clear_button_owner) {}

ClearButtonElement* ClearButtonElement::Create(
    Document& document,
    ClearButtonOwner& clear_button_owner) {
  ClearButtonElement* element =
      new ClearButtonElement(document, clear_button_owner);
  element->SetShadowPseudoId(AtomicString("-webkit-clear-button"));
  element->setAttribute(idAttr, ShadowElementNames::ClearButton());
  return element;
}

void ClearButtonElement::DetachLayoutTree(const AttachContext& context) {
  HTMLDivElement::DetachLayoutTree(context);
}

void ClearButtonElement::DefaultEventHandler(Event* event) {
  if (!clear_button_owner_) {
    if (!event->DefaultHandled())
      HTMLDivElement::DefaultEventHandler(event);
    return;
  }

  if (!clear_button_owner_->ShouldClearButtonRespondToMouseEvents()) {
    if (!event->DefaultHandled())
      HTMLDivElement::DefaultEventHandler(event);
    return;
  }

  if (event->type() == EventTypeNames::click) {
    if (GetLayoutObject() && GetLayoutObject()->VisibleToHitTesting()) {
      clear_button_owner_->FocusAndSelectClearButtonOwner();
      clear_button_owner_->ClearValue();
      event->SetDefaultHandled();
    }
  }

  if (!event->DefaultHandled())
    HTMLDivElement::DefaultEventHandler(event);
}

bool ClearButtonElement::IsClearButtonElement() const {
  return true;
}

void ClearButtonElement::Trace(blink::Visitor* visitor) {
  visitor->Trace(clear_button_owner_);
  HTMLDivElement::Trace(visitor);
}

}  // namespace blink
