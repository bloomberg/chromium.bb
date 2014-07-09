// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_test_util.h"
#include "sync/internal_api/public/base/cancelation_signal.h"
#include "sync/internal_api/public/http_bridge.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {
// TODO(timsteele): Should use PathService here. See Chromium Issue 3113.
const base::FilePath::CharType kDocRoot[] =
    FILE_PATH_LITERAL("chrome/test/data");
}

class SyncHttpBridgeTest : public testing::Test {
 public:
  SyncHttpBridgeTest()
      : test_server_(net::SpawnedTestServer::TYPE_HTTP,
                     net::SpawnedTestServer::kLocalhost,
                     base::FilePath(kDocRoot)),
        fake_default_request_context_getter_(NULL),
        bridge_for_race_test_(NULL),
        io_thread_("IO thread") {
  }

  virtual void SetUp() {
    base::Thread::Options options;
    options.message_loop_type = base::MessageLoop::TYPE_IO;
    io_thread_.StartWithOptions(options);
  }

  virtual void TearDown() {
    if (fake_default_request_context_getter_) {
      GetIOThreadLoop()->ReleaseSoon(FROM_HERE,
          fake_default_request_context_getter_);
      fake_default_request_context_getter_ = NULL;
    }
    io_thread_.Stop();
  }

  HttpBridge* BuildBridge() {
    if (!fake_default_request_context_getter_) {
      fake_default_request_context_getter_ =
          new net::TestURLRequestContextGetter(io_thread_.message_loop_proxy());
      fake_default_request_context_getter_->AddRef();
    }
    HttpBridge* bridge = new HttpBridge(
        new HttpBridge::RequestContextGetter(
            fake_default_request_context_getter_,
            "user agent"),
        NetworkTimeUpdateCallback());
    return bridge;
  }

  static void Abort(HttpBridge* bridge) {
    bridge->Abort();
  }

  // Used by AbortAndReleaseBeforeFetchCompletes to test an interesting race
  // condition.
  void RunSyncThreadBridgeUseTest(base::WaitableEvent* signal_when_created,
                                  base::WaitableEvent* signal_when_released);

  static void TestSameHttpNetworkSession(base::MessageLoop* main_message_loop,
                                         SyncHttpBridgeTest* test) {
    scoped_refptr<HttpBridge> http_bridge(test->BuildBridge());
    EXPECT_TRUE(test->GetTestRequestContextGetter());
    net::HttpNetworkSession* test_session =
        test->GetTestRequestContextGetter()->GetURLRequestContext()->
        http_transaction_factory()->GetSession();
    EXPECT_EQ(test_session,
              http_bridge->GetRequestContextGetterForTest()->
                  GetURLRequestContext()->
                  http_transaction_factory()->GetSession());
    main_message_loop->PostTask(FROM_HERE, base::MessageLoop::QuitClosure());
  }

  base::MessageLoop* GetIOThreadLoop() { return io_thread_.message_loop(); }

  // Note this is lazy created, so don't call this before your bridge.
  net::TestURLRequestContextGetter* GetTestRequestContextGetter() {
    return fake_default_request_context_getter_;
  }

  net::SpawnedTestServer test_server_;

  base::Thread* io_thread() { return &io_thread_; }

  HttpBridge* bridge_for_race_test() { return bridge_for_race_test_; }

 private:
  // A make-believe "default" request context, as would be returned by
  // Profile::GetDefaultRequestContext().  Created lazily by BuildBridge.
  net::TestURLRequestContextGetter* fake_default_request_context_getter_;

  HttpBridge* bridge_for_race_test_;

  // Separate thread for IO used by the HttpBridge.
  base::Thread io_thread_;
  base::MessageLoop loop_;
};

