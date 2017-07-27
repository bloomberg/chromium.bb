/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#include "public/platform/WebURLError.h"

#include "platform/loader/fetch/ResourceError.h"
#include "platform/weborigin/KURL.h"

namespace blink {

WebURLError::WebURLError(const WebURL& unreachable_url,
                         bool stale_copy_in_cache,
                         int reason) {
  // This is implemented in ResourceError.h due to a DEPS restriction.
  ResourceError::InitializeWebURLError(this, unreachable_url,
                                       stale_copy_in_cache, reason);
}

WebURLError::WebURLError(const ResourceError& error) {
  *this = error;
}

WebURLError& WebURLError::operator=(const ResourceError& error) {
  if (error.IsNull()) {
    *this = WebURLError();
  } else {
    domain = error.GetDomain();
    reason = error.ErrorCode();
    unreachable_url = KURL(kParsedURLString, error.FailingURL());
    stale_copy_in_cache = error.StaleCopyInCache();
    localized_description = error.LocalizedDescription();
    was_ignored_by_handler = error.WasIgnoredByHandler();
    is_web_security_violation = error.IsAccessCheck();
  }
  return *this;
}

WebURLError::operator ResourceError() const {
  if (!reason)
    return ResourceError();
  ResourceError resource_error =
      ResourceError(domain, reason, unreachable_url, localized_description);
  resource_error.SetStaleCopyInCache(stale_copy_in_cache);
  resource_error.SetWasIgnoredByHandler(was_ignored_by_handler);
  resource_error.SetIsAccessCheck(is_web_security_violation);
  return resource_error;
}

std::ostream& operator<<(std::ostream& out, const WebURLError::Domain domain) {
  switch (domain) {
    case WebURLError::Domain::kEmpty:
      out << "(null)";
      break;
    case WebURLError::Domain::kNet:
      out << "net";
      break;
    case WebURLError::Domain::kBlinkInternal:
      out << "blink";
      break;
    case WebURLError::Domain::kHttp:
      out << "http";
      break;
    case WebURLError::Domain::kDnsProbe:
      out << "dns probe";
      break;
    case WebURLError::Domain::kTest:
      out << "testing";
      break;
  }
  return out;
}

}  // namespace blink
