// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/test/fake_server/fake_server_http_post_provider.h"

#include <string>

#include "sync/test/fake_server/fake_server.h"

namespace syncer {

FakeServerHttpPostProviderFactory::FakeServerHttpPostProviderFactory(
    FakeServer* fake_server) : fake_server_(fake_server) { }

FakeServerHttpPostProviderFactory::~FakeServerHttpPostProviderFactory() { }

void FakeServerHttpPostProviderFactory::Init(const std::string& user_agent) { }

HttpPostProviderInterface* FakeServerHttpPostProviderFactory::Create() {
  FakeServerHttpPostProvider* http =
      new FakeServerHttpPostProvider(fake_server_);
  http->AddRef();
  return http;
}

void FakeServerHttpPostProviderFactory::Destroy(
    HttpPostProviderInterface* http) {
  static_cast<FakeServerHttpPostProvider*>(http)->Release();
}

FakeServerHttpPostProvider::FakeServerHttpPostProvider(
    FakeServer* fake_server) : fake_server_(fake_server) { }

FakeServerHttpPostProvider::~FakeServerHttpPostProvider() { }

void FakeServerHttpPostProvider::SetExtraRequestHeaders(const char* headers) {
  // TODO(pvalenzuela): Add assertions on this value.
  extra_request_headers_.assign(headers);
}

void FakeServerHttpPostProvider::SetURL(const char* url, int port) {
  // TODO(pvalenzuela): Add assertions on these values.
  request_url_.assign(url);
  request_port_ = port;
}

void FakeServerHttpPostProvider::SetPostPayload(const char* content_type,
                                                int content_length,
                                                const char* content) {
  request_content_type_.assign(content_type);
  request_content_.assign(content, content_length);
}

bool FakeServerHttpPostProvider::MakeSynchronousPost(int* error_code,
                                                     int* response_code) {
  // This assumes that a POST is being made to /command.
  *error_code = fake_server_->HandleCommand(request_content_,
                                            response_code,
                                            &response_);
  return (*error_code == 0);
}

int FakeServerHttpPostProvider::GetResponseContentLength() const {
  return response_.length();
}

const char* FakeServerHttpPostProvider::GetResponseContent() const {
  return response_.c_str();
}

const std::string FakeServerHttpPostProvider::GetResponseHeaderValue(
    const std::string& name) const {
  return std::string();
}

void FakeServerHttpPostProvider::Abort() {
}

}  // namespace syncer
