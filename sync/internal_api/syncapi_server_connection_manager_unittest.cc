// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/syncapi_server_connection_manager.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread.h"
#include "base/time.h"
#include "net/base/net_errors.h"
#include "sync/internal_api/public/http_post_provider_factory.h"
#include "sync/internal_api/public/http_post_provider_interface.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace {

using base::TimeDelta;

class BlockingHttpPost : public HttpPostProviderInterface {
 public:
  BlockingHttpPost() : wait_for_abort_(false, false) {}
  virtual ~BlockingHttpPost() {}

  virtual void SetExtraRequestHeaders(const char* headers) OVERRIDE {}
  virtual void SetURL(const char* url, int port) OVERRIDE {}
  virtual void SetPostPayload(const char* content_type,
                              int content_length,
                              const char* content) OVERRIDE {}
  virtual bool MakeSynchronousPost(int* error_code, int* response_code)
      OVERRIDE {
    wait_for_abort_.TimedWait(TestTimeouts::action_max_timeout());
    *error_code = net::ERR_ABORTED;
    return false;
  }
  virtual int GetResponseContentLength() const OVERRIDE {
    return 0;
  }
  virtual const char* GetResponseContent() const OVERRIDE {
    return "";
  }
  virtual const std::string GetResponseHeaderValue(
      const std::string& name) const OVERRIDE {
    return std::string();
  }
  virtual void Abort() OVERRIDE {
    wait_for_abort_.Signal();
  }
 private:
  base::WaitableEvent wait_for_abort_;
};

class BlockingHttpPostFactory : public HttpPostProviderFactory {
 public:
  virtual ~BlockingHttpPostFactory() {}
  virtual HttpPostProviderInterface* Create() OVERRIDE {
    return new BlockingHttpPost();
  }
  virtual void Destroy(HttpPostProviderInterface* http) OVERRIDE {
    delete static_cast<BlockingHttpPost*>(http);
  }
};

}  // namespace

TEST(SyncAPIServerConnectionManagerTest, EarlyAbortPost) {
  SyncAPIServerConnectionManager server(
      "server", 0, true, new BlockingHttpPostFactory());

  ServerConnectionManager::PostBufferParams params;
  ScopedServerStatusWatcher watcher(&server, &params.response);

  server.TerminateAllIO();
  bool result = server.PostBufferToPath(
      &params, "/testpath", "testauth", &watcher);

  EXPECT_FALSE(result);
  EXPECT_EQ(HttpResponse::CONNECTION_UNAVAILABLE,
            params.response.server_status);
}

TEST(SyncAPIServerConnectionManagerTest, AbortPost) {
  SyncAPIServerConnectionManager server(
      "server", 0, true, new BlockingHttpPostFactory());

  ServerConnectionManager::PostBufferParams params;
  ScopedServerStatusWatcher watcher(&server, &params.response);

  base::Thread abort_thread("Test_AbortThread");
  ASSERT_TRUE(abort_thread.Start());
  abort_thread.message_loop()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&ServerConnectionManager::TerminateAllIO,
                 base::Unretained(&server)),
      TestTimeouts::tiny_timeout());

  bool result = server.PostBufferToPath(
      &params, "/testpath", "testauth", &watcher);

  EXPECT_FALSE(result);
  EXPECT_EQ(HttpResponse::CONNECTION_UNAVAILABLE,
            params.response.server_status);
  abort_thread.Stop();
}

}  // namespace syncer
