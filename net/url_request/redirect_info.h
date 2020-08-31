// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_REDIRECT_INFO_H_
#define NET_URL_REQUEST_REDIRECT_INFO_H_

#include <string>

#include "net/base/net_export.h"
#include "net/url_request/url_request.h"
#include "url/gurl.h"

namespace net {

// RedirectInfo captures information about a redirect and any fields in a
// request that change. This struct must be kept in sync with
// content/common/resource_messages.h.
struct NET_EXPORT RedirectInfo {
  RedirectInfo();
  RedirectInfo(const RedirectInfo& other);
  ~RedirectInfo();

  // Computes a new RedirectInfo.
  static RedirectInfo ComputeRedirectInfo(
      // The following arguments |original_*| are the properties of the original
      // request.
      const std::string& original_method,
      const GURL& original_url,
      const SiteForCookies& original_site_for_cookies,
      URLRequest::FirstPartyURLPolicy original_first_party_url_policy,
      URLRequest::ReferrerPolicy original_referrer_policy,
      const std::string& original_referrer,
      // The HTTP status code of the redirect response.
      int http_status_code,
      // The new location URL of the redirect response.
      const GURL& new_location,
      // Referrer-Policy header of the redirect response.
      const base::Optional<std::string>& referrer_policy_header,
      // Whether the URL was upgraded to HTTPS due to upgrade-insecure-requests.
      bool insecure_scheme_was_upgraded,
      // This method copies the URL fragment of the original URL to the new URL
      // by default. Set false only when the network delegate has set the
      // desired redirect URL (with or without fragment), so it must not be
      // changed any more.
      bool copy_fragment = true,
      // Whether the redirect is caused by a failure of signed exchange loading.
      bool is_signed_exchange_fallback_redirect = false);

  // The status code for the redirect response. This is almost redundant with
  // the response headers, but some URLRequestJobs emit redirects without
  // headers.
  int status_code;

  // The new request method. Depending on the response code, the request method
  // may change.
  std::string new_method;

  // The new request URL.
  GURL new_url;

  // The new first-party for cookies.
  SiteForCookies new_site_for_cookies;

  // The new HTTP referrer header.
  std::string new_referrer;

  // True if this redirect was upgraded to HTTPS due to the
  // upgrade-insecure-requests policy.
  bool insecure_scheme_was_upgraded;

  // True if this is a redirect from Signed Exchange to its fallback URL.
  bool is_signed_exchange_fallback_redirect;

  // The new referrer policy that should be obeyed if there are
  // subsequent redirects.
  URLRequest::ReferrerPolicy new_referrer_policy;
};

}  // namespace net

#endif  // NET_URL_REQUEST_REDIRECT_INFO_H_
