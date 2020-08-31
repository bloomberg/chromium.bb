// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/error_page/common/error.h"

#include "net/base/net_errors.h"

namespace error_page {

const char Error::kNetErrorDomain[] = "net";
const char Error::kHttpErrorDomain[] = "http";
const char Error::kDnsProbeErrorDomain[] = "dnsprobe";

Error Error::NetError(const GURL& url,
                      int reason,
                      net::ResolveErrorInfo resolve_error_info,
                      bool stale_copy_in_cache) {
  return Error(url, kNetErrorDomain, reason, std::move(resolve_error_info),
               stale_copy_in_cache);
}

Error Error::HttpError(const GURL& url, int http_status_code) {
  return Error(url, kHttpErrorDomain, http_status_code,
               net::ResolveErrorInfo(net::OK), false);
}

Error Error::DnsProbeError(const GURL& url,
                           int status,
                           bool stale_copy_in_cache) {
  return Error(url, kDnsProbeErrorDomain, status,
               net::ResolveErrorInfo(net::OK), stale_copy_in_cache);
}

Error::Error(const GURL& url,
             const std::string& domain,
             int reason,
             net::ResolveErrorInfo resolve_error_info,
             bool stale_copy_in_cache)
    : url_(url),
      domain_(domain),
      reason_(reason),
      resolve_error_info_(std::move(resolve_error_info)),
      stale_copy_in_cache_(stale_copy_in_cache) {}

}  // namespace error_page
