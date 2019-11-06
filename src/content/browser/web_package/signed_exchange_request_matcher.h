// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_REQUEST_MATCHER_H_
#define CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_REQUEST_MATCHER_H_

#include <map>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "content/browser/web_package/http_structured_header.h"
#include "content/common/content_export.h"
#include "net/http/http_request_headers.h"

namespace content {

// SignedExchangeRequestMatcher implements the Request Matching algorithm [1].
// [1] https://wicg.github.io/webpackage/loading.html#request-matching
class CONTENT_EXPORT SignedExchangeRequestMatcher {
 public:
  // Keys must be lower-cased.
  using HeaderMap = std::map<std::string, std::string>;

  // |request_headers| is the request headers of `browserRequest` in [1]. If
  // it does not have an Accept-Language header, languages in |accept_langs|
  // are used for matching.
  // |accept_langs| is a comma separated ordered list of language codes.
  SignedExchangeRequestMatcher(const net::HttpRequestHeaders& request_headers,
                               const std::string& accept_langs);
  bool MatchRequest(const HeaderMap& response_headers) const;

 private:
  net::HttpRequestHeaders request_headers_;

  static bool MatchRequest(const net::HttpRequestHeaders& request_headers,
                           const HeaderMap& response_headers);
  static std::vector<std::vector<std::string>> CacheBehavior(
      const http_structured_header::ListOfLists& variants,
      const net::HttpRequestHeaders& request_headers);
  FRIEND_TEST_ALL_PREFIXES(SignedExchangeRequestMatcherTest, MatchRequest);
  FRIEND_TEST_ALL_PREFIXES(SignedExchangeRequestMatcherTest, CacheBehavior);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_REQUEST_MATCHER_H_
