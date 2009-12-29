// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "base/thread.h"
#include "base/waitable_event.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_error_job.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/appcache/appcache.h"
#include "webkit/appcache/appcache_request_handler.h"
#include "webkit/appcache/appcache_url_request_job.h"
#include "webkit/appcache/mock_appcache_service.h"

namespace appcache {

class AppCacheRequestHandlerTest : public testing::Test {
 public:
  class MockFrontend : public AppCacheFrontend {
   public:
    virtual void OnCacheSelected(int host_id, int64 cache_id ,
                                 appcache::Status status) {}

    virtual void OnStatusChanged(const std::vector<int>& host_ids,
                                 appcache::Status status) {}

    virtual void OnEventRaised(const std::vector<int>& host_ids,
                               appcache::EventID event_id) {}
  };

  // Helper class run a test on our io_thread. The io_thread
  // is spun up once and reused for all tests.
  template <class Method>
  class WrapperTask : public Task {
   public:
    WrapperTask(AppCacheRequestHandlerTest* test, Method method)
        : test_(test), method_(method) {
    }

    virtual void Run() {
      test_->SetUpTest();
      (test_->*method_)();
    }

   private:
    AppCacheRequestHandlerTest* test_;
    Method method_;
  };

  // Subclasses to simulate particular response codes so test cases can
  // exercise fallback code paths.

  class MockURLRequestJob : public URLRequestJob {
   public:
    MockURLRequestJob(URLRequest* request, int response_code)
        : URLRequestJob(request), response_code_(response_code) {}
    virtual void Start() {}
    virtual int GetResponseCode() const { return response_code_; }
    int response_code_;
  };

  class MockURLRequest : public URLRequest {
   public:
    explicit MockURLRequest(const GURL& url) : URLRequest(url, NULL) {}

    void SimulateResponseCode(int http_response_code) {
      mock_factory_job_ = new MockURLRequestJob(this, http_response_code);
      Start();
      DCHECK(!mock_factory_job_);
      // All our simulation need to do satisfy are the following two DCHECKs
      DCHECK(status().is_success());
      DCHECK_EQ(http_response_code, GetResponseCode());
    }
  };

  static URLRequestJob* MockHttpJobFactory(URLRequest* request,
                                           const std::string& scheme) {
    if (mock_factory_job_) {
      URLRequestJob* temp = mock_factory_job_;
      mock_factory_job_ = NULL;
      return temp;
    } else {
      // Some of these tests trigger UpdateJobs which start URLRequests.
      // We short circuit those be returning error jobs.
      return new URLRequestErrorJob(request, net::ERR_INTERNET_DISCONNECTED);
    }
  }

  static void SetUpTestCase() {
    io_thread_.reset(new base::Thread("AppCacheRequestHandlerTest Thread"));
    base::Thread::Options options(MessageLoop::TYPE_IO, 0);
    io_thread_->StartWithOptions(options);
  }

  static void TearDownTestCase() {
    io_thread_.reset(NULL);
  }

  // Test harness --------------------------------------------------

