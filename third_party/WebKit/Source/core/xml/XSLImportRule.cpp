/*
 * This file is part of the XSL implementation.
 *
 * Copyright (C) 2004, 2005, 2006, 2008 Apple Inc. All rights reserved.
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

#include "core/xml/XSLImportRule.h"

#include "core/dom/Document.h"
#include "core/loader/resource/XSLStyleSheetResource.h"
#include "platform/SharedBuffer.h"
#include "platform/loader/fetch/FetchParameters.h"
#include "platform/loader/fetch/RawResource.h"
#include "platform/loader/fetch/ResourceFetcher.h"
#include "platform/loader/fetch/ResourceLoaderOptions.h"
#include "platform/loader/fetch/fetch_initiator_type_names.h"
#include "platform/wtf/text/TextEncoding.h"

namespace blink {

XSLImportRule::XSLImportRule(XSLStyleSheet* parent, const String& href)
    : parent_style_sheet_(parent), str_href_(href), loading_(false) {}

XSLImportRule::~XSLImportRule() {}

void XSLImportRule::SetXSLStyleSheet(const String& href,
                                     const KURL& base_url,
                                     const String& sheet) {
  if (style_sheet_)
    style_sheet_->SetParentStyleSheet(nullptr);

  style_sheet_ = XSLStyleSheet::Create(this, href, base_url);

  XSLStyleSheet* parent = ParentStyleSheet();
  if (parent)
    style_sheet_->SetParentStyleSheet(parent);

  style_sheet_->ParseString(sheet);
  loading_ = false;

  if (parent)
    parent->CheckLoaded();
}

bool XSLImportRule::IsLoading() {
  return loading_ || (style_sheet_ && style_sheet_->IsLoading());
}

void XSLImportRule::LoadSheet() {
  Document* owner_document = nullptr;
  XSLStyleSheet* root_sheet = ParentStyleSheet();

  if (root_sheet) {
    while (XSLStyleSheet* parent_sheet = root_sheet->parentStyleSheet())
      root_sheet = parent_sheet;
  }

  if (root_sheet)
    owner_document = root_sheet->OwnerDocument();

  String abs_href = str_href_;
  XSLStyleSheet* parent_sheet = ParentStyleSheet();
  if (!parent_sheet->BaseURL().IsNull()) {
    // Use parent styleheet's URL as the base URL
    abs_href = KURL(parent_sheet->BaseURL(), str_href_).GetString();
  }

  // Check for a cycle in our import chain. If we encounter a stylesheet in
  // our parent chain with the same URL, then just bail.
  for (XSLStyleSheet* parent_sheet = ParentStyleSheet(); parent_sheet;
       parent_sheet = parent_sheet->parentStyleSheet()) {
    if (abs_href == parent_sheet->BaseURL().GetString())
      return;
  }

  ResourceLoaderOptions fetch_options;
  fetch_options.initiator_info.name = FetchInitiatorTypeNames::xml;
  FetchParameters params(ResourceRequest(owner_document->CompleteURL(abs_href)),
                         fetch_options);
  params.SetOriginRestriction(FetchParameters::kRestrictToSameOrigin);
  XSLStyleSheetResource* resource = XSLStyleSheetResource::FetchSynchronously(
      params, owner_document->Fetcher());
  if (!resource || !resource->Sheet())
    return;

  DCHECK(!style_sheet_);
  SetXSLStyleSheet(abs_href, resource->GetResponse().Url(), resource->Sheet());
}

void XSLImportRule::Trace(blink::Visitor* visitor) {
  visitor->Trace(parent_style_sheet_);
  visitor->Trace(style_sheet_);
}

}  // namespace blink
