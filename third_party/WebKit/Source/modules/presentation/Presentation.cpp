// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/presentation/Presentation.h"

#include "core/dom/Document.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "modules/presentation/PresentationController.h"
#include "modules/presentation/PresentationReceiver.h"
#include "modules/presentation/PresentationRequest.h"
#include "platform/weborigin/KURL.h"
#include "platform/wtf/Vector.h"

namespace blink {

Presentation::Presentation(LocalFrame* frame) : ContextClient(frame) {}

// static
Presentation* Presentation::Create(LocalFrame* frame) {
  DCHECK(frame);
  Presentation* presentation = new Presentation(frame);
  PresentationController* controller = PresentationController::From(*frame);
  DCHECK(controller);
  controller->SetPresentation(presentation);
  return presentation;
}

void Presentation::Trace(blink::Visitor* visitor) {
  visitor->Trace(default_request_);
  visitor->Trace(receiver_);
  ContextClient::Trace(visitor);
}

PresentationRequest* Presentation::defaultRequest() const {
  return default_request_;
}

void Presentation::setDefaultRequest(PresentationRequest* request) {
  default_request_ = request;

  if (!GetFrame())
    return;

  PresentationController* controller =
      PresentationController::From(*GetFrame());
  if (!controller)
    return;
  controller->SetDefaultRequestUrl(request ? request->Urls()
                                           : WTF::Vector<KURL>());
}

PresentationReceiver* Presentation::receiver() {
  if (!GetFrame() || !GetFrame()->GetSettings())
    return nullptr;

  if (!GetFrame()->GetSettings()->GetPresentationReceiver())
    return nullptr;

  if (!receiver_) {
    PresentationController* controller =
        PresentationController::From(*GetFrame());
    auto* client = controller ? controller->Client() : nullptr;
    receiver_ = new PresentationReceiver(GetFrame(), client);
  }

  return receiver_;
}

}  // namespace blink