  AppCacheRequestHandlerTest()
      : ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)),
        orig_http_factory_(NULL) {
  }

  template <class Method>
  void RunTestOnIOThread(Method method) {
    test_finished_event_ .reset(new base::WaitableEvent(false, false));
    io_thread_->message_loop()->PostTask(
        FROM_HERE, new WrapperTask<Method>(this, method));
    test_finished_event_->Wait();
  }

  void SetUpTest() {
    DCHECK(MessageLoop::current() == io_thread_->message_loop());
    orig_http_factory_ = URLRequest::RegisterProtocolFactory(
        "http", MockHttpJobFactory);
    mock_service_.reset(new MockAppCacheService);
    mock_frontend_.reset(new MockFrontend);
    host_.reset(
        new AppCacheHost(1, mock_frontend_.get(), mock_service_.get()));
  }

  void TearDownTest() {
    DCHECK(MessageLoop::current() == io_thread_->message_loop());
    DCHECK(!mock_factory_job_);
    URLRequest::RegisterProtocolFactory("http", orig_http_factory_);
    orig_http_factory_ = NULL;
    job_ = NULL;
    handler_.reset();
    request_.reset();
    host_.reset();
    mock_frontend_.reset();
    mock_service_.reset();
  }

  void TestFinished() {
    // We unwind the stack prior to finishing up to let stack
    // based objects get deleted.
    DCHECK(MessageLoop::current() == io_thread_->message_loop());
    MessageLoop::current()->PostTask(FROM_HERE,
        method_factory_.NewRunnableMethod(
            &AppCacheRequestHandlerTest::TestFinishedUnwound));
  }

  void TestFinishedUnwound() {
    TearDownTest();
    test_finished_event_->Signal();
  }

  void PushNextTask(Task* task) {
    task_stack_.push(task);
  }

  void ScheduleNextTask() {
    DCHECK(MessageLoop::current() == io_thread_->message_loop());
    if (task_stack_.empty()) {
      TestFinished();
      return;
    }
    MessageLoop::current()->PostTask(FROM_HERE, task_stack_.top());
    task_stack_.pop();
  }

  // MainResource_Miss --------------------------------------------------

  void MainResource_Miss() {
    PushNextTask(method_factory_.NewRunnableMethod(
       &AppCacheRequestHandlerTest::Verify_MainResource_Miss));

    request_.reset(new MockURLRequest(GURL("http://blah/")));
    handler_.reset(host_->CreateRequestHandler(request_.get(), true));
    EXPECT_TRUE(handler_.get());

    job_ = handler_->MaybeLoadResource(request_.get());
    EXPECT_TRUE(job_.get());
    EXPECT_TRUE(job_->is_waiting());

    // We have to wait for completion of storage->FindResponseForMainRequest.
    ScheduleNextTask();
  }

  void Verify_MainResource_Miss() {
    EXPECT_FALSE(job_->is_waiting());
    EXPECT_TRUE(job_->is_delivering_network_response());

    int64 cache_id = kNoCacheId;
    GURL manifest_url;
    handler_->GetExtraResponseInfo(&cache_id, &manifest_url);
    EXPECT_EQ(kNoCacheId, cache_id);
    EXPECT_EQ(GURL(), manifest_url);

    AppCacheURLRequestJob* fallback_job;
    fallback_job = handler_->MaybeLoadFallbackForRedirect(
        request_.get(), GURL("http://blah/redirect"));
    EXPECT_FALSE(fallback_job);
    fallback_job = handler_->MaybeLoadFallbackForResponse(request_.get());
    EXPECT_FALSE(fallback_job);

    TestFinished();
  }

  // MainResource_Hit --------------------------------------------------

  void MainResource_Hit() {
    PushNextTask(method_factory_.NewRunnableMethod(
       &AppCacheRequestHandlerTest::Verify_MainResource_Hit));

    request_.reset(new MockURLRequest(GURL("http://blah/")));
    handler_.reset(host_->CreateRequestHandler(request_.get(), true));
    EXPECT_TRUE(handler_.get());

    mock_storage()->SimulateFindMainResource(
        AppCacheEntry(AppCacheEntry::EXPLICIT, 1), AppCacheEntry(),
        1, GURL("http://blah/manifest/"));

    job_ = handler_->MaybeLoadResource(request_.get());
    EXPECT_TRUE(job_.get());
    EXPECT_TRUE(job_->is_waiting());

    // We have to wait for completion of storage->FindResponseForMainRequest.
    ScheduleNextTask();
  }

  void Verify_MainResource_Hit() {
    EXPECT_FALSE(job_->is_waiting());
    EXPECT_TRUE(job_->is_delivering_appcache_response());

    int64 cache_id = kNoCacheId;
    GURL manifest_url;
    handler_->GetExtraResponseInfo(&cache_id, &manifest_url);
    EXPECT_EQ(1, cache_id);
    EXPECT_EQ(GURL("http://blah/manifest/"), manifest_url);

    AppCacheURLRequestJob* fallback_job;
    fallback_job = handler_->MaybeLoadFallbackForResponse(request_.get());
    EXPECT_FALSE(fallback_job);

    TestFinished();
  }

  // MainResource_Fallback --------------------------------------------------

  void MainResource_Fallback() {
    PushNextTask(method_factory_.NewRunnableMethod(
       &AppCacheRequestHandlerTest::Verify_MainResource_Fallback));

    request_.reset(new MockURLRequest(GURL("http://blah/")));
    handler_.reset(host_->CreateRequestHandler(request_.get(), true));
    EXPECT_TRUE(handler_.get());

    mock_storage()->SimulateFindMainResource(
        AppCacheEntry(), AppCacheEntry(AppCacheEntry::EXPLICIT, 1),
        1, GURL("http://blah/manifest/"));

    job_ = handler_->MaybeLoadResource(request_.get());
    EXPECT_TRUE(job_.get());
    EXPECT_TRUE(job_->is_waiting());

    // We have to wait for completion of storage->FindResponseForMainRequest.
    ScheduleNextTask();
  }

  void Verify_MainResource_Fallback() {
    EXPECT_FALSE(job_->is_waiting());
    EXPECT_TRUE(job_->is_delivering_network_response());

    // When the request is restarted, the existing job is dropped so a
    // real network job gets created. We expect NULL here which will cause
    // the net library to create a real job.
    job_ = handler_->MaybeLoadResource(request_.get());
    EXPECT_FALSE(job_);

    // Simulate an http error of the real network job.
    request_->SimulateResponseCode(500);

    job_ = handler_->MaybeLoadFallbackForResponse(request_.get());
    EXPECT_TRUE(job_);
    EXPECT_TRUE(job_->is_delivering_appcache_response());

    int64 cache_id = kNoCacheId;
    GURL manifest_url;
    handler_->GetExtraResponseInfo(&cache_id, &manifest_url);
    EXPECT_EQ(1, cache_id);
    EXPECT_EQ(GURL("http://blah/manifest/"), manifest_url);

    TestFinished();
  }

  // SubResource_Miss_WithNoCacheSelected ----------------------------------

  void SubResource_Miss_WithNoCacheSelected() {
    request_.reset(new MockURLRequest(GURL("http://blah/")));
    handler_.reset(host_->CreateRequestHandler(request_.get(), false));

    // We avoid creating handler when possible, sub-resource requests are not
    // subject to retrieval from an appcache when there's no associated cache.
    EXPECT_FALSE(handler_.get());

    TestFinished();
  }

  // SubResource_Miss_WithCacheSelected ----------------------------------

  void SubResource_Miss_WithCacheSelected() {
    // A sub-resource load where the resource is not in an appcache, or
    // in a network or fallback namespace, should result in a failed request.
    host_->AssociateCache(MakeNewCache());

    request_.reset(new MockURLRequest(GURL("http://blah/")));
    handler_.reset(host_->CreateRequestHandler(request_.get(), false));
    EXPECT_TRUE(handler_.get());

    job_ = handler_->MaybeLoadResource(request_.get());
    EXPECT_TRUE(job_.get());
    EXPECT_TRUE(job_->is_delivering_error_response());

    AppCacheURLRequestJob* fallback_job;
    fallback_job = handler_->MaybeLoadFallbackForRedirect(
        request_.get(), GURL("http://blah/redirect"));
    EXPECT_FALSE(fallback_job);
    fallback_job = handler_->MaybeLoadFallbackForResponse(request_.get());
    EXPECT_FALSE(fallback_job);

    TestFinished();
  }

  // SubResource_Miss_WithWaitForCacheSelection -----------------------------

  void SubResource_Miss_WithWaitForCacheSelection() {
    // Precondition, the host is waiting on cache selection.
    scoped_refptr<AppCache> cache(MakeNewCache());
    host_->pending_selected_cache_id_ = cache->cache_id();

    request_.reset(new MockURLRequest(GURL("http://blah/")));
    handler_.reset(host_->CreateRequestHandler(request_.get(), false));
    EXPECT_TRUE(handler_.get());
    job_ = handler_->MaybeLoadResource(request_.get());
    EXPECT_TRUE(job_.get());
    EXPECT_TRUE(job_->is_waiting());

    host_->FinishCacheSelection(cache, NULL);
    EXPECT_FALSE(job_->is_waiting());
    EXPECT_TRUE(job_->is_delivering_error_response());

    AppCacheURLRequestJob* fallback_job;
    fallback_job = handler_->MaybeLoadFallbackForRedirect(
        request_.get(), GURL("http://blah/redirect"));
    EXPECT_FALSE(fallback_job);
    fallback_job = handler_->MaybeLoadFallbackForResponse(request_.get());
    EXPECT_FALSE(fallback_job);

    TestFinished();
  }

  // SubResource_Hit -----------------------------

  void SubResource_Hit() {
    host_->AssociateCache(MakeNewCache());

    mock_storage()->SimulateFindSubResource(
        AppCacheEntry(AppCacheEntry::EXPLICIT, 1), AppCacheEntry(), false);

    request_.reset(new MockURLRequest(GURL("http://blah/")));
    handler_.reset(host_->CreateRequestHandler(request_.get(), false));
    EXPECT_TRUE(handler_.get());
    job_ = handler_->MaybeLoadResource(request_.get());
    EXPECT_TRUE(job_.get());
    EXPECT_TRUE(job_->is_delivering_appcache_response());

    AppCacheURLRequestJob* fallback_job;
    fallback_job = handler_->MaybeLoadFallbackForRedirect(
        request_.get(), GURL("http://blah/redirect"));
    EXPECT_FALSE(fallback_job);
    fallback_job = handler_->MaybeLoadFallbackForResponse(request_.get());
    EXPECT_FALSE(fallback_job);

    TestFinished();
  }

  // SubResource_RedirectFallback -----------------------------

  void SubResource_RedirectFallback() {
    // Redirects to resources in the a different origin are subject to
    // fallback namespaces.
    host_->AssociateCache(MakeNewCache());

    mock_storage()->SimulateFindSubResource(
        AppCacheEntry(), AppCacheEntry(AppCacheEntry::EXPLICIT, 1), false);

    request_.reset(new MockURLRequest(GURL("http://blah/")));
    handler_.reset(host_->CreateRequestHandler(request_.get(), false));
    EXPECT_TRUE(handler_.get());
    job_ = handler_->MaybeLoadResource(request_.get());
    EXPECT_FALSE(job_.get());

    job_ = handler_->MaybeLoadFallbackForRedirect(
        request_.get(), GURL("http://not_blah/redirect"));
    EXPECT_TRUE(job_.get());
    EXPECT_TRUE(job_->is_delivering_appcache_response());

    AppCacheURLRequestJob* fallback_job;
    fallback_job = handler_->MaybeLoadFallbackForResponse(request_.get());
    EXPECT_FALSE(fallback_job);

    TestFinished();
  }

  // SubResource_NoRedirectFallback -----------------------------

  void SubResource_NoRedirectFallback() {
    // Redirects to resources in the same-origin are not subject to
    // fallback namespaces.
    host_->AssociateCache(MakeNewCache());

    mock_storage()->SimulateFindSubResource(
        AppCacheEntry(), AppCacheEntry(AppCacheEntry::EXPLICIT, 1), false);

    request_.reset(new MockURLRequest(GURL("http://blah/")));
    handler_.reset(host_->CreateRequestHandler(request_.get(), false));
    EXPECT_TRUE(handler_.get());
    job_ = handler_->MaybeLoadResource(request_.get());
    EXPECT_FALSE(job_.get());

    AppCacheURLRequestJob* fallback_job;
    fallback_job = handler_->MaybeLoadFallbackForRedirect(
        request_.get(), GURL("http://blah/redirect"));
    EXPECT_FALSE(fallback_job);

    request_->SimulateResponseCode(200);
    fallback_job = handler_->MaybeLoadFallbackForResponse(request_.get());
    EXPECT_FALSE(fallback_job);

    TestFinished();
  }

  // SubResource_Network -----------------------------

  void SubResource_Network() {
    // A sub-resource load where the resource is in a network namespace,
    // should result in the system using a 'real' job to do the network
    // retrieval.
    host_->AssociateCache(MakeNewCache());

    mock_storage()->SimulateFindSubResource(
        AppCacheEntry(), AppCacheEntry(), true);

    request_.reset(new MockURLRequest(GURL("http://blah/")));
    handler_.reset(host_->CreateRequestHandler(request_.get(), false));
    EXPECT_TRUE(handler_.get());
    job_ = handler_->MaybeLoadResource(request_.get());
    EXPECT_FALSE(job_.get());

    AppCacheURLRequestJob* fallback_job;
    fallback_job = handler_->MaybeLoadFallbackForRedirect(
        request_.get(), GURL("http://blah/redirect"));
    EXPECT_FALSE(fallback_job);
    fallback_job = handler_->MaybeLoadFallbackForResponse(request_.get());
    EXPECT_FALSE(fallback_job);

    TestFinished();
  }

  // DestroyedHost -----------------------------

  void DestroyedHost() {
    host_->AssociateCache(MakeNewCache());

    mock_storage()->SimulateFindSubResource(
        AppCacheEntry(AppCacheEntry::EXPLICIT, 1), AppCacheEntry(), false);

    request_.reset(new MockURLRequest(GURL("http://blah/")));
    handler_.reset(host_->CreateRequestHandler(request_.get(), false));
    EXPECT_TRUE(handler_.get());

    host_.reset();

    EXPECT_FALSE(handler_->MaybeLoadResource(request_.get()));
    EXPECT_FALSE(handler_->MaybeLoadFallbackForRedirect(
        request_.get(), GURL("http://blah/redirect")));
    EXPECT_FALSE(handler_->MaybeLoadFallbackForResponse(request_.get()));

    TestFinished();
  }

  // DestroyedHostWithWaitingJob -----------------------------

  void DestroyedHostWithWaitingJob() {
    // Precondition, the host is waiting on cache selection.
    host_->pending_selected_cache_id_ = 1;

    request_.reset(new MockURLRequest(GURL("http://blah/")));
    handler_.reset(host_->CreateRequestHandler(request_.get(), false));
    EXPECT_TRUE(handler_.get());

    job_ = handler_->MaybeLoadResource(request_.get());
    EXPECT_TRUE(job_.get());
    EXPECT_TRUE(job_->is_waiting());

    host_.reset();
    EXPECT_TRUE(job_->has_been_killed());

    EXPECT_FALSE(handler_->MaybeLoadResource(request_.get()));
    EXPECT_FALSE(handler_->MaybeLoadFallbackForRedirect(
        request_.get(), GURL("http://blah/redirect")));
    EXPECT_FALSE(handler_->MaybeLoadFallbackForResponse(request_.get()));

    TestFinished();
  }

  // UnsupportedScheme -----------------------------

  void UnsupportedScheme() {
    // Precondition, the host is waiting on cache selection.
    host_->pending_selected_cache_id_ = 1;

    request_.reset(new MockURLRequest(GURL("ftp://blah/")));
    handler_.reset(host_->CreateRequestHandler(request_.get(), false));
    EXPECT_TRUE(handler_.get());  // we could redirect to http (conceivably)

    EXPECT_FALSE(handler_->MaybeLoadResource(request_.get()));
    EXPECT_FALSE(handler_->MaybeLoadFallbackForRedirect(
        request_.get(), GURL("ftp://blah/redirect")));
    EXPECT_FALSE(handler_->MaybeLoadFallbackForResponse(request_.get()));

    TestFinished();
  }

  // CanceledRequest -----------------------------

  void CanceledRequest() {
    request_.reset(new MockURLRequest(GURL("http://blah/")));
    handler_.reset(host_->CreateRequestHandler(request_.get(), true));
    EXPECT_TRUE(handler_.get());

    job_ = handler_->MaybeLoadResource(request_.get());
    EXPECT_TRUE(job_.get());
    EXPECT_TRUE(job_->is_waiting());
    EXPECT_FALSE(job_->has_been_started());

    mock_factory_job_ = job_.get();
    request_->Start();
    EXPECT_TRUE(job_->has_been_started());

    request_->Cancel();
    EXPECT_TRUE(job_->has_been_killed());

    EXPECT_FALSE(handler_->MaybeLoadFallbackForResponse(request_.get()));

    TestFinished();
  }

  // Test case helpers --------------------------------------------------

  AppCache* MakeNewCache() {
    AppCache* cache = new AppCache(
        mock_service_.get(), mock_storage()->NewCacheId());
    cache->set_complete(true);
    AppCacheGroup* group = new AppCacheGroup(
        mock_service_.get(), GURL("http://blah/manifest"),
        mock_storage()->NewGroupId());
    group->AddCache(cache);
    return cache;
  }

  MockAppCacheStorage* mock_storage() {
    return reinterpret_cast<MockAppCacheStorage*>(mock_service_->storage());
  }

  // Data members --------------------------------------------------

  ScopedRunnableMethodFactory<AppCacheRequestHandlerTest> method_factory_;
  scoped_ptr<base::WaitableEvent> test_finished_event_;
  std::stack<Task*> task_stack_;
  scoped_ptr<MockAppCacheService> mock_service_;
  scoped_ptr<MockFrontend> mock_frontend_;
  scoped_ptr<AppCacheHost> host_;
  scoped_ptr<MockURLRequest> request_;
  scoped_ptr<AppCacheRequestHandler> handler_;
  scoped_refptr<AppCacheURLRequestJob> job_;
  URLRequest::ProtocolFactory* orig_http_factory_;

  static scoped_ptr<base::Thread> io_thread_;
  static URLRequestJob* mock_factory_job_;
};

