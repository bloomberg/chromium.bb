// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/portal/document_portals.h"

#include "third_party/blink/renderer/core/html/portal/html_portal_element.h"

namespace blink {

// static
const char DocumentPortals::kSupplementName[] = "DocumentPortals";

// static
DocumentPortals& DocumentPortals::From(Document& document) {
  DocumentPortals* supplement =
      Supplement<Document>::From<DocumentPortals>(document);
  if (!supplement) {
    supplement = MakeGarbageCollected<DocumentPortals>(document);
    Supplement<Document>::ProvideTo(document, supplement);
  }
  return *supplement;
}

DocumentPortals::DocumentPortals(Document& document)
    : Supplement<Document>(document) {}

void DocumentPortals::OnPortalInserted(HTMLPortalElement* portal) {
  portals_.push_back(portal);
}

void DocumentPortals::OnPortalRemoved(HTMLPortalElement* portal) {
  portals_.EraseAt(portals_.Find(portal));
}

HTMLPortalElement* DocumentPortals::GetPortal(
    const base::UnguessableToken& token) const {
  for (HTMLPortalElement* portal : portals_) {
    if (portal->GetToken() == token)
      return portal;
  }

  return nullptr;
}

bool DocumentPortals::IsPortalInDocumentActivating() const {
  for (HTMLPortalElement* portal : portals_) {
    if (portal->IsActivating())
      return true;
  }
  return false;
}

void DocumentPortals::Trace(Visitor* visitor) {
  Supplement<Document>::Trace(visitor);
  visitor->Trace(portals_);
}

}  // namespace blink
