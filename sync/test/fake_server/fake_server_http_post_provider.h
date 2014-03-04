// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_TEST_FAKE_SERVER_FAKE_SERVER_HTTP_POST_PROVIDER_H_
#define SYNC_TEST_FAKE_SERVER_FAKE_SERVER_HTTP_POST_PROVIDER_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "sync/internal_api/public/http_post_provider_factory.h"
#include "sync/internal_api/public/http_post_provider_interface.h"

namespace fake_server {

class FakeServer;

class FakeServerHttpPostProvider
    : public syncer::HttpPostProviderInterface,
      public base::RefCountedThreadSafe<FakeServerHttpPostProvider> {
 public:
  explicit FakeServerHttpPostProvider(FakeServer* fake_server);

  // HttpPostProviderInterface implementation.
  virtual void SetExtraRequestHeaders(const char* headers) OVERRIDE;
  virtual void SetURL(const char* url, int port) OVERRIDE;
  virtual void SetPostPayload(const char* content_type, int content_length,
                              const char* content) OVERRIDE;
  virtual bool MakeSynchronousPost(int* error_code,
                                   int* response_code) OVERRIDE;
  virtual void Abort() OVERRIDE;
  virtual int GetResponseContentLength() const OVERRIDE;
  virtual const char* GetResponseContent() const OVERRIDE;
  virtual const std::string GetResponseHeaderValue(
      const std::string& name) const OVERRIDE;

 protected:
  friend class base::RefCountedThreadSafe<FakeServerHttpPostProvider>;
  virtual ~FakeServerHttpPostProvider();

 private:
  FakeServer* const fake_server_;
  std::string response_;
  std::string request_url_;
  int request_port_;
  std::string request_content_;
  std::string request_content_type_;
  std::string extra_request_headers_;

  DISALLOW_COPY_AND_ASSIGN(FakeServerHttpPostProvider);
};

class FakeServerHttpPostProviderFactory
    : public syncer::HttpPostProviderFactory {
 public:
  explicit FakeServerHttpPostProviderFactory(FakeServer* fake_server);
  virtual ~FakeServerHttpPostProviderFactory();

  // HttpPostProviderFactory:
  virtual void Init(const std::string& user_agent) OVERRIDE;
  virtual syncer::HttpPostProviderInterface* Create() OVERRIDE;
  virtual void Destroy(syncer::HttpPostProviderInterface* http) OVERRIDE;

 private:
  FakeServer* const fake_server_;

  DISALLOW_COPY_AND_ASSIGN(FakeServerHttpPostProviderFactory);
};

}  //  namespace fake_server

#endif  // SYNC_TEST_FAKE_SERVER_FAKE_SERVER_HTTP_POST_PROVIDER_H_