// static
scoped_ptr<base::Thread> AppCacheRequestHandlerTest::io_thread_;
URLRequestJob* AppCacheRequestHandlerTest::mock_factory_job_ = NULL;

TEST_F(AppCacheRequestHandlerTest, MainResource_Miss) {
  RunTestOnIOThread(&AppCacheRequestHandlerTest::MainResource_Miss);
}

TEST_F(AppCacheRequestHandlerTest, MainResource_Hit) {
  RunTestOnIOThread(&AppCacheRequestHandlerTest::MainResource_Hit);
}

TEST_F(AppCacheRequestHandlerTest, MainResource_Fallback) {
  RunTestOnIOThread(&AppCacheRequestHandlerTest::MainResource_Fallback);
}

TEST_F(AppCacheRequestHandlerTest, SubResource_Miss_WithNoCacheSelected) {
  RunTestOnIOThread(
      &AppCacheRequestHandlerTest::SubResource_Miss_WithNoCacheSelected);
}

TEST_F(AppCacheRequestHandlerTest, SubResource_Miss_WithCacheSelected) {
  RunTestOnIOThread(
      &AppCacheRequestHandlerTest::SubResource_Miss_WithCacheSelected);
}

TEST_F(AppCacheRequestHandlerTest,
       SubResource_Miss_WithWaitForCacheSelection) {
  RunTestOnIOThread(
      &AppCacheRequestHandlerTest::SubResource_Miss_WithWaitForCacheSelection);
}

