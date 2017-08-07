/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#include "public/web/WebDocument.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/V8ElementRegistrationOptions.h"
#include "core/css/StyleSheetContents.h"
#include "core/dom/CSSSelectorWatch.h"
#include "core/dom/Document.h"
#include "core/dom/DocumentStatisticsCollector.h"
#include "core/dom/DocumentType.h"
#include "core/dom/Element.h"
#include "core/dom/StyleEngine.h"
#include "core/events/Event.h"
#include "core/frame/WebLocalFrameImpl.h"
#include "core/html/HTMLAllCollection.h"
#include "core/html/HTMLBodyElement.h"
#include "core/html/HTMLCollection.h"
#include "core/html/HTMLElement.h"
#include "core/html/HTMLFormElement.h"
#include "core/html/HTMLHeadElement.h"
#include "core/html/HTMLLinkElement.h"
#include "core/layout/LayoutObject.h"
#include "core/layout/api/LayoutAPIShim.h"
#include "core/layout/api/LayoutViewItem.h"
#include "core/loader/DocumentLoader.h"
#include "platform/bindings/ScriptState.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/wtf/RefPtr.h"
#include "public/platform/WebDistillability.h"
#include "public/platform/WebURL.h"
#include "public/web/WebDOMEvent.h"
#include "public/web/WebElement.h"
#include "public/web/WebElementCollection.h"
#include "public/web/WebFormElement.h"
#include "v8/include/v8.h"

