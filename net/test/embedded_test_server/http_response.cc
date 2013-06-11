// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/embedded_test_server/http_response.h"

#include "base/format_macros.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"

namespace net {
namespace test_server {

HttpResponse::~HttpResponse() {
}

BasicHttpResponse::BasicHttpResponse() : code_(SUCCESS) {
}

BasicHttpResponse::~BasicHttpResponse() {
}

std::string BasicHttpResponse::ToResponseString() const {
  // Response line with headers.
  std::string response_builder;

  // TODO(mtomasz): For http/1.0 requests, send http/1.0.
  // TODO(mtomasz): For different codes, send a corrent string instead of OK.
  base::StringAppendF(&response_builder, "HTTP/1.1 %d OK\r\n", code_);
  base::StringAppendF(&response_builder, "Connection: closed\r\n");
  base::StringAppendF(&response_builder,
                      "Content-Length: %"PRIuS"\r\n",
                      content_.size());
  base::StringAppendF(&response_builder,
                      "Content-Type: %s\r\n",
                      content_type_.c_str());
  for (std::map<std::string, std::string>::const_iterator it =
           custom_headers_.begin();
       it != custom_headers_.end();
       ++it) {
    // Multi-line header value support.
    const std::string& header_name = it->first;
    const std::string& header_value = it->second;
    DCHECK(header_value.find_first_of("\n\r") == std::string::npos) <<
        "Malformed header value.";
    base::StringAppendF(&response_builder,
                        "%s: %s\r\n",
                        header_name.c_str(),
                        header_value.c_str());
  }
  base::StringAppendF(&response_builder, "\r\n");

  return response_builder + content_;
}

}  // namespace test_server
}  // namespace net
