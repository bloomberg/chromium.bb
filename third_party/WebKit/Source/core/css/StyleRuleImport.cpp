/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * (C) 2002-2003 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2002, 2005, 2006, 2008, 2009, 2010, 2012 Apple Inc. All rights
 * reserved.
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

#include "core/css/StyleRuleImport.h"

#include "core/css/StyleSheetContents.h"
#include "core/dom/Document.h"
#include "core/loader/resource/CSSStyleSheetResource.h"
#include "platform/loader/fetch/FetchParameters.h"
#include "platform/loader/fetch/ResourceFetcher.h"
#include "platform/loader/fetch/ResourceLoaderOptions.h"
#include "platform/loader/fetch/fetch_initiator_type_names.h"

namespace blink {

StyleRuleImport* StyleRuleImport::Create(const String& href,
                                         scoped_refptr<MediaQuerySet> media) {
  return new StyleRuleImport(href, media);
}

StyleRuleImport::StyleRuleImport(const String& href,
                                 scoped_refptr<MediaQuerySet> media)
    : StyleRuleBase(kImport),
      parent_style_sheet_(nullptr),
      style_sheet_client_(new ImportedStyleSheetClient(this)),
      str_href_(href),
      media_queries_(media),
      loading_(false) {
  if (!media_queries_)
    media_queries_ = MediaQuerySet::Create(String());
}

StyleRuleImport::~StyleRuleImport() {}

void StyleRuleImport::Dispose() {
  if (resource_)
    resource_->RemoveClient(style_sheet_client_);
  resource_ = nullptr;
}

void StyleRuleImport::TraceAfterDispatch(blink::Visitor* visitor) {
  visitor->Trace(style_sheet_client_);
  visitor->Trace(parent_style_sheet_);
  visitor->Trace(style_sheet_);
  visitor->Trace(resource_);
  StyleRuleBase::TraceAfterDispatch(visitor);
}

void StyleRuleImport::SetCSSStyleSheet(
    const String& href,
    const KURL& base_url,
    ReferrerPolicy referrer_policy,
    const WTF::TextEncoding& charset,
    const CSSStyleSheetResource* cached_style_sheet) {
  if (style_sheet_)
    style_sheet_->ClearOwnerRule();

  Document* document = nullptr;
  const CSSParserContext* context = StrictCSSParserContext();
  if (parent_style_sheet_) {
    document = parent_style_sheet_->SingleOwnerDocument();
    context = parent_style_sheet_->ParserContext();
  }
  context = CSSParserContext::Create(context, base_url, referrer_policy,
                                     charset, document);

  style_sheet_ = StyleSheetContents::Create(this, href, context);

  style_sheet_->ParseAuthorStyleSheet(
      cached_style_sheet, document ? document->GetSecurityOrigin() : nullptr);

  loading_ = false;

  if (parent_style_sheet_) {
    parent_style_sheet_->NotifyLoadedSheet(cached_style_sheet);
    parent_style_sheet_->CheckLoaded();
  }
}

bool StyleRuleImport::IsLoading() const {
  return loading_ || (style_sheet_ && style_sheet_->IsLoading());
}

void StyleRuleImport::RequestStyleSheet() {
  if (!parent_style_sheet_)
    return;
  Document* document = parent_style_sheet_->SingleOwnerDocument();
  if (!document)
    return;

  ResourceFetcher* fetcher = document->Fetcher();
  if (!fetcher)
    return;

  KURL abs_url;
  if (!parent_style_sheet_->BaseURL().IsNull()) {
    // use parent styleheet's URL as the base URL
    abs_url = KURL(parent_style_sheet_->BaseURL(), str_href_);
  } else {
    abs_url = document->CompleteURL(str_href_);
  }

  // Check for a cycle in our import chain.  If we encounter a stylesheet
  // in our parent chain with the same URL, then just bail.
  StyleSheetContents* root_sheet = parent_style_sheet_;
  for (StyleSheetContents* sheet = parent_style_sheet_; sheet;
       sheet = sheet->ParentStyleSheet()) {
    if (EqualIgnoringFragmentIdentifier(abs_url, sheet->BaseURL()) ||
        EqualIgnoringFragmentIdentifier(
            abs_url, document->CompleteURL(sheet->OriginalURL())))
      return;
    root_sheet = sheet;
  }

  ResourceLoaderOptions options;
  options.initiator_info.name = FetchInitiatorTypeNames::css;
  FetchParameters params(ResourceRequest(abs_url), options);
  params.SetCharset(parent_style_sheet_->Charset());
  resource_ = CSSStyleSheetResource::Fetch(params, fetcher);
  if (resource_) {
    // if the import rule is issued dynamically, the sheet may be
    // removed from the pending sheet count, so let the doc know
    // the sheet being imported is pending.
    if (parent_style_sheet_ && parent_style_sheet_->LoadCompleted() &&
        root_sheet == parent_style_sheet_)
      parent_style_sheet_->StartLoadingDynamicSheet();
    loading_ = true;
    resource_->AddClient(style_sheet_client_);
  }
}

}  // namespace blink
