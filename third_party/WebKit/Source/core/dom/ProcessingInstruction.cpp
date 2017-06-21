/*
 * Copyright (C) 2000 Peter Kelly (pmk@post.com)
 * Copyright (C) 2006, 2008, 2009 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "core/dom/ProcessingInstruction.h"

#include <memory>
#include "core/css/CSSStyleSheet.h"
#include "core/css/MediaList.h"
#include "core/css/StyleSheetContents.h"
#include "core/dom/Document.h"
#include "core/dom/IncrementLoadEventDelayCount.h"
#include "core/dom/StyleEngine.h"
#include "core/loader/resource/CSSStyleSheetResource.h"
#include "core/loader/resource/XSLStyleSheetResource.h"
#include "core/xml/DocumentXSLT.h"
#include "core/xml/XSLStyleSheet.h"
#include "core/xml/parser/XMLDocumentParser.h"  // for parseAttributes()
#include "platform/loader/fetch/FetchInitiatorTypeNames.h"
#include "platform/loader/fetch/FetchParameters.h"
#include "platform/loader/fetch/ResourceFetcher.h"
#include "platform/loader/fetch/ResourceLoaderOptions.h"

namespace blink {

inline ProcessingInstruction::ProcessingInstruction(Document& document,
                                                    const String& target,
                                                    const String& data)
    : CharacterData(document, data, kCreateOther),
      target_(target),
      loading_(false),
      alternate_(false),
      is_css_(false),
      is_xsl_(false),
      listener_for_xslt_(nullptr) {}

ProcessingInstruction* ProcessingInstruction::Create(Document& document,
                                                     const String& target,
                                                     const String& data) {
  return new ProcessingInstruction(document, target, data);
}

ProcessingInstruction::~ProcessingInstruction() {}

EventListener* ProcessingInstruction::EventListenerForXSLT() {
  if (!listener_for_xslt_)
    return 0;

  return listener_for_xslt_->ToEventListener();
}

void ProcessingInstruction::ClearEventListenerForXSLT() {
  if (listener_for_xslt_) {
    listener_for_xslt_->Detach();
    listener_for_xslt_.Clear();
  }
}

String ProcessingInstruction::nodeName() const {
  return target_;
}

Node::NodeType ProcessingInstruction::getNodeType() const {
  return kProcessingInstructionNode;
}

Node* ProcessingInstruction::cloneNode(bool /*deep*/, ExceptionState&) {
  // FIXME: Is it a problem that this does not copy m_localHref?
  // What about other data members?
  return Create(GetDocument(), target_, data_);
}

void ProcessingInstruction::DidAttributeChanged() {
  if (sheet_)
    ClearSheet();

  String href;
  String charset;
  if (!CheckStyleSheet(href, charset))
    return;
  Process(href, charset);
}

bool ProcessingInstruction::CheckStyleSheet(String& href, String& charset) {
  if (target_ != "xml-stylesheet" || !GetDocument().GetFrame() ||
      parentNode() != GetDocument())
    return false;

  // see http://www.w3.org/TR/xml-stylesheet/
  // ### support stylesheet included in a fragment of this (or another) document
  // ### make sure this gets called when adding from javascript
  bool attrs_ok;
  const HashMap<String, String> attrs = ParseAttributes(data_, attrs_ok);
  if (!attrs_ok)
    return false;
  HashMap<String, String>::const_iterator i = attrs.find("type");
  String type;
  if (i != attrs.end())
    type = i->value;

  is_css_ = type.IsEmpty() || type == "text/css";
  is_xsl_ = (type == "text/xml" || type == "text/xsl" ||
             type == "application/xml" || type == "application/xhtml+xml" ||
             type == "application/rss+xml" || type == "application/atom+xml");
  if (!is_css_ && !is_xsl_)
    return false;

  href = attrs.at("href");
  charset = attrs.at("charset");
  String alternate = attrs.at("alternate");
  alternate_ = alternate == "yes";
  title_ = attrs.at("title");
  media_ = attrs.at("media");

  return !alternate_ || !title_.IsEmpty();
}

void ProcessingInstruction::Process(const String& href, const String& charset) {
  if (href.length() > 1 && href[0] == '#') {
    local_href_ = href.Substring(1);
    // We need to make a synthetic XSLStyleSheet that is embedded.
    // It needs to be able to kick off import/include loads that
    // can hang off some parent sheet.
    if (is_xsl_ && RuntimeEnabledFeatures::XSLTEnabled()) {
      KURL final_url(kParsedURLString, local_href_);
      sheet_ = XSLStyleSheet::CreateEmbedded(this, final_url);
      loading_ = false;
    }
    return;
  }

  ClearResource();

  String url = GetDocument().CompleteURL(href).GetString();

  StyleSheetResource* resource = nullptr;
  ResourceLoaderOptions options;
  options.initiator_info.name = FetchInitiatorTypeNames::processinginstruction;
  FetchParameters params(ResourceRequest(GetDocument().CompleteURL(href)),
                         options);
  if (is_xsl_) {
    if (RuntimeEnabledFeatures::XSLTEnabled())
      resource = XSLStyleSheetResource::Fetch(params, GetDocument().Fetcher());
  } else {
    params.SetCharset(charset.IsEmpty() ? GetDocument().Encoding()
                                        : WTF::TextEncoding(charset));
    resource = CSSStyleSheetResource::Fetch(params, GetDocument().Fetcher());
  }

  if (resource) {
    loading_ = true;
    if (!is_xsl_)
      GetDocument().GetStyleEngine().AddPendingSheet(style_engine_context_);
    SetResource(resource);
  }
}

