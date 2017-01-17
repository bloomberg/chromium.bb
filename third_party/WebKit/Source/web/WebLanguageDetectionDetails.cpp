// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/web/WebLanguageDetectionDetails.h"

#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/ElementTraversal.h"
#include "core/html/HTMLHeadElement.h"
#include "core/html/HTMLMetaElement.h"
#include "public/web/WebDocument.h"

namespace blink {

namespace {

const AtomicString& documentLanguage(const Document& document) {
  Element* htmlElement = document.documentElement();
  if (!htmlElement)
    return nullAtom;
  return htmlElement->getAttribute(HTMLNames::langAttr);
}

bool hasNoTranslate(const Document& document) {
  DEFINE_STATIC_LOCAL(const AtomicString, google, ("google"));

  HTMLHeadElement* headElement = document.head();
  if (!headElement)
    return false;

  for (const HTMLMetaElement& metaElement :
       Traversal<HTMLMetaElement>::childrenOf(*headElement)) {
    if (metaElement.name() != google)
      continue;

    // Check if the tag contains content="notranslate" or value="notranslate"
    AtomicString content = metaElement.content();
    if (content.isNull())
      content = metaElement.getAttribute(HTMLNames::valueAttr);
    if (equalIgnoringASCIICase(content, "notranslate"))
      return true;
  }

  return false;
}

}  // namespace

WebLanguageDetectionDetails
WebLanguageDetectionDetails::collectLanguageDetectionDetails(
    const WebDocument& webDocument) {
  const Document* document = webDocument.constUnwrap<Document>();

  WebLanguageDetectionDetails details;
  details.contentLanguage = document->contentLanguage();
  details.htmlLanguage = documentLanguage(*document);
  details.url = document->url();
  details.hasNoTranslateMeta = hasNoTranslate(*document);

  return details;
}

}  // namespace blink
