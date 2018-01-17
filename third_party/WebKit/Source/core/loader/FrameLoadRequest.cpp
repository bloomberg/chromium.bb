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
                       kCheckContentSecurityPolicy,
                       base::UnguessableToken::Create()) {}

FrameLoadRequest::FrameLoadRequest(Document* origin_document,
                                   const ResourceRequest& resource_request,
                                   const SubstituteData& substitute_data)
    : FrameLoadRequest(origin_document,
                       resource_request,
                       AtomicString(),
                       substitute_data,
                       kCheckContentSecurityPolicy,
                       base::UnguessableToken::Create()) {}

FrameLoadRequest::FrameLoadRequest(
    Document* origin_document,
    const ResourceRequest& resource_request,
    const AtomicString& frame_name,
    ContentSecurityPolicyDisposition
        should_check_main_world_content_security_policy)
    : FrameLoadRequest(origin_document,
                       resource_request,
                       frame_name,
                       should_check_main_world_content_security_policy,
                       base::UnguessableToken::Create()) {}

FrameLoadRequest::FrameLoadRequest(
    Document* origin_document,
    const ResourceRequest& resource_request,
    const AtomicString& frame_name,
    ContentSecurityPolicyDisposition
        should_check_main_world_content_security_policy,
    const base::UnguessableToken& devtools_navigation_token)
    : FrameLoadRequest(origin_document,
                       resource_request,
                       frame_name,
                       SubstituteData(),
                       should_check_main_world_content_security_policy,
                       devtools_navigation_token) {}

FrameLoadRequest::FrameLoadRequest(
    Document* origin_document,
    const ResourceRequest& resource_request,
    const AtomicString& frame_name,
    const SubstituteData& substitute_data,
    ContentSecurityPolicyDisposition
        should_check_main_world_content_security_policy,
    const base::UnguessableToken& devtools_navigation_token)
    : origin_document_(origin_document),
      resource_request_(resource_request),
      frame_name_(frame_name),
      substitute_data_(substitute_data),
      replaces_current_item_(false),
      client_redirect_(ClientRedirectPolicy::kNotClientRedirect),
      should_send_referrer_(kMaybeSendReferrer),
      should_set_opener_(kMaybeSetOpener),
      should_check_main_world_content_security_policy_(
          should_check_main_world_content_security_policy),
      devtools_navigation_token_(devtools_navigation_token) {
  // These flags are passed to a service worker which controls the page.
  resource_request_.SetFetchRequestMode(
      network::mojom::FetchRequestMode::kNavigate);
  resource_request_.SetFetchCredentialsMode(
      network::mojom::FetchCredentialsMode::kInclude);
  resource_request_.SetFetchRedirectMode(
      network::mojom::FetchRedirectMode::kManual);

  if (origin_document) {
    DCHECK(!resource_request_.RequestorOrigin());
    resource_request_.SetRequestorOrigin(
        SecurityOrigin::Create(origin_document->Url()));
  }
}

}  // namespace blink
