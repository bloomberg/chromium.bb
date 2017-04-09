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

const AtomicString& DocumentLanguage(const Document& document) {
  Element* html_element = document.documentElement();
  if (!html_element)
    return g_null_atom;
  return html_element->getAttribute(HTMLNames::langAttr);
}

bool HasNoTranslate(const Document& document) {
  DEFINE_STATIC_LOCAL(const AtomicString, google, ("google"));

  HTMLHeadElement* head_element = document.head();
  if (!head_element)
    return false;

  for (const HTMLMetaElement& meta_element :
       Traversal<HTMLMetaElement>::ChildrenOf(*head_element)) {
    if (meta_element.GetName() != google)
      continue;

    // Check if the tag contains content="notranslate" or value="notranslate"
    AtomicString content = meta_element.Content();
    if (content.IsNull())
      content = meta_element.getAttribute(HTMLNames::valueAttr);
    if (EqualIgnoringASCIICase(content, "notranslate"))
      return true;
  }

  return false;
}

}  // namespace

WebLanguageDetectionDetails
WebLanguageDetectionDetails::CollectLanguageDetectionDetails(
    const WebDocument& web_document) {
  const Document* document = web_document.ConstUnwrap<Document>();

  WebLanguageDetectionDetails details;
  details.content_language = document->ContentLanguage();
  details.html_language = DocumentLanguage(*document);
  details.url = document->Url();
  details.has_no_translate_meta = HasNoTranslate(*document);

  return details;
}

}  // namespace blink
