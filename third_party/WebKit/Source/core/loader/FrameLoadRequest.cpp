// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/loader/FrameLoadRequest.h"

#include "platform/loader/fetch/ResourceRequest.h"
#include "platform/wtf/text/AtomicString.h"
#include "public/platform/WebURLRequest.h"

namespace blink {

FrameLoadRequest::FrameLoadRequest(Document* origin_document)
    : FrameLoadRequest(origin_document, ResourceRequest()) {}

FrameLoadRequest::FrameLoadRequest(Document* origin_document,
                                   const ResourceRequest& resource_request)
    : FrameLoadRequest(origin_document, resource_request, AtomicString()) {}

FrameLoadRequest::FrameLoadRequest(Document* origin_document,
                                   const ResourceRequest& resource_request,
                                   const AtomicString& frame_name)
    : FrameLoadRequest(origin_document,
                       resource_request,
                       frame_name,
                       kCheckContentSecurityPolicy) {}

FrameLoadRequest::FrameLoadRequest(Document* origin_document,
                                   const ResourceRequest& resource_request,
                                   const SubstituteData& substitute_data)
    : FrameLoadRequest(origin_document,
                       resource_request,
                       AtomicString(),
                       substitute_data,
                       kCheckContentSecurityPolicy) {}

FrameLoadRequest::FrameLoadRequest(
    Document* origin_document,
    const ResourceRequest& resource_request,
    const AtomicString& frame_name,
    ContentSecurityPolicyDisposition
        should_check_main_world_content_security_policy)
    : FrameLoadRequest(origin_document,
                       resource_request,
                       frame_name,
                       SubstituteData(),
                       should_check_main_world_content_security_policy) {}

FrameLoadRequest::FrameLoadRequest(
    Document* origin_document,
    const ResourceRequest& resource_request,
    const AtomicString& frame_name,
    const SubstituteData& substitute_data,
    ContentSecurityPolicyDisposition
        should_check_main_world_content_security_policy)
    : origin_document_(origin_document),
      resource_request_(resource_request),
      frame_name_(frame_name),
      substitute_data_(substitute_data),
      replaces_current_item_(false),
      client_redirect_(ClientRedirectPolicy::kNotClientRedirect),
      should_send_referrer_(kMaybeSendReferrer),
      should_set_opener_(kMaybeSetOpener),
      should_check_main_world_content_security_policy_(
          should_check_main_world_content_security_policy) {
  // These flags are passed to a service worker which controls the page.
  resource_request_.SetFetchRequestMode(
      network::mojom::FetchRequestMode::kNavigate);
  resource_request_.SetFetchCredentialsMode(
      network::mojom::FetchCredentialsMode::kInclude);
  resource_request_.SetFetchRedirectMode(
      WebURLRequest::kFetchRedirectModeManual);

  if (origin_document) {
    resource_request_.SetRequestorOrigin(
        SecurityOrigin::Create(origin_document->Url()));
    return;
  }

  // If we don't have an origin document, and we're going to throw away the
  // response data regardless, set the requestor to a unique origin.
  if (substitute_data_.IsValid()) {
    resource_request_.SetRequestorOrigin(SecurityOrigin::CreateUnique());
    return;
  }

  // If we're dealing with a top-level request, use the origin of the requested
  // URL as the initiator.
  // TODO(mkwst): This should be `nullptr`. https://crbug.com/625969
  if (resource_request_.GetFrameType() == WebURLRequest::kFrameTypeTopLevel) {
    resource_request_.SetRequestorOrigin(
        SecurityOrigin::Create(resource_request.Url()));
    return;
  }
}

}  // namespace blink
