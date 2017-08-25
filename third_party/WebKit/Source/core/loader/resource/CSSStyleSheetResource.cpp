/*
    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller (mueller@kde.org)
    Copyright (C) 2002 Waldo Bastian (bastian@kde.org)
    Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
    Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.

    This class provides all functionality needed for loading images, style
    sheets and html pages from the web. It has a memory cache for these objects.
*/

#include "core/loader/resource/CSSStyleSheetResource.h"

#include "core/css/StyleSheetContents.h"
#include "core/loader/resource/StyleSheetResourceClient.h"
#include "platform/HTTPNames.h"
#include "platform/SharedBuffer.h"
#include "platform/loader/fetch/FetchParameters.h"
#include "platform/loader/fetch/MemoryCache.h"
#include "platform/loader/fetch/ResourceClientWalker.h"
#include "platform/loader/fetch/ResourceFetcher.h"
#include "platform/loader/fetch/ResourceLoaderOptions.h"
#include "platform/loader/fetch/TextResourceDecoderOptions.h"
#include "platform/weborigin/SecurityPolicy.h"
#include "platform/wtf/CurrentTime.h"
#include "platform/wtf/text/TextEncoding.h"

namespace blink {

CSSStyleSheetResource* CSSStyleSheetResource::Fetch(FetchParameters& params,
                                                    ResourceFetcher* fetcher) {
  DCHECK_EQ(params.GetResourceRequest().GetFrameType(),
            WebURLRequest::kFrameTypeNone);
  params.SetRequestContext(WebURLRequest::kRequestContextStyle);
  CSSStyleSheetResource* resource = ToCSSStyleSheetResource(
      fetcher->RequestResource(params, CSSStyleSheetResourceFactory()));
  // TODO(kouhei): Dedupe this logic w/ ScriptResource::fetch
  if (resource && !params.IntegrityMetadata().IsEmpty())
    resource->SetIntegrityMetadata(params.IntegrityMetadata());
  return resource;
}

CSSStyleSheetResource* CSSStyleSheetResource::CreateForTest(
    const KURL& url,
    const WTF::TextEncoding& encoding) {
  ResourceRequest request(url);
  request.SetFetchCredentialsMode(WebURLRequest::kFetchCredentialsModeOmit);
  ResourceLoaderOptions options;
  TextResourceDecoderOptions decoder_options(
      TextResourceDecoderOptions::kCSSContent, encoding);
  return new CSSStyleSheetResource(request, options, decoder_options);
}

CSSStyleSheetResource::CSSStyleSheetResource(
    const ResourceRequest& resource_request,
    const ResourceLoaderOptions& options,
    const TextResourceDecoderOptions& decoder_options)
    : StyleSheetResource(resource_request,
                         kCSSStyleSheet,
                         options,
                         decoder_options),
      did_notify_first_data_(false) {}

CSSStyleSheetResource::~CSSStyleSheetResource() {}

void CSSStyleSheetResource::SetParsedStyleSheetCache(
    StyleSheetContents* new_sheet) {
  if (parsed_style_sheet_cache_)
    parsed_style_sheet_cache_->ClearReferencedFromResource();
  parsed_style_sheet_cache_ = new_sheet;
  if (parsed_style_sheet_cache_)
    parsed_style_sheet_cache_->SetReferencedFromResource(this);

  // Updates the decoded size to take parsed stylesheet cache into account.
  UpdateDecodedSize();
}

DEFINE_TRACE(CSSStyleSheetResource) {
  visitor->Trace(parsed_style_sheet_cache_);
  StyleSheetResource::Trace(visitor);
}

void CSSStyleSheetResource::DidAddClient(ResourceClient* c) {
  DCHECK(StyleSheetResourceClient::IsExpectedType(c));
  // Resource::didAddClient() must be before setCSSStyleSheet(), because
  // setCSSStyleSheet() may cause scripts to be executed, which could destroy
  // 'c' if it is an instance of HTMLLinkElement. see the comment of
  // HTMLLinkElement::setCSSStyleSheet.
  Resource::DidAddClient(c);

  if (HasClient(c) && did_notify_first_data_)
    static_cast<StyleSheetResourceClient*>(c)->DidAppendFirstData(this);

  // |c| might be removed in didAppendFirstData, so ensure it is still a client.
  if (HasClient(c) && !IsLoading()) {
    ReferrerPolicy referrer_policy = kReferrerPolicyDefault;
    String referrer_policy_header =
        GetResponse().HttpHeaderField(HTTPNames::Referrer_Policy);
    if (!referrer_policy_header.IsNull()) {
      SecurityPolicy::ReferrerPolicyFromHeaderValue(
          referrer_policy_header, kDoNotSupportReferrerPolicyLegacyKeywords,
          &referrer_policy);
    }
    static_cast<StyleSheetResourceClient*>(c)->SetCSSStyleSheet(
        GetResourceRequest().Url(), GetResponse().Url(), referrer_policy,
        Encoding(), this);
  }
}

const String CSSStyleSheetResource::SheetText(
    MIMETypeCheck mime_type_check) const {
  if (!CanUseSheet(mime_type_check))
    return String();

  // Use cached decoded sheet text when available
  if (!decoded_sheet_text_.IsNull()) {
    // We should have the decoded sheet text cached when the resource is fully
    // loaded.
    DCHECK_EQ(GetStatus(), ResourceStatus::kCached);

    return decoded_sheet_text_;
  }

  if (!Data() || Data()->IsEmpty())
    return String();

  return DecodedText();
}

void CSSStyleSheetResource::AppendData(const char* data, size_t length) {
  Resource::AppendData(data, length);
  if (did_notify_first_data_)
    return;
  ResourceClientWalker<StyleSheetResourceClient> w(Clients());
  while (StyleSheetResourceClient* c = w.Next())
    c->DidAppendFirstData(this);
  did_notify_first_data_ = true;
}

void CSSStyleSheetResource::NotifyFinished() {
  TriggerNotificationForFinishObservers();

  // Decode the data to find out the encoding and cache the decoded sheet text.
  if (Data())
    SetDecodedSheetText(DecodedText());

  ReferrerPolicy referrer_policy = kReferrerPolicyDefault;
  String referrer_policy_header =
      GetResponse().HttpHeaderField(HTTPNames::Referrer_Policy);
  if (!referrer_policy_header.IsNull()) {
    SecurityPolicy::ReferrerPolicyFromHeaderValue(
        referrer_policy_header, kDoNotSupportReferrerPolicyLegacyKeywords,
        &referrer_policy);
  }

  ResourceClientWalker<StyleSheetResourceClient> w(Clients());
  while (StyleSheetResourceClient* c = w.Next()) {
    MarkClientFinished(c);
    c->SetCSSStyleSheet(GetResourceRequest().Url(), GetResponse().Url(),
                        referrer_policy, Encoding(), this);
  }

  // Clear raw bytes as now we have the full decoded sheet text.
  // We wait for all LinkStyle::setCSSStyleSheet to run (at least once)
  // as SubresourceIntegrity checks require raw bytes.
  // Note that LinkStyle::setCSSStyleSheet can be called from didAddClient too,
  // but is safe as we should have a cached ResourceIntegrityDisposition.
  ClearData();
}

void CSSStyleSheetResource::DestroyDecodedDataIfPossible() {
  if (!parsed_style_sheet_cache_)
    return;

  SetParsedStyleSheetCache(nullptr);
}

void CSSStyleSheetResource::DestroyDecodedDataForFailedRevalidation() {
  SetDecodedSheetText(String());
  DestroyDecodedDataIfPossible();
}

bool CSSStyleSheetResource::CanUseSheet(MIMETypeCheck mime_type_check) const {
  if (ErrorOccurred())
    return false;

  // This check exactly matches Firefox. Note that we grab the Content-Type
  // header directly because we want to see what the value is BEFORE content
  // sniffing. Firefox does this by setting a "type hint" on the channel. This
  // implementation should be observationally equivalent.
  //
  // This code defaults to allowing the stylesheet for non-HTTP protocols so
  // folks can use standards mode for local HTML documents.
  if (mime_type_check == MIMETypeCheck::kLax)
    return true;
  AtomicString content_type = HttpContentType();
  return content_type.IsEmpty() ||
         DeprecatedEqualIgnoringCase(content_type, "text/css") ||
         DeprecatedEqualIgnoringCase(content_type,
                                     "application/x-unknown-content-type");
}

StyleSheetContents* CSSStyleSheetResource::RestoreParsedStyleSheet(
    const CSSParserContext* context) {
  if (!parsed_style_sheet_cache_)
    return nullptr;
  if (parsed_style_sheet_cache_->HasFailedOrCanceledSubresources()) {
    SetParsedStyleSheetCache(nullptr);
    return nullptr;
  }

  DCHECK(parsed_style_sheet_cache_->IsCacheableForResource());
  DCHECK(parsed_style_sheet_cache_->IsReferencedFromResource());

  // Contexts must be identical so we know we would get the same exact result if
  // we parsed again.
  if (*parsed_style_sheet_cache_->ParserContext() != *context)
    return nullptr;

  return parsed_style_sheet_cache_;
}

void CSSStyleSheetResource::SaveParsedStyleSheet(StyleSheetContents* sheet) {
  DCHECK(sheet);
  DCHECK(sheet->IsCacheableForResource());

  if (!GetMemoryCache()->Contains(this)) {
    // This stylesheet resource did conflict with another resource and was not
    // added to the cache.
    SetParsedStyleSheetCache(nullptr);
    return;
  }
  SetParsedStyleSheetCache(sheet);
}

void CSSStyleSheetResource::SetDecodedSheetText(
    const String& decoded_sheet_text) {
  decoded_sheet_text_ = decoded_sheet_text;
  UpdateDecodedSize();
}

void CSSStyleSheetResource::UpdateDecodedSize() {
  size_t decoded_size = decoded_sheet_text_.CharactersSizeInBytes();
  if (parsed_style_sheet_cache_)
    decoded_size += parsed_style_sheet_cache_->EstimatedSizeInBytes();
  SetDecodedSize(decoded_size);
}

}  // namespace blink
