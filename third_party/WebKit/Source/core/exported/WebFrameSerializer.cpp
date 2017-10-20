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

#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/ElementShadow.h"
#include "core/exported/WebRemoteFrameImpl.h"
#include "core/frame/Frame.h"
#include "core/frame/FrameSerializer.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/RemoteFrame.h"
#include "core/frame/WebFrameSerializerImpl.h"
#include "core/frame/WebLocalFrameImpl.h"
#include "core/html/HTMLAllCollection.h"
#include "core/html/HTMLFrameElementBase.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/html/HTMLHeadElement.h"
#include "core/html/HTMLImageElement.h"
#include "core/html/HTMLTableElement.h"
#include "core/html/forms/HTMLInputElement.h"
#include "core/html_names.h"
#include "core/input_type_names.h"
#include "core/layout/LayoutBox.h"
#include "core/loader/DocumentLoader.h"
#include "platform/Histogram.h"
#include "platform/SerializedResource.h"
#include "platform/SharedBuffer.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "platform/loader/fetch/ResourceRequest.h"
#include "platform/loader/fetch/ResourceResponse.h"
#include "platform/mhtml/MHTMLArchive.h"
#include "platform/mhtml/MHTMLParser.h"
#include "platform/weborigin/KURL.h"
#include "platform/wtf/Assertions.h"
#include "platform/wtf/Deque.h"
#include "platform/wtf/HashMap.h"
#include "platform/wtf/HashSet.h"
#include "platform/wtf/Noncopyable.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/StringConcatenate.h"
#include "public/platform/WebString.h"
#include "public/platform/WebURL.h"
#include "public/platform/WebURLResponse.h"
#include "public/platform/WebVector.h"
#include "public/web/WebDocument.h"
#include "public/web/WebDocumentLoader.h"
#include "public/web/WebFrame.h"
#include "public/web/WebFrameSerializerCacheControlPolicy.h"
#include "public/web/WebFrameSerializerClient.h"

namespace blink {

namespace {

const int kPopupOverlayZIndexThreshold = 50;
const char kShadowModeAttributeName[] = "shadowmode";
const char kShadowDelegatesFocusAttributeName[] = "shadowdelegatesfocus";

class MHTMLFrameSerializerDelegate final : public FrameSerializer::Delegate {
  STACK_ALLOCATED();
  WTF_MAKE_NONCOPYABLE(MHTMLFrameSerializerDelegate);

 public:
  MHTMLFrameSerializerDelegate(
      WebFrameSerializer::MHTMLPartsGenerationDelegate&,
      HeapHashSet<WeakMember<const Element>>&);
  ~MHTMLFrameSerializerDelegate() override;
  bool ShouldIgnoreElement(const Element&) override;
  bool ShouldIgnoreAttribute(const Element&, const Attribute&) override;
  bool RewriteLink(const Element&, String& rewritten_link) override;
  bool ShouldSkipResourceWithURL(const KURL&) override;
  bool ShouldSkipResource(
      FrameSerializer::ResourceHasCacheControlNoStoreHeader) override;
  Vector<Attribute> GetCustomAttributes(const Element&) override;
  std::pair<Node*, Element*> GetAuxiliaryDOMTree(const Element&) const override;
  bool ShouldCollectProblemMetric() override;

 private:
  bool ShouldIgnoreHiddenElement(const Element&);
  bool ShouldIgnoreMetaElement(const Element&);
  bool ShouldIgnorePopupOverlayElement(const Element&);
  void GetCustomAttributesForImageElement(const HTMLImageElement&,
                                          Vector<Attribute>*);

