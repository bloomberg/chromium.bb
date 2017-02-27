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

#include "public/web/WebFrameSerializer.h"

#include "core/HTMLNames.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/frame/Frame.h"
#include "core/frame/FrameSerializer.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/RemoteFrame.h"
#include "core/html/HTMLAllCollection.h"
#include "core/html/HTMLFrameElementBase.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/html/HTMLImageElement.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/HTMLTableElement.h"
#include "core/layout/LayoutBox.h"
#include "core/loader/DocumentLoader.h"
#include "platform/Histogram.h"
#include "platform/SerializedResource.h"
#include "platform/SharedBuffer.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "platform/mhtml/MHTMLArchive.h"
#include "platform/mhtml/MHTMLParser.h"
#include "platform/network/ResourceRequest.h"
#include "platform/network/ResourceResponse.h"
#include "platform/weborigin/KURL.h"
#include "public/platform/WebString.h"
#include "public/platform/WebURL.h"
#include "public/platform/WebURLResponse.h"
#include "public/platform/WebVector.h"
#include "public/web/WebDataSource.h"
#include "public/web/WebDocument.h"
#include "public/web/WebFrame.h"
#include "public/web/WebFrameSerializerCacheControlPolicy.h"
#include "public/web/WebFrameSerializerClient.h"
#include "web/WebFrameSerializerImpl.h"
#include "web/WebLocalFrameImpl.h"
#include "web/WebRemoteFrameImpl.h"
#include "wtf/Assertions.h"
#include "wtf/Deque.h"
#include "wtf/HashMap.h"
#include "wtf/HashSet.h"
#include "wtf/Noncopyable.h"
#include "wtf/Vector.h"
#include "wtf/text/StringConcatenate.h"

namespace blink {

namespace {

const int kPopupOverlayZIndexThreshold = 50;

class MHTMLFrameSerializerDelegate final : public FrameSerializer::Delegate {
  WTF_MAKE_NONCOPYABLE(MHTMLFrameSerializerDelegate);

 public:
  explicit MHTMLFrameSerializerDelegate(
      WebFrameSerializer::MHTMLPartsGenerationDelegate&);
  bool shouldIgnoreElement(const Element&) override;
  bool shouldIgnoreAttribute(const Element&, const Attribute&) override;
  bool rewriteLink(const Element&, String& rewrittenLink) override;
  bool shouldSkipResourceWithURL(const KURL&) override;
  bool shouldSkipResource(
      FrameSerializer::ResourceHasCacheControlNoStoreHeader) override;
  Vector<Attribute> getCustomAttributes(const Element&) override;

 private:
  bool shouldIgnoreHiddenElement(const Element&);
  bool shouldIgnoreMetaElement(const Element&);
  bool shouldIgnorePopupOverlayElement(const Element&);
  void getCustomAttributesForImageElement(const HTMLImageElement&,
                                          Vector<Attribute>*);
  void getCustomAttributesForFormControlElement(const Element&,
                                                Vector<Attribute>*);

