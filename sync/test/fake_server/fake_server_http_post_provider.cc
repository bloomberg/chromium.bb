// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/test/fake_server/fake_server_http_post_provider.h"

#include <string>

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "sync/test/fake_server/fake_server.h"

using syncer::HttpPostProviderInterface;

namespace fake_server {

FakeServerHttpPostProviderFactory::FakeServerHttpPostProviderFactory(
    FakeServer* fake_server,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
        : fake_server_(fake_server), task_runner_(task_runner) { }

FakeServerHttpPostProviderFactory::~FakeServerHttpPostProviderFactory() { }

void FakeServerHttpPostProviderFactory::Init(const std::string& user_agent) { }

HttpPostProviderInterface* FakeServerHttpPostProviderFactory::Create() {
  FakeServerHttpPostProvider* http =
      new FakeServerHttpPostProvider(fake_server_, task_runner_);
  http->AddRef();
  return http;
}

void FakeServerHttpPostProviderFactory::Destroy(
    HttpPostProviderInterface* http) {
  static_cast<FakeServerHttpPostProvider*>(http)->Release();
}

FakeServerHttpPostProvider::FakeServerHttpPostProvider(
    FakeServer* fake_server,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
        : fake_server_(fake_server),
          task_runner_(task_runner),
          post_complete_(false, false) { }

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

void FakeServerHttpPostProvider::OnPostComplete(int error_code,
                                                int response_code,
                                                const std::string& response) {
  post_error_code_ = error_code;
  post_response_code_ = response_code;
  response_ = response;
  post_complete_.Signal();
}

bool FakeServerHttpPostProvider::MakeSynchronousPost(int* error_code,
                                                     int* response_code) {
  // It is assumed that a POST is being made to /command.
  FakeServer::HandleCommandCallback callback = base::Bind(
      &FakeServerHttpPostProvider::OnPostComplete, base::Unretained(this));
  task_runner_->PostNonNestableTask(FROM_HERE,
                                    base::Bind(&FakeServer::HandleCommand,
                                               base::Unretained(fake_server_),
                                               base::ConstRef(request_content_),
                                               base::ConstRef(callback)));
  post_complete_.Wait();
  *error_code = post_error_code_;
  *response_code = post_response_code_;
  return *error_code == 0;
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

}  // namespace fake_server
