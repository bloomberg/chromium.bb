// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/parser/PreloadRequest.h"

#include "core/dom/Document.h"
#include "core/loader/DocumentLoader.h"
#include "platform/CrossOriginAttributeValue.h"
#include "platform/loader/fetch/FetchInitiatorInfo.h"
#include "platform/loader/fetch/FetchRequest.h"
#include "platform/loader/fetch/ResourceFetcher.h"
#include "platform/weborigin/SecurityPolicy.h"

namespace blink {

bool PreloadRequest::IsSafeToSendToAnotherThread() const {
  return initiator_name_.IsSafeToSendToAnotherThread() &&
         charset_.IsSafeToSendToAnotherThread() &&
         resource_url_.IsSafeToSendToAnotherThread() &&
         base_url_.IsSafeToSendToAnotherThread();
}

KURL PreloadRequest::CompleteURL(Document* document) {
  if (!base_url_.IsEmpty())
    return document->CompleteURLWithOverride(resource_url_, base_url_);
  return document->CompleteURL(resource_url_);
}

Resource* PreloadRequest::Start(Document* document) {
  DCHECK(IsMainThread());

  FetchInitiatorInfo initiator_info;
  initiator_info.name = AtomicString(initiator_name_);
  initiator_info.position = initiator_position_;

  const KURL& url = CompleteURL(document);
  // Data URLs are filtered out in the preload scanner.
  DCHECK(!url.ProtocolIsData());

  ResourceRequest resource_request(url);
  resource_request.SetHTTPReferrer(SecurityPolicy::GenerateReferrer(
      referrer_policy_, url,
      resource_type_ == Resource::kCSSStyleSheet
          ? base_url_.StrippedForUseAsReferrer()
          : document->OutgoingReferrer()));
  resource_request.SetRequestContext(
      ResourceFetcher::DetermineRequestContext(resource_type_, false));

  FetchRequest request(resource_request, initiator_info);

  if (resource_type_ == Resource::kImportResource) {
    SecurityOrigin* security_origin =
        document->ContextDocument()->GetSecurityOrigin();
    request.SetCrossOriginAccessControl(security_origin,
                                        kCrossOriginAttributeAnonymous);
  }

  if (cross_origin_ != kCrossOriginAttributeNotSet) {
    request.SetCrossOriginAccessControl(document->GetSecurityOrigin(),
                                        cross_origin_);
  }

  request.SetDefer(defer_);
  request.SetResourceWidth(resource_width_);
  request.GetClientHintsPreferences().UpdateFrom(client_hints_preferences_);
  request.SetIntegrityMetadata(integrity_metadata_);
  request.SetContentSecurityPolicyNonce(nonce_);
  request.SetParserDisposition(kParserInserted);

  if (request_type_ == kRequestTypeLinkRelPreload)
    request.SetLinkPreload(true);

  if (resource_type_ == Resource::kScript ||
      resource_type_ == Resource::kCSSStyleSheet ||
      resource_type_ == Resource::kImportResource) {
    request.SetCharset(charset_.IsEmpty() ? document->characterSet().GetString()
                                          : charset_);
  }
  request.SetSpeculativePreload(true, discovery_time_);

  return document->Loader()->StartPreload(resource_type_, request);
}

}  // namespace blink