// An HttpBridge that doesn't actually make network requests and just calls
// back with dummy response info.
// TODO(tim): Instead of inheriting here we should inject a component
// responsible for the MakeAsynchronousPost bit.
class ShuntedHttpBridge : public HttpBridge {
 public:
  // If |never_finishes| is true, the simulated request never actually
  // returns.
  ShuntedHttpBridge(net::URLRequestContextGetter* baseline_context_getter,
                    SyncHttpBridgeTest* test, bool never_finishes)
      : HttpBridge(
          new HttpBridge::RequestContextGetter(
              baseline_context_getter, "user agent"),
          NetworkTimeUpdateCallback()),
        test_(test), never_finishes_(never_finishes) { }
 protected:
  virtual void MakeAsynchronousPost() OVERRIDE {
    ASSERT_TRUE(base::MessageLoop::current() == test_->GetIOThreadLoop());
    if (never_finishes_)
      return;

    // We don't actually want to make a request for this test, so just callback
    // as if it completed.
    test_->GetIOThreadLoop()->PostTask(FROM_HERE,
        base::Bind(&ShuntedHttpBridge::CallOnURLFetchComplete, this));
  }
 private:
  virtual ~ShuntedHttpBridge() {}

  void CallOnURLFetchComplete() {
    ASSERT_TRUE(base::MessageLoop::current() == test_->GetIOThreadLoop());
    // We return no cookies and a dummy content response.
    net::ResponseCookies cookies;

    std::string response_content = "success!";
    net::TestURLFetcher fetcher(0, GURL(), NULL);
    fetcher.set_url(GURL("www.google.com"));
    fetcher.set_response_code(200);
    fetcher.set_cookies(cookies);
    fetcher.SetResponseString(response_content);
    OnURLFetchComplete(&fetcher);
  }
  SyncHttpBridgeTest* test_;
  bool never_finishes_;
};

void SyncHttpBridgeTest::RunSyncThreadBridgeUseTest(
    base::WaitableEvent* signal_when_created,
    base::WaitableEvent* signal_when_released) {
  scoped_refptr<net::URLRequestContextGetter> ctx_getter(
      new net::TestURLRequestContextGetter(io_thread_.message_loop_proxy()));
  {
    scoped_refptr<ShuntedHttpBridge> bridge(
        new ShuntedHttpBridge(ctx_getter.get(), this, true));
    bridge->SetURL("http://www.google.com", 9999);
    bridge->SetPostPayload("text/plain", 2, " ");
    bridge_for_race_test_ = bridge.get();
    signal_when_created->Signal();

    int os_error = 0;
    int response_code = 0;
    bridge->MakeSynchronousPost(&os_error, &response_code);
    bridge_for_race_test_ = NULL;
  }
  signal_when_released->Signal();
}

TEST_F(SyncHttpBridgeTest, TestUsesSameHttpNetworkSession) {
  // Run this test on the IO thread because we can only call
  // URLRequestContextGetter::GetURLRequestContext on the IO thread.
  io_thread()->message_loop()
      ->PostTask(FROM_HERE,
                 base::Bind(&SyncHttpBridgeTest::TestSameHttpNetworkSession,
                            base::MessageLoop::current(),
                            this));
  base::MessageLoop::current()->Run();
}

// Test the HttpBridge without actually making any network requests.
TEST_F(SyncHttpBridgeTest, TestMakeSynchronousPostShunted) {
  scoped_refptr<net::URLRequestContextGetter> ctx_getter(
      new net::TestURLRequestContextGetter(io_thread()->message_loop_proxy()));
  scoped_refptr<HttpBridge> http_bridge(
      new ShuntedHttpBridge(ctx_getter.get(), this, false));
  http_bridge->SetURL("http://www.google.com", 9999);
  http_bridge->SetPostPayload("text/plain", 2, " ");

  int os_error = 0;
  int response_code = 0;
  bool success = http_bridge->MakeSynchronousPost(&os_error, &response_code);
  EXPECT_TRUE(success);
  EXPECT_EQ(200, response_code);
  EXPECT_EQ(0, os_error);

  EXPECT_EQ(8, http_bridge->GetResponseContentLength());
  EXPECT_EQ(std::string("success!"),
            std::string(http_bridge->GetResponseContent()));
}

