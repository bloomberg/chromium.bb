/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/html/HTMLShadowElement.h"

#include "core/HTMLNames.h"
#include "core/dom/Document.h"
#include "core/dom/ShadowRoot.h"
#include "core/frame/UseCounter.h"
#include "core/inspector/ConsoleMessage.h"

namespace blink {

class Document;

inline HTMLShadowElement::HTMLShadowElement(Document& document)
    : InsertionPoint(HTMLNames::shadowTag, document) {
  UseCounter::Count(document, WebFeature::kHTMLShadowElement);
}

DEFINE_NODE_FACTORY(HTMLShadowElement)

HTMLShadowElement::~HTMLShadowElement() {}

ShadowRoot* HTMLShadowElement::OlderShadowRoot() {
  ShadowRoot* containing_root = ContainingShadowRoot();
  if (!containing_root)
    return nullptr;

  UpdateDistribution();

  ShadowRoot* older = containing_root->OlderShadowRoot();
  if (!older || !older->IsOpenOrV0() ||
      older->ShadowInsertionPointOfYoungerShadowRoot() != this)
    return nullptr;

  DCHECK(older->IsOpenOrV0());
  return older;
}

Node::InsertionNotificationRequest HTMLShadowElement::InsertedInto(
    ContainerNode* insertion_point) {
  if (insertion_point->isConnected()) {
    // Warn if trying to reproject between user agent and author shadows.
    ShadowRoot* root = ContainingShadowRoot();
    if (root && root->OlderShadowRoot() &&
        root->GetType() != root->OlderShadowRoot()->GetType()) {
      String message =
          String::Format("<shadow> doesn't work for %s element host.",
                         root->host().tagName().Utf8().data());
      GetDocument().AddConsoleMessage(ConsoleMessage::Create(
          kRenderingMessageSource, kWarningMessageLevel, message));
    }
  }
  return InsertionPoint::InsertedInto(insertion_point);
}

}  // namespace blink
