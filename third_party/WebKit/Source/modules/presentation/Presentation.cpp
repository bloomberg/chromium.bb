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
#include "wtf/Vector.h"

namespace blink {

Presentation::Presentation(LocalFrame* frame) : ContextClient(frame) {}

// static
Presentation* Presentation::create(LocalFrame* frame) {
  ASSERT(frame);
  Presentation* presentation = new Presentation(frame);
  PresentationController* controller = PresentationController::from(*frame);
  ASSERT(controller);
  controller->setPresentation(presentation);
  return presentation;
}

DEFINE_TRACE(Presentation) {
  visitor->trace(m_defaultRequest);
  visitor->trace(m_receiver);
  ContextClient::trace(visitor);
}

PresentationRequest* Presentation::defaultRequest() const {
  return m_defaultRequest;
}

void Presentation::setDefaultRequest(PresentationRequest* request) {
  m_defaultRequest = request;

  if (!frame())
    return;

  PresentationController* controller = PresentationController::from(*frame());
  if (!controller)
    return;
  controller->setDefaultRequestUrl(request ? request->urls()
                                           : WTF::Vector<KURL>());
}

PresentationReceiver* Presentation::receiver() {
  if (!frame() || !frame()->settings())
    return nullptr;

  if (!frame()->settings()->getPresentationReceiver())
    return nullptr;

  if (!m_receiver) {
    PresentationController* controller = PresentationController::from(*frame());
    auto* client = controller ? controller->client() : nullptr;
    m_receiver = new PresentationReceiver(frame(), client);
  }

  return m_receiver;
}

}  // namespace blink
