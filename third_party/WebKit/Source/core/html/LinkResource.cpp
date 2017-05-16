/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/html/LinkResource.h"

#include "core/HTMLNames.h"
#include "core/dom/Document.h"
#include "core/html/HTMLLinkElement.h"
#include "core/html/imports/HTMLImportsController.h"
#include "platform/weborigin/SecurityPolicy.h"

namespace blink {

using namespace HTMLNames;

LinkResource::LinkResource(HTMLLinkElement* owner) : owner_(owner) {}

LinkResource::~LinkResource() {}

bool LinkResource::ShouldLoadResource() const {
  return owner_->GetDocument().GetFrame() ||
         owner_->GetDocument().ImportsController();
}

LocalFrame* LinkResource::LoadingFrame() const {
  HTMLImportsController* imports_controller =
      owner_->GetDocument().ImportsController();
  if (!imports_controller)
    return owner_->GetDocument().GetFrame();
  return imports_controller->Master()->GetFrame();
}

DEFINE_TRACE(LinkResource) {
  visitor->Trace(owner_);
}

LinkRequestBuilder::LinkRequestBuilder(HTMLLinkElement* owner)
    : owner_(owner), url_(owner->GetNonEmptyURLAttribute(hrefAttr)) {
  charset_ = owner_->getAttribute(charsetAttr);
  if (charset_.IsEmpty() && owner_->GetDocument().GetFrame())
    charset_ = owner_->GetDocument().characterSet();
}

FetchParameters LinkRequestBuilder::Build(bool low_priority) const {
  ResourceRequest resource_request(owner_->GetDocument().CompleteURL(url_));
  ReferrerPolicy referrer_policy = owner_->GetReferrerPolicy();
  if (referrer_policy != kReferrerPolicyDefault) {
    resource_request.SetHTTPReferrer(SecurityPolicy::GenerateReferrer(
        referrer_policy, url_, owner_->GetDocument().OutgoingReferrer()));
  }
  FetchParameters params(resource_request, owner_->localName(), charset_);
  if (low_priority)
    params.SetDefer(FetchParameters::kLazyLoad);
  params.SetContentSecurityPolicyNonce(owner_->nonce());
  return params;
}

}  // namespace blink