bool ProcessingInstruction::IsLoading() const {
  if (loading_)
    return true;
  if (!sheet_)
    return false;
  return sheet_->IsLoading();
}

bool ProcessingInstruction::SheetLoaded() {
  if (!IsLoading()) {
    if (!DocumentXSLT::SheetLoaded(GetDocument(), this))
      GetDocument().GetStyleEngine().RemovePendingSheet(*this,
                                                        style_engine_context_);
    return true;
  }
  return false;
}

void ProcessingInstruction::SetCSSStyleSheet(
    const String& href,
    const KURL& base_url,
    ReferrerPolicy referrer_policy,
    const WTF::TextEncoding& charset,
    const CSSStyleSheetResource* sheet) {
  if (!isConnected()) {
    DCHECK(!sheet_);
    return;
  }

  DCHECK(is_css_);
  CSSParserContext* parser_context = CSSParserContext::Create(
      GetDocument(), base_url, referrer_policy, charset);

  StyleSheetContents* new_sheet =
      StyleSheetContents::Create(href, parser_context);

  CSSStyleSheet* css_sheet = CSSStyleSheet::Create(new_sheet, *this);
  css_sheet->setDisabled(alternate_);
  css_sheet->SetTitle(title_);
  if (!alternate_ && !title_.IsEmpty())
    GetDocument().GetStyleEngine().SetPreferredStylesheetSetNameIfNotSet(
        title_);
  css_sheet->SetMediaQueries(MediaQuerySet::Create(media_));

  sheet_ = css_sheet;

  // We don't need the cross-origin security check here because we are
  // getting the sheet text in "strict" mode. This enforces a valid CSS MIME
  // type.
  ParseStyleSheet(sheet->SheetText());
}

void ProcessingInstruction::SetXSLStyleSheet(const String& href,
                                             const KURL& base_url,
                                             const String& sheet) {
  if (!isConnected()) {
    DCHECK(!sheet_);
    return;
  }

  DCHECK(is_xsl_);
  sheet_ = XSLStyleSheet::Create(this, href, base_url);
  std::unique_ptr<IncrementLoadEventDelayCount> delay =
      IncrementLoadEventDelayCount::Create(GetDocument());
  ParseStyleSheet(sheet);
}

void ProcessingInstruction::ParseStyleSheet(const String& sheet) {
  if (is_css_)
    ToCSSStyleSheet(sheet_.Get())->Contents()->ParseString(sheet);
  else if (is_xsl_)
    ToXSLStyleSheet(sheet_.Get())->ParseString(sheet);

  ClearResource();
  loading_ = false;

  if (is_css_)
    ToCSSStyleSheet(sheet_.Get())->Contents()->CheckLoaded();
  else if (is_xsl_)
    ToXSLStyleSheet(sheet_.Get())->CheckLoaded();
}

Node::InsertionNotificationRequest ProcessingInstruction::InsertedInto(
    ContainerNode* insertion_point) {
  CharacterData::InsertedInto(insertion_point);
  if (!insertion_point->isConnected())
    return kInsertionDone;

  String href;
  String charset;
  bool is_valid = CheckStyleSheet(href, charset);
  if (!DocumentXSLT::ProcessingInstructionInsertedIntoDocument(GetDocument(),
                                                               this))
    GetDocument().GetStyleEngine().AddStyleSheetCandidateNode(*this);
  if (is_valid)
    Process(href, charset);
  return kInsertionDone;
}

void ProcessingInstruction::RemovedFrom(ContainerNode* insertion_point) {
  CharacterData::RemovedFrom(insertion_point);
  if (!insertion_point->isConnected())
    return;

  // No need to remove XSLStyleSheet from StyleEngine.
  if (!DocumentXSLT::ProcessingInstructionRemovedFromDocument(GetDocument(),
                                                              this)) {
    GetDocument().GetStyleEngine().RemoveStyleSheetCandidateNode(
        *this, *insertion_point);
  }

  if (sheet_) {
    DCHECK_EQ(sheet_->ownerNode(), this);
    ClearSheet();
  }

  // No need to remove pending sheets.
  ClearResource();
}

void ProcessingInstruction::ClearSheet() {
  DCHECK(sheet_);
  if (sheet_->IsLoading())
    GetDocument().GetStyleEngine().RemovePendingSheet(*this,
                                                      style_engine_context_);
  sheet_.Release()->ClearOwnerNode();
}

DEFINE_TRACE(ProcessingInstruction) {
  visitor->Trace(sheet_);
  visitor->Trace(listener_for_xslt_);
  CharacterData::Trace(visitor);
  ResourceOwner<StyleSheetResource>::Trace(visitor);
}

}  // namespace blink
