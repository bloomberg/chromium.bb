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

#ifndef WebURLError_h
#define WebURLError_h

#include "WebString.h"
#include "WebURL.h"

namespace blink {

class ResourceError;

struct WebURLError {
  // A namespace for "reason" to support various layers generating resource
  // errors.
  enum class Domain {
    // |reason| should be always zero. An error with this domain is considered
    // as an empty error (== "no error").
    // TODO(yhirano): Consider removing this domain.
    kEmpty,

    // The error is a "net" error. |reason| is an error code specified in
    // net/base/net_error_list.h.
    kNet,

    // |reason| is an HTTP status code.
    kHttp,

    // |reason| is a DNS probe status.
    kDnsProbe,

    // Used for testing.
    kTest,
  };
  Domain domain = Domain::kEmpty;

  // A numeric error code detailing the reason for this error. A value
  // of 0 means no error.
  int reason = 0;

  // A flag showing whether or not "unreachableURL" has a copy in the
  // cache that was too stale to return for this request.
  bool stale_copy_in_cache = false;

  // True if this error is created for a web security violation.
  bool is_web_security_violation = false;

  // The url that failed to load.
  WebURL unreachable_url;

  // A description for the error.
  WebString localized_description;

  WebURLError() = default;
  // This constructor infers some members from the parameters.
  BLINK_PLATFORM_EXPORT WebURLError(const WebURL&,
                                    bool stale_copy_in_cache,
                                    int reason);

#if INSIDE_BLINK
  BLINK_PLATFORM_EXPORT WebURLError(const ResourceError&);
  BLINK_PLATFORM_EXPORT WebURLError& operator=(const ResourceError&);
  BLINK_PLATFORM_EXPORT operator ResourceError() const;
#endif
};

BLINK_PLATFORM_EXPORT std::ostream& operator<<(std::ostream&,
                                               WebURLError::Domain);

}  // namespace blink

#endif
