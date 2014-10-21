// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_TEST_FAKE_SERVER_FAKE_SERVER_HTTP_POST_PROVIDER_H_
#define SYNC_TEST_FAKE_SERVER_FAKE_SERVER_HTTP_POST_PROVIDER_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "sync/internal_api/public/http_post_provider_factory.h"
#include "sync/internal_api/public/http_post_provider_interface.h"

namespace fake_server {

class FakeServer;

class FakeServerHttpPostProvider
    : public syncer::HttpPostProviderInterface,
      public base::RefCountedThreadSafe<FakeServerHttpPostProvider> {
 public:
  FakeServerHttpPostProvider(
      FakeServer* fake_server,
      scoped_refptr<base::SequencedTaskRunner> task_runner);

  // HttpPostProviderInterface implementation.
  void SetExtraRequestHeaders(const char* headers) override;
  void SetURL(const char* url, int port) override;
  void SetPostPayload(const char* content_type,
                      int content_length,
                      const char* content) override;
  bool MakeSynchronousPost(int* error_code, int* response_code) override;
  void Abort() override;
  int GetResponseContentLength() const override;
  const char* GetResponseContent() const override;
  const std::string GetResponseHeaderValue(
      const std::string& name) const override;

 protected:
  friend class base::RefCountedThreadSafe<FakeServerHttpPostProvider>;
  ~FakeServerHttpPostProvider() override;

 private:
  void OnPostComplete(int error_code,
                      int response_code,
                      const std::string& response);

  FakeServer* const fake_server_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  std::string response_;
  std::string request_url_;
  int request_port_;
  std::string request_content_;
  std::string request_content_type_;
  std::string extra_request_headers_;
  int post_error_code_;
  int post_response_code_;
  base::WaitableEvent post_complete_;

  DISALLOW_COPY_AND_ASSIGN(FakeServerHttpPostProvider);
};

class FakeServerHttpPostProviderFactory
    : public syncer::HttpPostProviderFactory {
 public:
  FakeServerHttpPostProviderFactory(
      FakeServer* fake_server,
      scoped_refptr<base::SequencedTaskRunner> task_runner);
  ~FakeServerHttpPostProviderFactory() override;

  // HttpPostProviderFactory:
  void Init(const std::string& user_agent) override;
  syncer::HttpPostProviderInterface* Create() override;
  void Destroy(syncer::HttpPostProviderInterface* http) override;

 private:
  FakeServer* const fake_server_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(FakeServerHttpPostProviderFactory);
};

}  //  namespace fake_server

#endif  // SYNC_TEST_FAKE_SERVER_FAKE_SERVER_HTTP_POST_PROVIDER_H_