TEST_F(AppCacheRequestHandlerTest, SubResource_Hit) {
  RunTestOnIOThread(&AppCacheRequestHandlerTest::SubResource_Hit);
}

TEST_F(AppCacheRequestHandlerTest, SubResource_RedirectFallback) {
  RunTestOnIOThread(&AppCacheRequestHandlerTest::SubResource_RedirectFallback);
}

TEST_F(AppCacheRequestHandlerTest, SubResource_NoRedirectFallback) {
  RunTestOnIOThread(
    &AppCacheRequestHandlerTest::SubResource_NoRedirectFallback);
}

TEST_F(AppCacheRequestHandlerTest, SubResource_Network) {
  RunTestOnIOThread(&AppCacheRequestHandlerTest::SubResource_Network);
}

TEST_F(AppCacheRequestHandlerTest, DestroyedHost) {
  RunTestOnIOThread(&AppCacheRequestHandlerTest::DestroyedHost);
}

TEST_F(AppCacheRequestHandlerTest, DestroyedHostWithWaitingJob) {
  RunTestOnIOThread(&AppCacheRequestHandlerTest::DestroyedHostWithWaitingJob);
}

TEST_F(AppCacheRequestHandlerTest, UnsupportedScheme) {
  RunTestOnIOThread(&AppCacheRequestHandlerTest::UnsupportedScheme);
}

TEST_F(AppCacheRequestHandlerTest, CanceledRequest) {
  RunTestOnIOThread(&AppCacheRequestHandlerTest::CanceledRequest);
}

}  // namespace appcache

