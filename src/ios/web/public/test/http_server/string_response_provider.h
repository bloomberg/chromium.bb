// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_HTTP_SERVER_STRING_RESPONSE_PROVIDER_H_
#define IOS_WEB_PUBLIC_TEST_HTTP_SERVER_STRING_RESPONSE_PROVIDER_H_

#include <string>

#include "base/macros.h"
#import "ios/web/public/test/http_server/data_response_provider.h"

namespace web {

// A response provider that returns a  string for all requests. Used for testing
// purposes.
class StringResponseProvider : public web::DataResponseProvider {
 public:
  explicit StringResponseProvider(const std::string& response_body);

  // web::DataResponseProvider methods.
  bool CanHandleRequest(const Request& request) override;
  void GetResponseHeadersAndBody(
      const Request& request,
      scoped_refptr<net::HttpResponseHeaders>* headers,
      std::string* response_body) override;

 private:
  // The string that is returned in the response body.
  std::string response_body_;
  DISALLOW_COPY_AND_ASSIGN(StringResponseProvider);
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_TEST_HTTP_SERVER_STRING_RESPONSE_PROVIDER_H_
