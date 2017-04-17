// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/loader/BaseFetchContext.h"

#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/inspector/ConsoleMessage.h"
#include "platform/weborigin/SecurityPolicy.h"

namespace blink {

BaseFetchContext::BaseFetchContext(ExecutionContext* context)
    : execution_context_(context) {}

void BaseFetchContext::AddAdditionalRequestHeaders(ResourceRequest& request,
                                                   FetchResourceType type) {
  bool is_main_resource = type == kFetchMainResource;
  if (!is_main_resource) {
    if (!request.DidSetHTTPReferrer()) {
      DCHECK(execution_context_);
      request.SetHTTPReferrer(SecurityPolicy::GenerateReferrer(
          execution_context_->GetReferrerPolicy(), request.Url(),
          execution_context_->OutgoingReferrer()));
      request.AddHTTPOriginIfNeeded(execution_context_->GetSecurityOrigin());
    } else {
      DCHECK_EQ(SecurityPolicy::GenerateReferrer(request.GetReferrerPolicy(),
                                                 request.Url(),
                                                 request.HttpReferrer())
                    .referrer,
                request.HttpReferrer());
      request.AddHTTPOriginIfNeeded(request.HttpReferrer());
    }
  }

  if (execution_context_) {
    request.SetExternalRequestStateFromRequestorAddressSpace(
        execution_context_->GetSecurityContext().AddressSpace());
  }
}

SecurityOrigin* BaseFetchContext::GetSecurityOrigin() const {
  return execution_context_ ? execution_context_->GetSecurityOrigin() : nullptr;
}

void BaseFetchContext::PrintAccessDeniedMessage(const KURL& url) const {
  if (url.IsNull())
    return;

  DCHECK(execution_context_);
  String message;
  if (execution_context_->Url().IsNull()) {
    message = "Unsafe attempt to load URL " + url.ElidedString() + '.';
  } else if (url.IsLocalFile() || execution_context_->Url().IsLocalFile()) {
    message = "Unsafe attempt to load URL " + url.ElidedString() +
              " from frame with URL " +
              execution_context_->Url().ElidedString() +
              ". 'file:' URLs are treated as unique security origins.\n";
  } else {
    message = "Unsafe attempt to load URL " + url.ElidedString() +
              " from frame with URL " +
              execution_context_->Url().ElidedString() +
              ". Domains, protocols and ports must match.\n";
  }

  execution_context_->AddConsoleMessage(ConsoleMessage::Create(
      kSecurityMessageSource, kErrorMessageLevel, message));
}

void BaseFetchContext::AddCSPHeaderIfNecessary(Resource::Type type,
                                               ResourceRequest& request) {
  if (!execution_context_)
    return;

  const ContentSecurityPolicy* csp =
      execution_context_->GetContentSecurityPolicy();
  if (csp->ShouldSendCSPHeader(type))
    request.AddHTTPHeaderField("CSP", "active");
}

DEFINE_TRACE(BaseFetchContext) {
  visitor->Trace(execution_context_);
  FetchContext::Trace(visitor);
}

}  // namespace blink
