/*
 * Copyright (C) 2003, 2006, 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_FRAME_LOAD_REQUEST_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_FRAME_LOAD_REQUEST_H_

#include "services/network/public/mojom/request_context_frame_type.mojom-shared.h"
#include "third_party/blink/public/mojom/blob/blob_url_store.mojom-blink.h"
#include "third_party/blink/public/web/web_triggering_event_info.h"
#include "third_party/blink/public/web/web_window_features.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/frame_types.h"
#include "third_party/blink/renderer/core/loader/frame_loader_types.h"
#include "third_party/blink/renderer/core/loader/navigation_policy.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"

namespace blink {

class HTMLFormElement;

struct CORE_EXPORT FrameLoadRequest {
  STACK_ALLOCATED();

 public:
  FrameLoadRequest(Document* origin_document, const ResourceRequest&);
  FrameLoadRequest(Document* origin_document,
                   const ResourceRequest&,
                   const AtomicString& frame_name);

  Document* OriginDocument() const { return origin_document_.Get(); }

  network::mojom::RequestContextFrameType GetFrameType() const {
    return frame_type_;
  }
  void SetFrameType(network::mojom::RequestContextFrameType frame_type) {
    frame_type_ = frame_type;
  }

  ResourceRequest& GetResourceRequest() { return resource_request_; }
  const ResourceRequest& GetResourceRequest() const {
    return resource_request_;
  }

  const AtomicString& FrameName() const { return frame_name_; }
  void ClearFrameName() { frame_name_ = AtomicString(); }

  ClientRedirectPolicy ClientRedirect() const { return client_redirect_; }

  void SetClientRedirectReason(ClientNavigationReason reason) {
    client_redirect_ = ClientRedirectPolicy::kClientRedirect;
    client_navigation_reason_ = reason;
  }

  ClientNavigationReason ClientRedirectReason() const {
    DCHECK_EQ(ClientRedirectPolicy::kClientRedirect, client_redirect_);
    return client_navigation_reason_;
  }

  NavigationPolicy GetNavigationPolicy() const { return navigation_policy_; }
  void SetNavigationPolicy(NavigationPolicy navigation_policy) {
    navigation_policy_ = navigation_policy;
  }

  WebTriggeringEventInfo TriggeringEventInfo() const {
    return triggering_event_info_;
  }
  void SetTriggeringEventInfo(WebTriggeringEventInfo info) {
    DCHECK(info != WebTriggeringEventInfo::kUnknown);
    triggering_event_info_ = info;
  }

  HTMLFormElement* Form() const { return form_.Get(); }
  void SetForm(HTMLFormElement* form) { form_ = form; }

  ShouldSendReferrer GetShouldSendReferrer() const {
    return should_send_referrer_;
  }
  void SetShouldSendReferrer(ShouldSendReferrer should_send_referrer) {
    should_send_referrer_ = should_send_referrer;
  }

  const AtomicString& HrefTranslate() const { return href_translate_; }
  void SetHrefTranslate(const AtomicString& translate) {
    href_translate_ = translate;
  }

  ContentSecurityPolicyDisposition ShouldCheckMainWorldContentSecurityPolicy()
      const {
    return should_check_main_world_content_security_policy_;
  }

  // The BlobURLToken that should be used when fetching the resource. This
  // is needed for blob URLs, because the blob URL might be revoked before the
  // actual fetch happens, which would result in incorrect failures to fetch.
  // The token lets the browser process securely resolves the blob URL even
  // after the url has been revoked.
  mojom::blink::BlobURLTokenPtr GetBlobURLToken() const {
    if (!blob_url_token_)
      return nullptr;
    mojom::blink::BlobURLTokenPtr result;
    blob_url_token_->data->Clone(MakeRequest(&result));
    return result;
  }

  void SetInputStartTime(base::TimeTicks input_start_time) {
    input_start_time_ = input_start_time;
  }

  base::TimeTicks GetInputStartTime() const { return input_start_time_; }

  const WebWindowFeatures& GetWindowFeatures() const {
    return window_features_;
  }
  void SetFeaturesForWindowOpen(const WebWindowFeatures& features) {
    window_features_ = features;
    is_window_open_ = true;
  }
  bool IsWindowOpen() const { return is_window_open_; }

  void SetNoOpener() { window_features_.noopener = true; }

 private:
  Member<Document> origin_document_;
  ResourceRequest resource_request_;
  AtomicString frame_name_;
  AtomicString href_translate_;
  // TODO(caseq): merge ClientRedirectPolicy and ClientNavigationReason.
  // Currently, client_navigation_reason_ is set iff ClientRedirectPolicy
  // is set to kClientRedirect.
  ClientRedirectPolicy client_redirect_;
  ClientNavigationReason client_navigation_reason_;
  NavigationPolicy navigation_policy_ = kNavigationPolicyCurrentTab;
  WebTriggeringEventInfo triggering_event_info_ =
      WebTriggeringEventInfo::kNotFromEvent;
  Member<HTMLFormElement> form_;
  ShouldSendReferrer should_send_referrer_;
  ContentSecurityPolicyDisposition
      should_check_main_world_content_security_policy_;
  scoped_refptr<base::RefCountedData<mojom::blink::BlobURLTokenPtr>>
      blob_url_token_;
  base::TimeTicks input_start_time_;
  network::mojom::RequestContextFrameType frame_type_ =
      network::mojom::RequestContextFrameType::kNone;
  WebWindowFeatures window_features_;
  bool is_window_open_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_FRAME_LOAD_REQUEST_H_
