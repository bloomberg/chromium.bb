// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/platform/WebURLError.h"

#include "net/base/net_errors.h"

namespace blink {

WebURLError::WebURLError(int reason, const WebURL& url)
    : reason_(reason), url_(url) {
  DCHECK_NE(reason_, 0);
}

WebURLError::WebURLError(int reason,
                         HasCopyInCache has_copy_in_cache,
                         IsWebSecurityViolation is_web_security_violation,
                         const WebURL& url)
    : reason_(reason),
      has_copy_in_cache_(has_copy_in_cache == HasCopyInCache::kTrue),
      is_web_security_violation_(is_web_security_violation ==
                                 IsWebSecurityViolation::kTrue),
      url_(url) {
  DCHECK_NE(reason_, 0);
}

WebURLError::WebURLError(const network::CORSErrorStatus& cors_error_status,
                         HasCopyInCache has_copy_in_cache,
                         const WebURL& url)
    : reason_(net::ERR_FAILED),
      has_copy_in_cache_(has_copy_in_cache == HasCopyInCache::kTrue),
      is_web_security_violation_(true),
      url_(url),
      cors_error_status_(cors_error_status) {}

}  // namespace blink