  WebFrameSerializer::MHTMLPartsGenerationDelegate& m_webDelegate;
};

MHTMLFrameSerializerDelegate::MHTMLFrameSerializerDelegate(
    WebFrameSerializer::MHTMLPartsGenerationDelegate& webDelegate)
    : m_webDelegate(webDelegate) {}

bool MHTMLFrameSerializerDelegate::shouldIgnoreElement(const Element& element) {
  if (shouldIgnoreHiddenElement(element))
    return true;
  if (shouldIgnoreMetaElement(element))
    return true;
  if (m_webDelegate.removePopupOverlay() &&
      shouldIgnorePopupOverlayElement(element)) {
    return true;
  }
  return false;
}

bool MHTMLFrameSerializerDelegate::shouldIgnoreHiddenElement(
    const Element& element) {
  // Do not include elements that are are set to hidden without affecting layout
  // by the page. For those elements that are hidden by default, they will not
  // be excluded:
  // 1) All elements that are head or part of head, including head, meta, style,
  //    link and etc.
  // 2) Some specific elements in body: meta, style, datalist, option and etc.
  if (element.layoutObject())
    return false;
  if (isHTMLHeadElement(element) || isHTMLMetaElement(element) ||
      isHTMLStyleElement(element) || isHTMLDataListElement(element) ||
      isHTMLOptionElement(element) || isHTMLLinkElement(element)) {
    return false;
  }
  Element* parent = element.parentElement();
  return parent && !isHTMLHeadElement(parent);
}

bool MHTMLFrameSerializerDelegate::shouldIgnoreMetaElement(
    const Element& element) {
  // Do not include meta elements that declare Content-Security-Policy
  // directives. They should have already been enforced when the original
  // document is loaded. Since only the rendered resources are encapsulated in
  // the saved MHTML page, there is no need to carry the directives. If they
  // are still kept in the MHTML, child frames that are referred to using cid:
  // scheme could be prevented from loading.
  if (!isHTMLMetaElement(element))
    return false;
  if (!element.fastHasAttribute(HTMLNames::contentAttr))
    return false;
  const AtomicString& httpEquiv =
      element.fastGetAttribute(HTMLNames::http_equivAttr);
  return httpEquiv == "Content-Security-Policy";
}

bool MHTMLFrameSerializerDelegate::shouldIgnorePopupOverlayElement(
    const Element& element) {
  // The element should be visible.
  LayoutBox* box = element.layoutBox();
  if (!box)
    return false;

  // The bounding box of the element should contain center point of the
  // viewport.
  LocalDOMWindow* window = element.document().domWindow();
  DCHECK(window);
  LayoutPoint centerPoint(window->innerWidth() / 2, window->innerHeight() / 2);
  if (!box->frameRect().contains(centerPoint))
    return false;

  // The z-index should be greater than the threshold.
  if (box->style()->zIndex() < kPopupOverlayZIndexThreshold)
    return false;

  return true;
}

bool MHTMLFrameSerializerDelegate::shouldIgnoreAttribute(
    const Element& element,
    const Attribute& attribute) {
  // TODO(fgorski): Presence of srcset attribute causes MHTML to not display
  // images, as only the value of src is pulled into the archive. Discarding
  // srcset prevents the problem. Long term we should make sure to MHTML plays
  // nicely with srcset.
  if (attribute.localName() == HTMLNames::srcsetAttr)
    return true;

  // If srcdoc attribute for frame elements will be rewritten as src attribute
  // containing link instead of html contents, don't ignore the attribute.
  // Bail out now to avoid the check in Element::isScriptingAttribute.
  bool isSrcDocAttribute = isHTMLFrameElementBase(element) &&
                           attribute.name() == HTMLNames::srcdocAttr;
  String newLinkForTheElement;
  if (isSrcDocAttribute && rewriteLink(element, newLinkForTheElement))
    return false;

  // Do not include attributes that contain javascript. This is because the
  // script will not be executed when a MHTML page is being loaded.
  return element.isScriptingAttribute(attribute);
}

bool MHTMLFrameSerializerDelegate::rewriteLink(const Element& element,
                                               String& rewrittenLink) {
  if (!element.isFrameOwnerElement())
    return false;

  auto* frameOwnerElement = toHTMLFrameOwnerElement(&element);
  Frame* frame = frameOwnerElement->contentFrame();
  if (!frame)
    return false;

  WebString contentID = m_webDelegate.getContentID(WebFrame::fromFrame(frame));
  if (contentID.isNull())
    return false;

  KURL cidURI = MHTMLParser::convertContentIDToURI(contentID);
  DCHECK(cidURI.isValid());

  if (isHTMLFrameElementBase(&element)) {
    rewrittenLink = cidURI.getString();
    return true;
  }

  if (isHTMLObjectElement(&element)) {
    Document* doc = frameOwnerElement->contentDocument();
    bool isHandledBySerializer = doc->isHTMLDocument() ||
                                 doc->isXHTMLDocument() ||
                                 doc->isImageDocument();
    if (isHandledBySerializer) {
      rewrittenLink = cidURI.getString();
      return true;
    }
  }

  return false;
}

bool MHTMLFrameSerializerDelegate::shouldSkipResourceWithURL(const KURL& url) {
  return m_webDelegate.shouldSkipResource(url);
}

bool MHTMLFrameSerializerDelegate::shouldSkipResource(
    FrameSerializer::ResourceHasCacheControlNoStoreHeader
        hasCacheControlNoStoreHeader) {
  return m_webDelegate.cacheControlPolicy() ==
             WebFrameSerializerCacheControlPolicy::
                 SkipAnyFrameOrResourceMarkedNoStore &&
         hasCacheControlNoStoreHeader ==
             FrameSerializer::HasCacheControlNoStoreHeader;
}

Vector<Attribute> MHTMLFrameSerializerDelegate::getCustomAttributes(
    const Element& element) {
  Vector<Attribute> attributes;

  if (isHTMLImageElement(element)) {
    getCustomAttributesForImageElement(toHTMLImageElement(element),
                                       &attributes);
  } else if (element.isFormControlElement()) {
    getCustomAttributesForFormControlElement(element, &attributes);
  }

  return attributes;
}

void MHTMLFrameSerializerDelegate::getCustomAttributesForImageElement(
    const HTMLImageElement& element,
    Vector<Attribute>* attributes) {
  // Currently only the value of src is pulled into the archive and the srcset
  // attribute is ignored (see shouldIgnoreAttribute() above). If the device
  // has a higher DPR, a different image from srcset could be loaded instead.
  // When this occurs, we should provide the rendering width and height for
  // <img> element if not set.

  // The image should be loaded and participate the layout.
  ImageResourceContent* image = element.cachedImage();
  if (!image || !image->hasImage() || image->errorOccurred() ||
      !element.layoutObject()) {
    return;
  }

  // The width and height attributes should not be set.
  if (element.fastHasAttribute(HTMLNames::widthAttr) ||
      element.fastHasAttribute(HTMLNames::heightAttr)) {
    return;
  }

  // Check if different image is loaded. naturalWidth/naturalHeight will return
  // the image size adjusted with current DPR.
  if (((int)element.naturalWidth()) == image->getImage()->width() &&
      ((int)element.naturalHeight()) == image->getImage()->height()) {
    return;
  }

  Attribute widthAttribute(HTMLNames::widthAttr,
                           AtomicString::number(element.layoutBoxWidth()));
  attributes->push_back(widthAttribute);
  Attribute heightAttribute(HTMLNames::heightAttr,
                            AtomicString::number(element.layoutBoxHeight()));
  attributes->push_back(heightAttribute);
}

void MHTMLFrameSerializerDelegate::getCustomAttributesForFormControlElement(
    const Element& element,
    Vector<Attribute>* attributes) {
  // Disable all form elements in MTHML to tell the user that the form cannot be
  // worked on. MHTML is loaded in full sandboxing mode which disable the form
  // submission and script execution.
  if (element.fastHasAttribute(HTMLNames::disabledAttr))
    return;
  Attribute disabledAttribute(HTMLNames::disabledAttr, "");
  attributes->push_back(disabledAttribute);
}

bool cacheControlNoStoreHeaderPresent(
    const WebLocalFrameImpl& webLocalFrameImpl) {
  const ResourceResponse& response =
      webLocalFrameImpl.dataSource()->response().toResourceResponse();
  if (response.cacheControlContainsNoStore())
    return true;

  const ResourceRequest& request =
      webLocalFrameImpl.dataSource()->getRequest().toResourceRequest();
  return request.cacheControlContainsNoStore();
}

bool frameShouldBeSerializedAsMHTML(
    WebLocalFrame* frame,
    WebFrameSerializerCacheControlPolicy cacheControlPolicy) {
  WebLocalFrameImpl* webLocalFrameImpl = toWebLocalFrameImpl(frame);
  DCHECK(webLocalFrameImpl);

  if (cacheControlPolicy == WebFrameSerializerCacheControlPolicy::None)
    return true;

  bool needToCheckNoStore =
      cacheControlPolicy == WebFrameSerializerCacheControlPolicy::
                                SkipAnyFrameOrResourceMarkedNoStore ||
      (!frame->parent() &&
       cacheControlPolicy ==
           WebFrameSerializerCacheControlPolicy::FailForNoStoreMainFrame);

  if (!needToCheckNoStore)
    return true;

  return !cacheControlNoStoreHeaderPresent(*webLocalFrameImpl);
}

}  // namespace

WebThreadSafeData WebFrameSerializer::generateMHTMLHeader(
    const WebString& boundary,
    WebLocalFrame* frame,
    MHTMLPartsGenerationDelegate* delegate) {
  TRACE_EVENT0("page-serialization", "WebFrameSerializer::generateMHTMLHeader");
  DCHECK(frame);
  DCHECK(delegate);

  if (!frameShouldBeSerializedAsMHTML(frame, delegate->cacheControlPolicy()))
    return WebThreadSafeData();

  WebLocalFrameImpl* webLocalFrameImpl = toWebLocalFrameImpl(frame);
  DCHECK(webLocalFrameImpl);

  Document* document = webLocalFrameImpl->frame()->document();

  RefPtr<RawData> buffer = RawData::create();
  MHTMLArchive::generateMHTMLHeader(boundary, document->title(),
                                    document->suggestedMIMEType(),
                                    *buffer->mutableData());
  return buffer.release();
}

WebThreadSafeData WebFrameSerializer::generateMHTMLParts(
    const WebString& boundary,
    WebLocalFrame* webFrame,
    MHTMLPartsGenerationDelegate* webDelegate) {
  TRACE_EVENT0("page-serialization", "WebFrameSerializer::generateMHTMLParts");
  DCHECK(webFrame);
  DCHECK(webDelegate);

  if (!frameShouldBeSerializedAsMHTML(webFrame,
                                      webDelegate->cacheControlPolicy()))
    return WebThreadSafeData();

  // Translate arguments from public to internal blink APIs.
  LocalFrame* frame = toWebLocalFrameImpl(webFrame)->frame();
  MHTMLArchive::EncodingPolicy encodingPolicy =
      webDelegate->useBinaryEncoding()
          ? MHTMLArchive::EncodingPolicy::UseBinaryEncoding
          : MHTMLArchive::EncodingPolicy::UseDefaultEncoding;

  // Serialize.
  TRACE_EVENT_BEGIN0("page-serialization",
                     "WebFrameSerializer::generateMHTMLParts serializing");
  Deque<SerializedResource> resources;
  {
    SCOPED_BLINK_UMA_HISTOGRAM_TIMER(
        "PageSerialization.MhtmlGeneration.SerializationTime.SingleFrame");
    MHTMLFrameSerializerDelegate coreDelegate(*webDelegate);
    FrameSerializer serializer(resources, coreDelegate);
    serializer.serializeFrame(*frame);
  }

  TRACE_EVENT_END1("page-serialization",
                   "WebFrameSerializer::generateMHTMLParts serializing",
                   "resource count",
                   static_cast<unsigned long long>(resources.size()));

  // There was an error serializing the frame (e.g. of an image resource).
  if (resources.isEmpty())
    return WebThreadSafeData();

  // Encode serialized resources as MHTML.
  RefPtr<RawData> output = RawData::create();
  {
    SCOPED_BLINK_UMA_HISTOGRAM_TIMER(
        "PageSerialization.MhtmlGeneration.EncodingTime.SingleFrame");
    // Frame is the 1st resource (see FrameSerializer::serializeFrame doc
    // comment). Frames get a Content-ID header.
    MHTMLArchive::generateMHTMLPart(
        boundary, webDelegate->getContentID(webFrame), encodingPolicy,
        resources.takeFirst(), *output->mutableData());
    while (!resources.isEmpty()) {
      TRACE_EVENT0("page-serialization",
                   "WebFrameSerializer::generateMHTMLParts encoding");
      MHTMLArchive::generateMHTMLPart(boundary, String(), encodingPolicy,
                                      resources.takeFirst(),
                                      *output->mutableData());
    }
  }
  return output.release();
}

WebThreadSafeData WebFrameSerializer::generateMHTMLFooter(
    const WebString& boundary) {
  TRACE_EVENT0("page-serialization", "WebFrameSerializer::generateMHTMLFooter");
  RefPtr<RawData> buffer = RawData::create();
  MHTMLArchive::generateMHTMLFooter(boundary, *buffer->mutableData());
  return buffer.release();
}

bool WebFrameSerializer::serialize(
    WebLocalFrame* frame,
    WebFrameSerializerClient* client,
    WebFrameSerializer::LinkRewritingDelegate* delegate) {
  WebFrameSerializerImpl serializerImpl(frame, client, delegate);
  return serializerImpl.serialize();
}

WebString WebFrameSerializer::generateMetaCharsetDeclaration(
    const WebString& charset) {
  // TODO(yosin) We should call |FrameSerializer::metaCharsetDeclarationOf()|.
  String charsetString =
      "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=" +
      static_cast<const String&>(charset) + "\">";
  return charsetString;
}

WebString WebFrameSerializer::generateMarkOfTheWebDeclaration(
    const WebURL& url) {
  StringBuilder builder;
  builder.append("\n<!-- ");
  builder.append(FrameSerializer::markOfTheWebDeclaration(url));
  builder.append(" -->\n");
  return builder.toString();
}

WebString WebFrameSerializer::generateBaseTagDeclaration(
    const WebString& baseTarget) {
  // TODO(yosin) We should call |FrameSerializer::baseTagDeclarationOf()|.
  if (baseTarget.isEmpty())
    return String("<base href=\".\">");
  String baseString = "<base href=\".\" target=\"" +
                      static_cast<const String&>(baseTarget) + "\">";
  return baseString;
}

}  // namespace blink