namespace blink {

WebURL WebDocument::Url() const {
  return ConstUnwrap<Document>()->Url();
}

WebSecurityOrigin WebDocument::GetSecurityOrigin() const {
  if (!ConstUnwrap<Document>())
    return WebSecurityOrigin();
  return WebSecurityOrigin(ConstUnwrap<Document>()->GetSecurityOrigin());
}

bool WebDocument::IsSecureContext() const {
  const Document* document = ConstUnwrap<Document>();
  return document && document->IsSecureContext();
}

WebString WebDocument::Encoding() const {
  return ConstUnwrap<Document>()->EncodingName();
}

WebString WebDocument::ContentLanguage() const {
  return ConstUnwrap<Document>()->ContentLanguage();
}

WebString WebDocument::GetReferrer() const {
  return ConstUnwrap<Document>()->referrer();
}

WebColor WebDocument::ThemeColor() const {
  return ConstUnwrap<Document>()->ThemeColor().Rgb();
}

WebURL WebDocument::OpenSearchDescriptionURL() const {
  return const_cast<Document*>(ConstUnwrap<Document>())
      ->OpenSearchDescriptionURL();
}

WebLocalFrame* WebDocument::GetFrame() const {
  return WebLocalFrameImpl::FromFrame(ConstUnwrap<Document>()->GetFrame());
}

bool WebDocument::IsHTMLDocument() const {
  return ConstUnwrap<Document>()->IsHTMLDocument();
}

bool WebDocument::IsXHTMLDocument() const {
  return ConstUnwrap<Document>()->IsXHTMLDocument();
}

bool WebDocument::IsPluginDocument() const {
  return ConstUnwrap<Document>()->IsPluginDocument();
}

WebURL WebDocument::BaseURL() const {
  return ConstUnwrap<Document>()->BaseURL();
}

WebURL WebDocument::FirstPartyForCookies() const {
  return ConstUnwrap<Document>()->FirstPartyForCookies();
}

WebElement WebDocument::DocumentElement() const {
  return WebElement(ConstUnwrap<Document>()->documentElement());
}

WebElement WebDocument::Body() const {
  return WebElement(ConstUnwrap<Document>()->body());
}

WebElement WebDocument::Head() {
  return WebElement(Unwrap<Document>()->head());
}

WebString WebDocument::Title() const {
  return WebString(ConstUnwrap<Document>()->title());
}

WebString WebDocument::ContentAsTextForTesting() const {
  if (Element* document_element = ConstUnwrap<Document>()->documentElement())
    return WebString(document_element->innerText());
  return WebString();
}

WebElementCollection WebDocument::All() {
  return WebElementCollection(Unwrap<Document>()->all());
}

void WebDocument::Forms(WebVector<WebFormElement>& results) const {
  HTMLCollection* forms =
      const_cast<Document*>(ConstUnwrap<Document>())->forms();
  size_t source_length = forms->length();
  Vector<WebFormElement> temp;
  temp.ReserveCapacity(source_length);
  for (size_t i = 0; i < source_length; ++i) {
    Element* element = forms->item(i);
    // Strange but true, sometimes node can be 0.
    if (element && element->IsHTMLElement())
      temp.push_back(WebFormElement(toHTMLFormElement(element)));
  }
  results.Assign(temp);
}

WebURL WebDocument::CompleteURL(const WebString& partial_url) const {
  return ConstUnwrap<Document>()->CompleteURL(partial_url);
}

WebElement WebDocument::GetElementById(const WebString& id) const {
  return WebElement(ConstUnwrap<Document>()->getElementById(id));
}

WebElement WebDocument::FocusedElement() const {
  return WebElement(ConstUnwrap<Document>()->FocusedElement());
}

WebStyleSheetId WebDocument::InsertStyleSheet(const WebString& source_code) {
  Document* document = Unwrap<Document>();
  DCHECK(document);
  StyleSheetContents* parsed_sheet =
      StyleSheetContents::Create(CSSParserContext::Create(*document));
  parsed_sheet->ParseString(source_code);
  return document->GetStyleEngine().InjectAuthorSheet(parsed_sheet);
}

void WebDocument::RemoveInsertedStyleSheet(WebStyleSheetId stylesheet_id) {
  Unwrap<Document>()->GetStyleEngine().RemoveInjectedAuthorSheet(stylesheet_id);
}

void WebDocument::WatchCSSSelectors(const WebVector<WebString>& web_selectors) {
  Document* document = Unwrap<Document>();
  CSSSelectorWatch* watch = CSSSelectorWatch::FromIfExists(*document);
  if (!watch && web_selectors.empty())
    return;
  Vector<String> selectors;
  selectors.Append(web_selectors.Data(), web_selectors.size());
  CSSSelectorWatch::From(*document).WatchCSSSelectors(selectors);
}

WebReferrerPolicy WebDocument::GetReferrerPolicy() const {
  return static_cast<WebReferrerPolicy>(
      ConstUnwrap<Document>()->GetReferrerPolicy());
}

WebString WebDocument::OutgoingReferrer() {
  return WebString(Unwrap<Document>()->OutgoingReferrer());
}

WebVector<WebDraggableRegion> WebDocument::DraggableRegions() const {
  WebVector<WebDraggableRegion> draggable_regions;
  const Document* document = ConstUnwrap<Document>();
  if (document->HasAnnotatedRegions()) {
    const Vector<AnnotatedRegionValue>& regions = document->AnnotatedRegions();
    draggable_regions = WebVector<WebDraggableRegion>(regions.size());
    for (size_t i = 0; i < regions.size(); i++) {
      const AnnotatedRegionValue& value = regions[i];
      draggable_regions[i].draggable = value.draggable;
      draggable_regions[i].bounds = IntRect(value.bounds);
    }
  }
  return draggable_regions;
}

v8::Local<v8::Value> WebDocument::RegisterEmbedderCustomElement(
    const WebString& name,
    v8::Local<v8::Value> options,
    WebExceptionCode& ec) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  Document* document = Unwrap<Document>();
  DummyExceptionStateForTesting exception_state;
  ElementRegistrationOptions registration_options;
  V8ElementRegistrationOptions::toImpl(isolate, options, registration_options,
                                       exception_state);
  if (exception_state.HadException())
    return v8::Local<v8::Value>();
  ScriptValue constructor = document->registerElement(
      ScriptState::Current(isolate), name, registration_options,
      exception_state, V0CustomElement::kEmbedderNames);
  ec = exception_state.Code();
  if (exception_state.HadException())
    return v8::Local<v8::Value>();
  return constructor.V8Value();
}

WebURL WebDocument::ManifestURL() const {
  const Document* document = ConstUnwrap<Document>();
  HTMLLinkElement* link_element = document->LinkManifest();
  if (!link_element)
    return WebURL();
  return link_element->Href();
}

bool WebDocument::ManifestUseCredentials() const {
  const Document* document = ConstUnwrap<Document>();
  HTMLLinkElement* link_element = document->LinkManifest();
  if (!link_element)
    return false;
  return EqualIgnoringASCIICase(
      link_element->FastGetAttribute(HTMLNames::crossoriginAttr),
      "use-credentials");
}

WebDistillabilityFeatures WebDocument::DistillabilityFeatures() {
  return DocumentStatisticsCollector::CollectStatistics(*Unwrap<Document>());
}

WebDocument::WebDocument(Document* elem) : WebNode(elem) {}

DEFINE_WEB_NODE_TYPE_CASTS(WebDocument, ConstUnwrap<Node>()->IsDocumentNode());

WebDocument& WebDocument::operator=(Document* elem) {
  private_ = elem;
  return *this;
}

WebDocument::operator Document*() const {
  return ToDocument(private_.Get());
}

}  // namespace blink
