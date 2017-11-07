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

// TODO(yhirano): Change this to a class.
struct WebURLError {
 public:
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

    // Used for testing.
    kTest,
  };

  enum class HasCopyInCache {
    kFalse,
    kTrue,
  };
  enum class IsWebSecurityViolation {
    kFalse,
    kTrue,
  };

  WebURLError() = default;
  WebURLError(Domain domain, int reason, const WebURL& url)
      : domain_(domain), reason_(reason), url_(url) {}
  WebURLError(Domain domain,
              int reason,
              HasCopyInCache has_copy_in_cache,
              IsWebSecurityViolation is_web_security_violation,
              const WebURL& url)
      : domain_(domain),
        reason_(reason),
        has_copy_in_cache_(has_copy_in_cache == HasCopyInCache::kTrue),
        is_web_security_violation_(is_web_security_violation ==
                                   IsWebSecurityViolation::kTrue),
        url_(url) {}

#if INSIDE_BLINK
  BLINK_PLATFORM_EXPORT WebURLError(const ResourceError&);
  BLINK_PLATFORM_EXPORT WebURLError& operator=(const ResourceError&);
  BLINK_PLATFORM_EXPORT operator ResourceError() const;
#endif

  Domain domain() const { return domain_; }
  int reason() const { return reason_; }
  bool has_copy_in_cache() const { return has_copy_in_cache_; }
  bool is_web_security_violation() const { return is_web_security_violation_; }
  const WebURL& url() const { return url_; }

 private:
  Domain domain_ = Domain::kEmpty;

  // A numeric error code detailing the reason for this error. A value
  // of 0 means no error.
  int reason_ = 0;

  // A flag showing whether or not we have a (possibly stale) copy of the
  // requested resource in the cache.
  bool has_copy_in_cache_ = false;

  // True if this error is created for a web security violation.
  bool is_web_security_violation_ = false;

  // The url that failed to load.
  WebURL url_;
};

BLINK_PLATFORM_EXPORT std::ostream& operator<<(std::ostream&,
                                               WebURLError::Domain);

}  // namespace blink

#endif