// Full round-trip test of the HttpBridge, using default UA string and
// no request cookies.
TEST_F(SyncHttpBridgeTest, TestMakeSynchronousPostLiveWithPayload) {
  ASSERT_TRUE(test_server_.Start());

  scoped_refptr<HttpBridge> http_bridge(BuildBridge());

  std::string payload = "this should be echoed back";
  GURL echo = test_server_.GetURL("echo");
  http_bridge->SetURL(echo.spec().c_str(), echo.IntPort());
  http_bridge->SetPostPayload("application/x-www-form-urlencoded",
                              payload.length() + 1, payload.c_str());
  int os_error = 0;
  int response_code = 0;
  bool success = http_bridge->MakeSynchronousPost(&os_error, &response_code);
  EXPECT_TRUE(success);
  EXPECT_EQ(200, response_code);
  EXPECT_EQ(0, os_error);

  EXPECT_EQ(payload.length() + 1,
            static_cast<size_t>(http_bridge->GetResponseContentLength()));
  EXPECT_EQ(payload, std::string(http_bridge->GetResponseContent()));
}

// Full round-trip test of the HttpBridge.
TEST_F(SyncHttpBridgeTest, TestMakeSynchronousPostLiveComprehensive) {
  ASSERT_TRUE(test_server_.Start());

  scoped_refptr<HttpBridge> http_bridge(BuildBridge());

  GURL echo_header = test_server_.GetURL("echoall");
  http_bridge->SetURL(echo_header.spec().c_str(), echo_header.IntPort());

  std::string test_payload = "###TEST PAYLOAD###";
  http_bridge->SetPostPayload("text/html", test_payload.length() + 1,
                              test_payload.c_str());

  int os_error = 0;
  int response_code = 0;
  bool success = http_bridge->MakeSynchronousPost(&os_error, &response_code);
  EXPECT_TRUE(success);
  EXPECT_EQ(200, response_code);
  EXPECT_EQ(0, os_error);

  std::string response(http_bridge->GetResponseContent(),
                       http_bridge->GetResponseContentLength());
  EXPECT_EQ(std::string::npos, response.find("Cookie:"));
  EXPECT_NE(std::string::npos, response.find("User-Agent: user agent"));
  EXPECT_NE(std::string::npos, response.find(test_payload.c_str()));
}

TEST_F(SyncHttpBridgeTest, TestExtraRequestHeaders) {
  ASSERT_TRUE(test_server_.Start());

  scoped_refptr<HttpBridge> http_bridge(BuildBridge());

  GURL echo_header = test_server_.GetURL("echoall");

  http_bridge->SetURL(echo_header.spec().c_str(), echo_header.IntPort());
  http_bridge->SetExtraRequestHeaders("test:fnord");

  std::string test_payload = "###TEST PAYLOAD###";
  http_bridge->SetPostPayload("text/html", test_payload.length() + 1,
                              test_payload.c_str());

  int os_error = 0;
  int response_code = 0;
  bool success = http_bridge->MakeSynchronousPost(&os_error, &response_code);
  EXPECT_TRUE(success);
  EXPECT_EQ(200, response_code);
  EXPECT_EQ(0, os_error);

  std::string response(http_bridge->GetResponseContent(),
                       http_bridge->GetResponseContentLength());

  EXPECT_NE(std::string::npos, response.find("fnord"));
  EXPECT_NE(std::string::npos, response.find(test_payload.c_str()));
}

TEST_F(SyncHttpBridgeTest, TestResponseHeader) {
  ASSERT_TRUE(test_server_.Start());

  scoped_refptr<HttpBridge> http_bridge(BuildBridge());

  GURL echo_header = test_server_.GetURL("echoall");
  http_bridge->SetURL(echo_header.spec().c_str(), echo_header.IntPort());

  std::string test_payload = "###TEST PAYLOAD###";
  http_bridge->SetPostPayload("text/html", test_payload.length() + 1,
                              test_payload.c_str());

  int os_error = 0;
  int response_code = 0;
  bool success = http_bridge->MakeSynchronousPost(&os_error, &response_code);
  EXPECT_TRUE(success);
  EXPECT_EQ(200, response_code);
  EXPECT_EQ(0, os_error);

  EXPECT_EQ(http_bridge->GetResponseHeaderValue("Content-type"), "text/html");
  EXPECT_TRUE(http_bridge->GetResponseHeaderValue("invalid-header").empty());
}

