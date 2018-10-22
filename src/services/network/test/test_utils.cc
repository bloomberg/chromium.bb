// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/test/test_utils.h"

#include "base/strings/stringprintf.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/resource_response.h"

namespace network {

std::string GetUploadData(const network::ResourceRequest& request) {
  auto body = request.request_body;
  if (!body || body->elements()->empty())
    return std::string();

  CHECK_EQ(1u, body->elements()->size());
  const auto& element = body->elements()->at(0);
  CHECK_EQ(network::DataElement::TYPE_BYTES, element.type());
  return std::string(element.bytes(), element.length());
}

ResourceResponseHead CreateResourceResponseHead(net::HttpStatusCode http_status,
                                                bool report_raw_headers) {
  ResourceResponseHead head;
  std::string status_line(
      base::StringPrintf("HTTP/1.1 %d %s", static_cast<int>(http_status),
                         net::GetHttpReasonPhrase(http_status)));
  std::string headers = status_line + "\nContent-type: text/html\n\n";
  head.headers = new net::HttpResponseHeaders(
      net::HttpUtil::AssembleRawHeaders(headers.c_str(), headers.size()));
  if (report_raw_headers) {
    head.raw_request_response_info =
        base::MakeRefCounted<HttpRawRequestResponseInfo>();
    head.raw_request_response_info->http_status_text = status_line;
    head.raw_request_response_info->http_status_code = http_status;
  }
  return head;
}

void AddCookiesToResourceResponseHead(const std::vector<std::string>& cookies,
                                      ResourceResponseHead* response_head) {
  for (const auto& cookie_string : cookies) {
    response_head->headers->AddCookie(cookie_string);
    if (response_head->raw_request_response_info) {
      response_head->raw_request_response_info->response_headers.push_back(
          std::make_pair("Set-Cookie", cookie_string));
    }
  }
}

}  // namespace network
