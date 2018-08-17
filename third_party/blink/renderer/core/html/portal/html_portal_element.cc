// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/portal/html_portal_element.h"

#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/html_unknown_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

HTMLPortalElement::HTMLPortalElement(Document& document)
    : HTMLFrameOwnerElement(HTMLNames::portalTag, document) {}

HTMLPortalElement::~HTMLPortalElement() {}

HTMLElement* HTMLPortalElement::Create(Document& document) {
  if (RuntimeEnabledFeatures::PortalsEnabled())
    return new HTMLPortalElement(document);
  return HTMLUnknownElement::Create(HTMLNames::portalTag, document);
}

HTMLPortalElement::InsertionNotificationRequest HTMLPortalElement::InsertedInto(
    ContainerNode& node) {
  auto result = HTMLFrameOwnerElement::InsertedInto(node);

  Document& document = GetDocument();

  if (node.IsInDocumentTree() && document.IsHTMLDocument()) {
    document.GetFrame()->GetInterfaceProvider().GetInterface(
        mojo::MakeRequest(&portal_ptr_));
  }

  return result;
}

void HTMLPortalElement::RemovedFrom(ContainerNode* node) {
  HTMLFrameOwnerElement::RemovedFrom(node);

  Document& document = GetDocument();

  if (node->IsInDocumentTree() && document.IsHTMLDocument()) {
    portal_ptr_.reset();
  }
}

}  // namespace blink