TEST_F(SyncHttpBridgeTest, Abort) {
  scoped_refptr<net::URLRequestContextGetter> ctx_getter(
      new net::TestURLRequestContextGetter(io_thread()->message_loop_proxy()));
  scoped_refptr<ShuntedHttpBridge> http_bridge(
      new ShuntedHttpBridge(ctx_getter.get(), this, true));
  http_bridge->SetURL("http://www.google.com", 9999);
  http_bridge->SetPostPayload("text/plain", 2, " ");

  int os_error = 0;
  int response_code = 0;

  io_thread()->message_loop_proxy()->PostTask(
      FROM_HERE,
      base::Bind(&SyncHttpBridgeTest::Abort, http_bridge));
  bool success = http_bridge->MakeSynchronousPost(&os_error, &response_code);
  EXPECT_FALSE(success);
  EXPECT_EQ(net::ERR_ABORTED, os_error);
}

TEST_F(SyncHttpBridgeTest, AbortLate) {
  scoped_refptr<net::URLRequestContextGetter> ctx_getter(
      new net::TestURLRequestContextGetter(io_thread()->message_loop_proxy()));
  scoped_refptr<ShuntedHttpBridge> http_bridge(
      new ShuntedHttpBridge(ctx_getter.get(), this, false));
  http_bridge->SetURL("http://www.google.com", 9999);
  http_bridge->SetPostPayload("text/plain", 2, " ");

  int os_error = 0;
  int response_code = 0;

  bool success = http_bridge->MakeSynchronousPost(&os_error, &response_code);
  ASSERT_TRUE(success);
  http_bridge->Abort();
  // Ensures no double-free of URLFetcher, etc.
}

// Tests an interesting case where code using the HttpBridge aborts the fetch
// and releases ownership before a pending fetch completed callback is issued by
// the underlying URLFetcher (and before that URLFetcher is destroyed, which
// would cancel the callback).
TEST_F(SyncHttpBridgeTest, AbortAndReleaseBeforeFetchComplete) {
  base::Thread sync_thread("SyncThread");
  sync_thread.Start();

  // First, block the sync thread on the post.
  base::WaitableEvent signal_when_created(false, false);
  base::WaitableEvent signal_when_released(false, false);
  sync_thread.message_loop()->PostTask(FROM_HERE,
      base::Bind(&SyncHttpBridgeTest::RunSyncThreadBridgeUseTest,
                 base::Unretained(this),
                 &signal_when_created,
                 &signal_when_released));

  // Stop IO so we can control order of operations.
  base::WaitableEvent io_waiter(false, false);
  ASSERT_TRUE(io_thread()->message_loop_proxy()->PostTask(
      FROM_HERE,
      base::Bind(&base::WaitableEvent::Wait, base::Unretained(&io_waiter))));

  signal_when_created.Wait();  // Wait till we have a bridge to abort.
  ASSERT_TRUE(bridge_for_race_test());

  // Schedule the fetch completion callback (but don't run it yet). Don't take
  // a reference to the bridge to mimic URLFetcher's handling of the delegate.
  net::URLFetcherDelegate* delegate =
      static_cast<net::URLFetcherDelegate*>(bridge_for_race_test());
  net::ResponseCookies cookies;
  std::string response_content = "success!";
  net::TestURLFetcher fetcher(0, GURL(), NULL);
  fetcher.set_url(GURL("www.google.com"));
  fetcher.set_response_code(200);
  fetcher.set_cookies(cookies);
  fetcher.SetResponseString(response_content);
  ASSERT_TRUE(io_thread()->message_loop_proxy()->PostTask(
      FROM_HERE,
      base::Bind(&net::URLFetcherDelegate::OnURLFetchComplete,
          base::Unretained(delegate), &fetcher)));

  // Abort the fetch. This should be smart enough to handle the case where
  // the bridge is destroyed before the callback scheduled above completes.
  bridge_for_race_test()->Abort();

  // Wait until the sync thread releases its ref on the bridge.
  signal_when_released.Wait();
  ASSERT_FALSE(bridge_for_race_test());

  // Unleash the hounds. The fetch completion callback should fire first, and
  // succeed even though we Release()d the bridge above because the call to
  // Abort should have held a reference.
  io_waiter.Signal();

  // Done.
  sync_thread.Stop();
  io_thread()->Stop();
}

