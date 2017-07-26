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

#ifndef ContentSecurityPolicyResponseHeaders_h
#define ContentSecurityPolicyResponseHeaders_h

#include "platform/CrossThreadCopier.h"
#include "platform/PlatformExport.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class ResourceResponse;
class HTTPHeaderMap;

class PLATFORM_EXPORT ContentSecurityPolicyResponseHeaders final {
 public:
  ContentSecurityPolicyResponseHeaders() {}
  explicit ContentSecurityPolicyResponseHeaders(const ResourceResponse&);
  explicit ContentSecurityPolicyResponseHeaders(const HTTPHeaderMap&);

  const String& ContentSecurityPolicy() const {
    return content_security_policy_;
  }
  const String& ContentSecurityPolicyReportOnly() const {
    return content_security_policy_report_only_;
  }

  ContentSecurityPolicyResponseHeaders IsolatedCopy() const;

 private:
  String content_security_policy_;
  String content_security_policy_report_only_;
};

template <>
struct CrossThreadCopier<ContentSecurityPolicyResponseHeaders> {
  STATIC_ONLY(CrossThreadCopier);
  using Type = ContentSecurityPolicyResponseHeaders;
  PLATFORM_EXPORT static Type Copy(
      const ContentSecurityPolicyResponseHeaders& headers) {
    return headers.IsolatedCopy();
  }
};

}  // namespace blink

#endif
