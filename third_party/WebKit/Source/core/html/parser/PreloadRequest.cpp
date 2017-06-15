// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/parser/PreloadRequest.h"

#include "core/dom/Document.h"
#include "core/dom/DocumentWriteIntervention.h"
#include "core/loader/DocumentLoader.h"
#include "platform/CrossOriginAttributeValue.h"
#include "platform/loader/fetch/FetchInitiatorInfo.h"
#include "platform/loader/fetch/FetchParameters.h"
#include "platform/loader/fetch/ResourceFetcher.h"
#include "platform/loader/fetch/ResourceLoaderOptions.h"
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
      referrer_source_ == kBaseUrlIsReferrer
          ? base_url_.StrippedForUseAsReferrer()
          : document->OutgoingReferrer()));
  resource_request.SetRequestContext(ResourceFetcher::DetermineRequestContext(
      resource_type_, is_image_set_, false));

  if (resource_type_ == Resource::kScript)
    MaybeDisallowFetchForDocWrittenScript(resource_request, defer_, *document);

  ResourceLoaderOptions options(kAllowStoredCredentials,
                                kClientRequestedCredentials);
  options.initiator_info = initiator_info;
  FetchParameters params(resource_request, options);

  if (resource_type_ == Resource::kImportResource) {
    SecurityOrigin* security_origin =
        document->ContextDocument()->GetSecurityOrigin();
    params.SetCrossOriginAccessControl(security_origin,
                                       kCrossOriginAttributeAnonymous);
  }

  if (cross_origin_ != kCrossOriginAttributeNotSet) {
    params.SetCrossOriginAccessControl(document->GetSecurityOrigin(),
                                       cross_origin_);
  }

  params.SetDefer(defer_);
  params.SetResourceWidth(resource_width_);
  params.GetClientHintsPreferences().UpdateFrom(client_hints_preferences_);
  params.SetIntegrityMetadata(integrity_metadata_);
  params.SetContentSecurityPolicyNonce(nonce_);
  params.SetParserDisposition(kParserInserted);

  if (request_type_ == kRequestTypeLinkRelPreload)
    params.SetLinkPreload(true);

  if (resource_type_ == Resource::kScript ||
      resource_type_ == Resource::kCSSStyleSheet ||
      resource_type_ == Resource::kImportResource) {
    params.SetCharset(charset_.IsEmpty() ? document->Encoding()
                                         : WTF::TextEncoding(charset_));
  }
  FetchParameters::SpeculativePreloadType speculative_preload_type =
      FetchParameters::SpeculativePreloadType::kInDocument;
  if (from_insertion_scanner_) {
    speculative_preload_type =
        FetchParameters::SpeculativePreloadType::kInserted;
  }
  params.SetSpeculativePreloadType(speculative_preload_type, discovery_time_);

  return document->Loader()->StartPreload(resource_type_, params);
}

}  // namespace blink