void HttpBridgeRunOnSyncThread(
    net::URLRequestContextGetter* baseline_context_getter,
    CancelationSignal* factory_cancelation_signal,
    syncer::HttpPostProviderFactory** bridge_factory_out,
    syncer::HttpPostProviderInterface** bridge_out,
    base::WaitableEvent* signal_when_created,
    base::WaitableEvent* wait_for_shutdown) {
  scoped_ptr<syncer::HttpBridgeFactory> bridge_factory(
      new syncer::HttpBridgeFactory(baseline_context_getter,
                                    NetworkTimeUpdateCallback(),
                                    factory_cancelation_signal));
  bridge_factory->Init("test");
  *bridge_factory_out = bridge_factory.get();

  HttpPostProviderInterface* bridge = bridge_factory->Create();
  *bridge_out = bridge;

  signal_when_created->Signal();
  wait_for_shutdown->Wait();

  bridge_factory->Destroy(bridge);
}

void WaitOnIOThread(base::WaitableEvent* signal_wait_start,
                    base::WaitableEvent* wait_done) {
  signal_wait_start->Signal();
  wait_done->Wait();
}

// Tests RequestContextGetter is properly released on IO thread even when
// IO thread stops before sync thread.
TEST_F(SyncHttpBridgeTest, RequestContextGetterReleaseOrder) {
  base::Thread sync_thread("SyncThread");
  sync_thread.Start();

  syncer::HttpPostProviderFactory* factory = NULL;
  syncer::HttpPostProviderInterface* bridge = NULL;

  scoped_refptr<net::URLRequestContextGetter> baseline_context_getter(
      new net::TestURLRequestContextGetter(io_thread()->message_loop_proxy()));

  base::WaitableEvent signal_when_created(false, false);
  base::WaitableEvent wait_for_shutdown(false, false);

  CancelationSignal release_request_context_signal;

  // Create bridge factory and factory on sync thread and wait for the creation
  // to finish.
  sync_thread.message_loop()->PostTask(FROM_HERE,
      base::Bind(&HttpBridgeRunOnSyncThread,
                 base::Unretained(baseline_context_getter.get()),
                 &release_request_context_signal ,&factory, &bridge,
                 &signal_when_created, &wait_for_shutdown));
  signal_when_created.Wait();

  // Simulate sync shutdown by aborting bridge and shutting down factory on
  // frontend.
  bridge->Abort();
  release_request_context_signal.Signal();

  // Wait for sync's RequestContextGetter to be cleared on IO thread and
  // check for reference count.
  base::WaitableEvent signal_wait_start(false, false);
  base::WaitableEvent wait_done(false, false);
  io_thread()->message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&WaitOnIOThread, &signal_wait_start, &wait_done));
  signal_wait_start.Wait();
  // |baseline_context_getter| should have only one reference from local
  // variable.
  EXPECT_TRUE(baseline_context_getter->HasOneRef());
  baseline_context_getter = NULL;

  // Unblock and stop IO thread before sync thread.
  wait_done.Signal();
  io_thread()->Stop();

  // Unblock and stop sync thread.
  wait_for_shutdown.Signal();
  sync_thread.Stop();
}

// Attempt to release the URLRequestContextGetter before the HttpBridgeFactory
// is initialized.
TEST_F(SyncHttpBridgeTest, EarlyAbortFactory) {
  // In a real scenario, the following would happen on many threads.  For
  // simplicity, this test uses only one thread.

  scoped_refptr<net::URLRequestContextGetter> baseline_context_getter(
      new net::TestURLRequestContextGetter(io_thread()->message_loop_proxy()));
  CancelationSignal release_request_context_signal;

  // UI Thread: Initialize the HttpBridgeFactory.  The next step would be to
  // post a task to SBH::Core to have it initialized.
  scoped_ptr<syncer::HttpBridgeFactory> factory(new HttpBridgeFactory(
      baseline_context_getter,
      NetworkTimeUpdateCallback(),
      &release_request_context_signal));

  // UI Thread: A very early shutdown request arrives and executes on the UI
  // thread before the posted sync thread task is run.
  release_request_context_signal.Signal();

  // Sync thread: Finally run the posted task, only to find that our
  // HttpBridgeFactory has been neutered.  Should not crash.
  factory->Init("TestUserAgent");

  // At this point, attempting to use the factory would trigger a crash.  Both
  // this test and the real world code should make sure this never happens.
};

}  // namespace syncer
