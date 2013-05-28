// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TEST_EMBEDDED_TEST_SERVER_HTTP_RESPONSE_H_
#define NET_TEST_EMBEDDED_TEST_SERVER_HTTP_RESPONSE_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"

namespace net {
namespace test_server {

enum ResponseCode {
  SUCCESS = 200,
  CREATED = 201,
  NO_CONTENT = 204,
  MOVED = 301,
  RESUME_INCOMPLETE = 308,
  NOT_FOUND = 404,
  PRECONDITION = 412,
  ACCESS_DENIED = 500,
};

// Interface for HTTP response implementations.
class HttpResponse{
 public:
  virtual ~HttpResponse();

  // Returns raw contents to be written to the network socket
  // in response. If you intend to make this a valid HTTP response,
  // it should start with "HTTP/x.x" line, followed by response headers.
  virtual std::string ToResponseString() const = 0;
};

// This class is used to handle basic HTTP responses with commonly used
// response headers such as "Content-Type".
class BasicHttpResponse : public HttpResponse {
 public:
  BasicHttpResponse();
  virtual ~BasicHttpResponse();

  // The response code.
  ResponseCode code() const { return code_; }
  void set_code(ResponseCode code) { code_ = code; }

  // The content of the response.
  const std::string& content() const { return content_; }
  void set_content(const std::string& content) { content_ = content; }

  // The content type.
  const std::string& content_type() const { return content_type_; }
  void set_content_type(const std::string& content_type) {
    content_type_ = content_type;
  }

  // The custom headers to be sent to the client.
  const std::map<std::string, std::string>& custom_headers() const {
    return custom_headers_;
  }

  // Adds a custom header.
  void AddCustomHeader(const std::string& key, const std::string& value) {
    custom_headers_[key] = value;
  }

  // Generates and returns a http response string.
  virtual std::string ToResponseString() const OVERRIDE;

 private:
  ResponseCode code_;
  std::string content_;
  std::string content_type_;
  std::map<std::string, std::string> custom_headers_;

  DISALLOW_COPY_AND_ASSIGN(BasicHttpResponse);
};

}  // namespace test_server
}  // namespace net

#endif  // NET_TEST_EMBEDDED_TEST_SERVER_HTTP_RESPONSE_H_