  WebFrameSerializer::MHTMLPartsGenerationDelegate& web_delegate_;
  HeapHashSet<WeakMember<const Element>>& shadow_template_elements_;
  bool popup_overlays_skipped_;
};

MHTMLFrameSerializerDelegate::MHTMLFrameSerializerDelegate(
    WebFrameSerializer::MHTMLPartsGenerationDelegate& web_delegate,
    HeapHashSet<WeakMember<const Element>>& shadow_template_elements)
    : web_delegate_(web_delegate),
      shadow_template_elements_(shadow_template_elements),
      popup_overlays_skipped_(false) {}

MHTMLFrameSerializerDelegate::~MHTMLFrameSerializerDelegate() {
  if (web_delegate_.RemovePopupOverlay()) {
    UMA_HISTOGRAM_BOOLEAN(
        "PageSerialization.MhtmlGeneration.PopupOverlaySkipped",
        popup_overlays_skipped_);
  }
}

bool MHTMLFrameSerializerDelegate::ShouldIgnoreElement(const Element& element) {
  if (ShouldIgnoreHiddenElement(element))
    return true;
  if (ShouldIgnoreMetaElement(element))
    return true;
  if (web_delegate_.RemovePopupOverlay() &&
      ShouldIgnorePopupOverlayElement(element)) {
    return true;
  }
  return false;
}

bool MHTMLFrameSerializerDelegate::ShouldIgnoreHiddenElement(
    const Element& element) {
  // If an iframe is in the head, it will be moved to the body when the page is
  // being loaded. But if an iframe is injected into the head later, it will
  // stay there and not been displayed. To prevent it from being brought to the
  // saved page and cause it being displayed, we should not include it.
  if (IsHTMLIFrameElement(element) &&
      Traversal<HTMLHeadElement>::FirstAncestor(element)) {
    return true;
  }

  // Do not include the element that is marked with hidden attribute.
  if (element.FastHasAttribute(HTMLNames::hiddenAttr))
    return true;

  // Do not include the hidden form element.
  return IsHTMLInputElement(element) &&
         ToHTMLInputElement(&element)->type() == InputTypeNames::hidden;
}

bool MHTMLFrameSerializerDelegate::ShouldIgnoreMetaElement(
    const Element& element) {
  // Do not include meta elements that declare Content-Security-Policy
  // directives. They should have already been enforced when the original
  // document is loaded. Since only the rendered resources are encapsulated in
  // the saved MHTML page, there is no need to carry the directives. If they
  // are still kept in the MHTML, child frames that are referred to using cid:
  // scheme could be prevented from loading.
  if (!IsHTMLMetaElement(element))
    return false;
  if (!element.FastHasAttribute(HTMLNames::contentAttr))
    return false;
  const AtomicString& http_equiv =
      element.FastGetAttribute(HTMLNames::http_equivAttr);
  return http_equiv == "Content-Security-Policy";
}

bool MHTMLFrameSerializerDelegate::ShouldIgnorePopupOverlayElement(
    const Element& element) {
  // The element should be visible.
  LayoutBox* box = element.GetLayoutBox();
  if (!box)
    return false;

  // The bounding box of the element should contain center point of the
  // viewport.
  LocalDOMWindow* window = element.GetDocument().domWindow();
  DCHECK(window);
  LayoutPoint center_point(window->innerWidth() / 2, window->innerHeight() / 2);
  if (!box->FrameRect().Contains(center_point))
    return false;

  // The z-index should be greater than the threshold.
  if (box->Style()->ZIndex() < kPopupOverlayZIndexThreshold)
    return false;

  popup_overlays_skipped_ = true;

  return true;
}

bool MHTMLFrameSerializerDelegate::ShouldIgnoreAttribute(
    const Element& element,
    const Attribute& attribute) {
  // TODO(fgorski): Presence of srcset attribute causes MHTML to not display
  // images, as only the value of src is pulled into the archive. Discarding
  // srcset prevents the problem. Long term we should make sure to MHTML plays
  // nicely with srcset.
  if (attribute.LocalName() == HTMLNames::srcsetAttr)
    return true;

  // Do not save ping attribute since anyway the ping will be blocked from
  // MHTML.
  if (IsHTMLAnchorElement(element) &&
      attribute.LocalName() == HTMLNames::pingAttr) {
    return true;
  }

  // The special attribute in a template element to denote the shadow DOM
  // should only be generated from MHTML serialization. If it is found in the
  // original page, it should be ignored.
  if (IsHTMLTemplateElement(element) &&
      (attribute.LocalName() == kShadowModeAttributeName ||
       attribute.LocalName() == kShadowDelegatesFocusAttributeName) &&
      !shadow_template_elements_.Contains(&element)) {
    return true;
  }

  // If srcdoc attribute for frame elements will be rewritten as src attribute
  // containing link instead of html contents, don't ignore the attribute.
  // Bail out now to avoid the check in Element::isScriptingAttribute.
  bool is_src_doc_attribute = IsHTMLFrameElementBase(element) &&
                              attribute.GetName() == HTMLNames::srcdocAttr;
  String new_link_for_the_element;
  if (is_src_doc_attribute && RewriteLink(element, new_link_for_the_element))
    return false;

  // Do not include attributes that contain javascript. This is because the
  // script will not be executed when a MHTML page is being loaded.
  return element.IsScriptingAttribute(attribute);
}

bool MHTMLFrameSerializerDelegate::RewriteLink(const Element& element,
                                               String& rewritten_link) {
  if (!element.IsFrameOwnerElement())
    return false;

  auto* frame_owner_element = ToHTMLFrameOwnerElement(&element);
  Frame* frame = frame_owner_element->ContentFrame();
  if (!frame)
    return false;

  WebString content_id = web_delegate_.GetContentID(WebFrame::FromFrame(frame));
  if (content_id.IsNull())
    return false;

  KURL cid_uri = MHTMLParser::ConvertContentIDToURI(content_id);
  DCHECK(cid_uri.IsValid());

  if (IsHTMLFrameElementBase(&element)) {
    rewritten_link = cid_uri.GetString();
    return true;
  }

  if (IsHTMLObjectElement(&element)) {
    Document* doc = frame_owner_element->contentDocument();
    bool is_handled_by_serializer = doc->IsHTMLDocument() ||
                                    doc->IsXHTMLDocument() ||
                                    doc->IsImageDocument();
    if (is_handled_by_serializer) {
      rewritten_link = cid_uri.GetString();
      return true;
    }
  }

  return false;
}

bool MHTMLFrameSerializerDelegate::ShouldSkipResourceWithURL(const KURL& url) {
  return web_delegate_.ShouldSkipResource(url);
}

bool MHTMLFrameSerializerDelegate::ShouldSkipResource(
    FrameSerializer::ResourceHasCacheControlNoStoreHeader
        has_cache_control_no_store_header) {
  return web_delegate_.CacheControlPolicy() ==
             WebFrameSerializerCacheControlPolicy::
                 kSkipAnyFrameOrResourceMarkedNoStore &&
         has_cache_control_no_store_header ==
             FrameSerializer::kHasCacheControlNoStoreHeader;
}

Vector<Attribute> MHTMLFrameSerializerDelegate::GetCustomAttributes(
    const Element& element) {
  Vector<Attribute> attributes;

  if (auto* image = ToHTMLImageElementOrNull(element)) {
    GetCustomAttributesForImageElement(*image, &attributes);
  }

  return attributes;
}

bool MHTMLFrameSerializerDelegate::ShouldCollectProblemMetric() {
  return web_delegate_.UsePageProblemDetectors();
}

void MHTMLFrameSerializerDelegate::GetCustomAttributesForImageElement(
    const HTMLImageElement& element,
    Vector<Attribute>* attributes) {
  // Currently only the value of src is pulled into the archive and the srcset
  // attribute is ignored (see shouldIgnoreAttribute() above). If the device
  // has a higher DPR, a different image from srcset could be loaded instead.
  // When this occurs, we should provide the rendering width and height for
  // <img> element if not set.

  // The image should be loaded and participate the layout.
  ImageResourceContent* image = element.CachedImage();
  if (!image || !image->HasImage() || image->ErrorOccurred() ||
      !element.GetLayoutObject()) {
    return;
  }

  // The width and height attributes should not be set.
  if (element.FastHasAttribute(HTMLNames::widthAttr) ||
      element.FastHasAttribute(HTMLNames::heightAttr)) {
    return;
  }

  // Check if different image is loaded. naturalWidth/naturalHeight will return
  // the image size adjusted with current DPR.
  if (((int)element.naturalWidth()) == image->GetImage()->width() &&
      ((int)element.naturalHeight()) == image->GetImage()->height()) {
    return;
  }

  Attribute width_attribute(HTMLNames::widthAttr,
                            AtomicString::Number(element.LayoutBoxWidth()));
  attributes->push_back(width_attribute);
  Attribute height_attribute(HTMLNames::heightAttr,
                             AtomicString::Number(element.LayoutBoxHeight()));
  attributes->push_back(height_attribute);
}

std::pair<Node*, Element*> MHTMLFrameSerializerDelegate::GetAuxiliaryDOMTree(
    const Element& element) const {
  const ElementShadow* shadow = element.Shadow();
  if (!shadow)
    return std::pair<Node*, Element*>();
  ShadowRoot& shadow_root = shadow->OldestShadowRoot();

  String shadow_mode;
  switch (shadow_root.GetType()) {
    case ShadowRootType::kUserAgent:
      // No need to serialize.
      return std::pair<Node*, Element*>();
    case ShadowRootType::V0:
      shadow_mode = "v0";
      break;
    case ShadowRootType::kOpen:
      shadow_mode = "open";
      break;
    case ShadowRootType::kClosed:
      shadow_mode = "closed";
      break;
  }

  // Put the shadow DOM content inside a template element. A special attribute
  // is set to tell the mode of the shadow DOM.
  Element* template_element =
      Element::Create(HTMLNames::templateTag, &(element.GetDocument()));
  template_element->setAttribute(
      QualifiedName(g_null_atom, kShadowModeAttributeName, g_null_atom),
      AtomicString(shadow_mode));
  if (shadow_root.GetType() != ShadowRootType::V0 &&
      shadow_root.delegatesFocus()) {
    template_element->setAttribute(
        QualifiedName(g_null_atom, kShadowDelegatesFocusAttributeName,
                      g_null_atom),
        g_empty_atom);
  }
  shadow_template_elements_.insert(template_element);

  return std::pair<Node*, Element*>(&shadow_root, template_element);
}

bool CacheControlNoStoreHeaderPresent(
    const WebLocalFrameImpl& web_local_frame) {
  const ResourceResponse& response =
      web_local_frame.GetDocumentLoader()->GetResponse().ToResourceResponse();
  if (response.CacheControlContainsNoStore())
    return true;

  const ResourceRequest& request =
      web_local_frame.GetDocumentLoader()->GetRequest().ToResourceRequest();
  return request.CacheControlContainsNoStore();
}

bool FrameShouldBeSerializedAsMHTML(
    WebLocalFrame* frame,
    WebFrameSerializerCacheControlPolicy cache_control_policy) {
  WebLocalFrameImpl* web_local_frame = ToWebLocalFrameImpl(frame);
  DCHECK(web_local_frame);

  if (cache_control_policy == WebFrameSerializerCacheControlPolicy::kNone)
    return true;

  bool need_to_check_no_store =
      cache_control_policy == WebFrameSerializerCacheControlPolicy::
                                  kSkipAnyFrameOrResourceMarkedNoStore ||
      (!frame->Parent() &&
       cache_control_policy ==
           WebFrameSerializerCacheControlPolicy::kFailForNoStoreMainFrame);

  if (!need_to_check_no_store)
    return true;

  return !CacheControlNoStoreHeaderPresent(*web_local_frame);
}

}  // namespace

WebThreadSafeData WebFrameSerializer::GenerateMHTMLHeader(
    const WebString& boundary,
    WebLocalFrame* frame,
    MHTMLPartsGenerationDelegate* delegate) {
  TRACE_EVENT0("page-serialization", "WebFrameSerializer::generateMHTMLHeader");
  DCHECK(frame);
  DCHECK(delegate);

  if (!FrameShouldBeSerializedAsMHTML(frame, delegate->CacheControlPolicy()))
    return WebThreadSafeData();

  WebLocalFrameImpl* web_local_frame = ToWebLocalFrameImpl(frame);
  DCHECK(web_local_frame);

  Document* document = web_local_frame->GetFrame()->GetDocument();

  scoped_refptr<RawData> buffer = RawData::Create();
  MHTMLArchive::GenerateMHTMLHeader(
      boundary, document->Url(), document->title(),
      document->SuggestedMIMEType(), *buffer->MutableData());
  return WebThreadSafeData(buffer);
}

WebThreadSafeData WebFrameSerializer::GenerateMHTMLParts(
    const WebString& boundary,
    WebLocalFrame* web_frame,
    MHTMLPartsGenerationDelegate* web_delegate) {
  TRACE_EVENT0("page-serialization", "WebFrameSerializer::generateMHTMLParts");
  DCHECK(web_frame);
  DCHECK(web_delegate);

  if (!FrameShouldBeSerializedAsMHTML(web_frame,
                                      web_delegate->CacheControlPolicy()))
    return WebThreadSafeData();

  // Translate arguments from public to internal blink APIs.
  LocalFrame* frame = ToWebLocalFrameImpl(web_frame)->GetFrame();
  MHTMLArchive::EncodingPolicy encoding_policy =
      web_delegate->UseBinaryEncoding()
          ? MHTMLArchive::EncodingPolicy::kUseBinaryEncoding
          : MHTMLArchive::EncodingPolicy::kUseDefaultEncoding;

  // Serialize.
  TRACE_EVENT_BEGIN0("page-serialization",
                     "WebFrameSerializer::generateMHTMLParts serializing");
  Deque<SerializedResource> resources;
  {
    SCOPED_BLINK_UMA_HISTOGRAM_TIMER(
        "PageSerialization.MhtmlGeneration.SerializationTime.SingleFrame");
    HeapHashSet<WeakMember<const Element>> shadow_template_elements;
    MHTMLFrameSerializerDelegate core_delegate(*web_delegate,
                                               shadow_template_elements);
    FrameSerializer serializer(resources, core_delegate);
    serializer.SerializeFrame(*frame);
  }

  TRACE_EVENT_END1("page-serialization",
                   "WebFrameSerializer::generateMHTMLParts serializing",
                   "resource count",
                   static_cast<unsigned long long>(resources.size()));

  // There was an error serializing the frame (e.g. of an image resource).
  if (resources.IsEmpty())
    return WebThreadSafeData();

  // Encode serialized resources as MHTML.
  scoped_refptr<RawData> output = RawData::Create();
  {
    SCOPED_BLINK_UMA_HISTOGRAM_TIMER(
        "PageSerialization.MhtmlGeneration.EncodingTime.SingleFrame");
    // Frame is the 1st resource (see FrameSerializer::serializeFrame doc
    // comment). Frames get a Content-ID header.
    MHTMLArchive::GenerateMHTMLPart(
        boundary, web_delegate->GetContentID(web_frame), encoding_policy,
        resources.TakeFirst(), *output->MutableData());
    while (!resources.IsEmpty()) {
      TRACE_EVENT0("page-serialization",
                   "WebFrameSerializer::generateMHTMLParts encoding");
      MHTMLArchive::GenerateMHTMLPart(boundary, String(), encoding_policy,
                                      resources.TakeFirst(),
                                      *output->MutableData());
    }
  }
  return WebThreadSafeData(output);
}

bool WebFrameSerializer::Serialize(
    WebLocalFrame* frame,
    WebFrameSerializerClient* client,
    WebFrameSerializer::LinkRewritingDelegate* delegate) {
  WebFrameSerializerImpl serializer_impl(frame, client, delegate);
  return serializer_impl.Serialize();
}

WebString WebFrameSerializer::GenerateMetaCharsetDeclaration(
    const WebString& charset) {
  // TODO(yosin) We should call |FrameSerializer::metaCharsetDeclarationOf()|.
  String charset_string =
      "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=" +
      static_cast<const String&>(charset) + "\">";
  return charset_string;
}

WebString WebFrameSerializer::GenerateMarkOfTheWebDeclaration(
    const WebURL& url) {
  StringBuilder builder;
  builder.Append("\n<!-- ");
  builder.Append(FrameSerializer::MarkOfTheWebDeclaration(url));
  builder.Append(" -->\n");
  return builder.ToString();
}

WebString WebFrameSerializer::GenerateBaseTagDeclaration(
    const WebString& base_target) {
  // TODO(yosin) We should call |FrameSerializer::baseTagDeclarationOf()|.
  if (base_target.IsEmpty())
    return String("<base href=\".\">");
  String base_string = "<base href=\".\" target=\"" +
                       static_cast<const String&>(base_target) + "\">";
  return base_string;
}

}  // namespace blink
