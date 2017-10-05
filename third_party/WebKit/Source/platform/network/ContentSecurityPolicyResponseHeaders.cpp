/*
 * Copyright (C) 2013 Google, Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
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

#include "platform/network/ContentSecurityPolicyResponseHeaders.h"

#include "platform/http_names.h"
#include "platform/loader/fetch/ResourceResponse.h"
#include "platform/network/HTTPHeaderMap.h"

namespace blink {

ContentSecurityPolicyResponseHeaders::ContentSecurityPolicyResponseHeaders(
    const ResourceResponse& response)
    : ContentSecurityPolicyResponseHeaders(response.HttpHeaderFields()) {}

ContentSecurityPolicyResponseHeaders::ContentSecurityPolicyResponseHeaders(
    const HTTPHeaderMap& headers)
    : content_security_policy_(headers.Get(HTTPNames::Content_Security_Policy)),
      content_security_policy_report_only_(
          headers.Get(HTTPNames::Content_Security_Policy_Report_Only)) {}

ContentSecurityPolicyResponseHeaders
ContentSecurityPolicyResponseHeaders::IsolatedCopy() const {
  ContentSecurityPolicyResponseHeaders headers;
  headers.content_security_policy_ = content_security_policy_.IsolatedCopy();
  headers.content_security_policy_report_only_ =
      content_security_policy_report_only_.IsolatedCopy();
  return headers;
}
}
