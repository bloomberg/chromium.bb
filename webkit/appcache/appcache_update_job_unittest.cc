// Copyright (c) 2009 The Chromium Authos. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include "base/stl_util-inl.h"
#include "base/thread.h"
#include "base/waitable_event.h"
#include "net/url_request/url_request_test_job.h"
#include "net/url_request/url_request_unittest.h"
#include "webkit/appcache/appcache_group.h"
#include "webkit/appcache/appcache_host.h"
#include "webkit/appcache/appcache_update_job.h"
#include "webkit/appcache/mock_appcache_service.h"

namespace appcache {
class AppCacheUpdateJobTest;

const wchar_t kDocRoot[] = L"webkit/appcache/data/appcache_unittest";

class MockFrontend : public AppCacheFrontend {
 public:
  MockFrontend() : start_update_trigger_(CHECKING_EVENT), update_(NULL) { }

  virtual void OnCacheSelected(int host_id, int64 cache_id,
                               Status status) {
  }

  virtual void OnStatusChanged(const std::vector<int>& host_ids,
                               Status status) {
  }

  virtual void OnEventRaised(const std::vector<int>& host_ids,
                             EventID event_id) {
    raised_events_.push_back(RaisedEvent(host_ids, event_id));

    // Trigger additional updates if requested.
    if (event_id == start_update_trigger_ && update_) {
      for (std::vector<AppCacheHost*>::iterator it = update_hosts_.begin();
           it != update_hosts_.end(); ++it) {
        AppCacheHost* host = *it;
        update_->StartUpdate(host,
            (host ? host->pending_master_entry_url() : GURL::EmptyGURL()));
      }
      update_hosts_.clear();  // only trigger once
    }
  }

  void AddExpectedEvent(const std::vector<int>& host_ids, EventID event_id) {
    expected_events_.push_back(RaisedEvent(host_ids, event_id));
  }

  void TriggerAdditionalUpdates(EventID trigger_event,
                                AppCacheUpdateJob* update) {
    start_update_trigger_ = trigger_event;
    update_ = update;
  }

  void AdditionalUpdateHost(AppCacheHost* host) {
    update_hosts_.push_back(host);
  }

  typedef std::vector<int> HostIds;
  typedef std::pair<HostIds, EventID> RaisedEvent;
  typedef std::vector<RaisedEvent> RaisedEvents;
  RaisedEvents raised_events_;

  // Set the expected events if verification needs to happen asynchronously.
  RaisedEvents expected_events_;

  // Add ability for frontend to add master entries to an inprogress update.
  EventID start_update_trigger_;
  AppCacheUpdateJob* update_;
  std::vector<AppCacheHost*> update_hosts_;
};

// Helper class to let us call methods of AppCacheUpdateJobTest on a
// thread of our choice.
template <class Method>
class WrapperTask : public Task {
 public:
  WrapperTask(AppCacheUpdateJobTest* test, Method method)
      : test_(test),
        method_(method) {
  }

  virtual void Run() {
    (test_->*method_)( );
  }

 private:
  AppCacheUpdateJobTest* test_;
  Method method_;
};

// Helper factories to simulate redirected URL responses for tests.
static URLRequestJob* RedirectFactory(URLRequest* request,
                                      const std::string& scheme) {
  return new URLRequestTestJob(request,
                               URLRequestTestJob::test_redirect_headers(),
                               URLRequestTestJob::test_data_1(),
                               true);
}

// Helper class to simulate a URL that returns retry or success.
class RetryRequestTestJob : public URLRequestTestJob {
 public:
  enum RetryHeader {
    NO_RETRY_AFTER,
    NONZERO_RETRY_AFTER,
    RETRY_AFTER_0,
  };

  static const GURL kRetryUrl;

  // Call this at the start of each retry test.
  static void Initialize(int num_retry_responses, RetryHeader header,
      int expected_requests) {
    num_requests_ = 0;
    num_retries_ = num_retry_responses;
    retry_after_ = header;
    expected_requests_ = expected_requests;
  }

  // Verifies results at end of test and resets counters.
  static void Verify() {
    EXPECT_EQ(expected_requests_, num_requests_);
    num_requests_ = 0;
    expected_requests_ = 0;
  }

  static URLRequestJob* RetryFactory(URLRequest* request,
                                   const std::string& scheme) {
    ++num_requests_;
    if (num_retries_ > 0 && request->original_url() == kRetryUrl) {
      --num_retries_;
      return new RetryRequestTestJob(
          request, RetryRequestTestJob::retry_headers(), 503);
    } else {
      return new RetryRequestTestJob(
          request, RetryRequestTestJob::manifest_headers(), 200);
    }
  }

  virtual int GetResponseCode() const { return response_code_; }

 private:
  ~RetryRequestTestJob() {}

  static std::string retry_headers() {
    const char no_retry_after[] =
        "HTTP/1.1 503 BOO HOO\0"
        "\0";
    const char nonzero[] =
        "HTTP/1.1 503 BOO HOO\0"
        "Retry-After: 60\0"
        "\0";
    const char retry_after_0[] =
        "HTTP/1.1 503 BOO HOO\0"
        "Retry-After: 0\0"
        "\0";

    switch (retry_after_) {
      case NO_RETRY_AFTER:
        return std::string(no_retry_after, arraysize(no_retry_after));
      case NONZERO_RETRY_AFTER:
        return std::string(nonzero, arraysize(nonzero));
      case RETRY_AFTER_0:
      default:
        return std::string(retry_after_0, arraysize(retry_after_0));
    }
  }

  static std::string manifest_headers() {
    const char headers[] =
        "HTTP/1.1 200 OK\0"
        "Content-type: text/cache-manifest\0"
        "\0";
    return std::string(headers, arraysize(headers));
  }

  static std::string data() {
    return std::string("CACHE MANIFEST\r"
        "http://retry\r");  // must be same as kRetryUrl
  }

  explicit RetryRequestTestJob(URLRequest* request, const std::string& headers,
                               int response_code)
      : URLRequestTestJob(request, headers, data(), true),
        response_code_(response_code) {
  }

  int response_code_;

  static int num_requests_;
  static int num_retries_;
  static RetryHeader retry_after_;
  static int expected_requests_;
};

// static
const GURL RetryRequestTestJob::kRetryUrl("http://retry");
int RetryRequestTestJob::num_requests_ = 0;
int RetryRequestTestJob::num_retries_;
RetryRequestTestJob::RetryHeader RetryRequestTestJob::retry_after_;
int RetryRequestTestJob::expected_requests_ = 0;

class AppCacheUpdateJobTest : public testing::Test,
                              public AppCacheGroup::UpdateObserver {
 public:
  AppCacheUpdateJobTest()
      : ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)),
        do_checks_after_update_finished_(false),
        expect_group_obsolete_(false),
        expect_group_has_cache_(false),
        expect_old_cache_(NULL),
        expect_newest_cache_(NULL),
        tested_manifest_(NONE),
        registered_factory_(false),
        old_factory_(NULL) {
  }

  static void SetUpTestCase() {
    io_thread_.reset(new base::Thread("AppCacheUpdateJob IO test thread"));
    base::Thread::Options options(MessageLoop::TYPE_IO, 0);
    io_thread_->StartWithOptions(options);

    http_server_ =
        HTTPTestServer::CreateServer(kDocRoot, io_thread_->message_loop());
    ASSERT_TRUE(http_server_);
  }

  static void TearDownTestCase() {
    http_server_ = NULL;
    io_thread_.reset(NULL);
  }

  // Use a separate IO thread to run a test. Thread will be destroyed
  // when it goes out of scope.
  template <class Method>
  void RunTestOnIOThread(Method method) {
    event_ .reset(new base::WaitableEvent(false, false));
    io_thread_->message_loop()->PostTask(
        FROM_HERE, new WrapperTask<Method>(this, method));

    // Wait until task is done before exiting the test.
    event_->Wait();
  }

  void StartCacheAttemptTest() {
    ASSERT_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());

    MakeService();
    group_ = new AppCacheGroup(service_.get(), GURL("http://failme"),
                               service_->storage()->NewGroupId());

    AppCacheUpdateJob* update = new AppCacheUpdateJob(service_.get(), group_);
    group_->update_job_ = update;

    MockFrontend mock_frontend;
    AppCacheHost host(1, &mock_frontend, service_.get());

    update->StartUpdate(&host, GURL::EmptyGURL());

    // Verify state.
    EXPECT_EQ(AppCacheUpdateJob::CACHE_ATTEMPT, update->update_type_);
    EXPECT_EQ(AppCacheUpdateJob::FETCH_MANIFEST, update->internal_state_);
    EXPECT_EQ(AppCacheGroup::CHECKING, group_->update_status());

    // Verify notifications.
    MockFrontend::RaisedEvents& events = mock_frontend.raised_events_;
    size_t expected = 1;
    EXPECT_EQ(expected, events.size());
    EXPECT_EQ(expected, events[0].first.size());
    EXPECT_EQ(host.host_id(), events[0].first[0]);
    EXPECT_EQ(CHECKING_EVENT, events[0].second);

    // Abort as we're not testing actual URL fetches in this test.
    delete update;
    UpdateFinished();
  }

  void StartUpgradeAttemptTest() {
    ASSERT_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());

    {
      MakeService();
      group_ = new AppCacheGroup(service_.get(), GURL("http://failme"),
                                 service_->storage()->NewGroupId());

      // Give the group some existing caches.
      AppCache* cache1 = MakeCacheForGroup(1, 111);
      AppCache* cache2 = MakeCacheForGroup(2, 222);

      // Associate some hosts with caches in the group.
      MockFrontend mock_frontend1;
      MockFrontend mock_frontend2;
      MockFrontend mock_frontend3;

      AppCacheHost host1(1, &mock_frontend1, service_.get());
      host1.AssociateCache(cache1);

      AppCacheHost host2(2, &mock_frontend2, service_.get());
      host2.AssociateCache(cache2);

      AppCacheHost host3(3, &mock_frontend1, service_.get());
      host3.AssociateCache(cache1);

      AppCacheHost host4(4, &mock_frontend3, service_.get());

      AppCacheUpdateJob* update = new AppCacheUpdateJob(service_.get(), group_);
      group_->update_job_ = update;
      update->StartUpdate(&host4, GURL::EmptyGURL());

      // Verify state after starting an update.
      EXPECT_EQ(AppCacheUpdateJob::UPGRADE_ATTEMPT, update->update_type_);
      EXPECT_EQ(AppCacheUpdateJob::FETCH_MANIFEST, update->internal_state_);
      EXPECT_EQ(AppCacheGroup::CHECKING, group_->update_status());

      // Verify notifications.
      MockFrontend::RaisedEvents& events = mock_frontend1.raised_events_;
      size_t expected = 1;
      EXPECT_EQ(expected, events.size());
      expected = 2;  // 2 hosts using frontend1
      EXPECT_EQ(expected, events[0].first.size());
      MockFrontend::HostIds& host_ids = events[0].first;
      EXPECT_TRUE(std::find(host_ids.begin(), host_ids.end(), host1.host_id())
          != host_ids.end());
      EXPECT_TRUE(std::find(host_ids.begin(), host_ids.end(), host3.host_id())
          != host_ids.end());
      EXPECT_EQ(CHECKING_EVENT, events[0].second);

      events = mock_frontend2.raised_events_;
      expected = 1;
      EXPECT_EQ(expected, events.size());
      EXPECT_EQ(expected, events[0].first.size());  // 1 host using frontend2
      EXPECT_EQ(host2.host_id(), events[0].first[0]);
      EXPECT_EQ(CHECKING_EVENT, events[0].second);

      events = mock_frontend3.raised_events_;
      EXPECT_TRUE(events.empty());

      // Abort as we're not testing actual URL fetches in this test.
      delete update;
    }
    UpdateFinished();
  }

  void CacheAttemptFetchManifestFailTest() {
    ASSERT_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());

    MakeService();
    group_ = new AppCacheGroup(service_.get(), GURL("http://failme"),
                               service_->storage()->NewGroupId());
    AppCacheUpdateJob* update = new AppCacheUpdateJob(service_.get(), group_);
    group_->update_job_ = update;

    MockFrontend* frontend = MakeMockFrontend();
    AppCacheHost* host = MakeHost(1, frontend);
    update->StartUpdate(host, GURL::EmptyGURL());
    EXPECT_TRUE(update->manifest_url_request_ != NULL);

    update->manifest_url_request_->SimulateError(-100);

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = false;
    frontend->AddExpectedEvent(MockFrontend::HostIds(1, host->host_id()),
                               CHECKING_EVENT);

    WaitForUpdateToFinish();
  }

  void UpgradeFetchManifestFailTest() {
    ASSERT_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());

    MakeService();
    group_ = new AppCacheGroup(service_.get(), GURL("http://failme"),
                               service_->storage()->NewGroupId());
    AppCacheUpdateJob* update = new AppCacheUpdateJob(service_.get(), group_);
    group_->update_job_ = update;

    AppCache* cache = MakeCacheForGroup(1, 111);
    MockFrontend* frontend1 = MakeMockFrontend();
    MockFrontend* frontend2 = MakeMockFrontend();
    AppCacheHost* host1 = MakeHost(1, frontend1);
    AppCacheHost* host2 = MakeHost(2, frontend2);
    host1->AssociateCache(cache);
    host2->AssociateCache(cache);

    update->StartUpdate(NULL, GURL::EmptyGURL());
    EXPECT_TRUE(update->manifest_url_request_ != NULL);

    update->manifest_url_request_->SimulateError(-100);

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = true;
    expect_newest_cache_ = cache;  // newest cache unaffected by update
    MockFrontend::HostIds ids1(1, host1->host_id());
    frontend1->AddExpectedEvent(ids1, CHECKING_EVENT);
    frontend1->AddExpectedEvent(ids1, ERROR_EVENT);
    MockFrontend::HostIds ids2(1, host2->host_id());
    frontend2->AddExpectedEvent(ids2, CHECKING_EVENT);
    frontend2->AddExpectedEvent(ids2, ERROR_EVENT);

    WaitForUpdateToFinish();
  }

  void ManifestRedirectTest() {
    ASSERT_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());

    old_factory_ =
        URLRequest::RegisterProtocolFactory("http", RedirectFactory);
    registered_factory_ = true;

    MakeService();
    group_ = new AppCacheGroup(service_.get(), GURL("http://testme"),
                               service_->storage()->NewGroupId());
    AppCacheUpdateJob* update = new AppCacheUpdateJob(service_.get(), group_);
    group_->update_job_ = update;

    MockFrontend* frontend = MakeMockFrontend();
    AppCacheHost* host = MakeHost(1, frontend);
    update->StartUpdate(host, GURL::EmptyGURL());
    EXPECT_TRUE(update->manifest_url_request_ != NULL);

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = false;  // redirect is like a failed request
    frontend->AddExpectedEvent(MockFrontend::HostIds(1, host->host_id()),
                               CHECKING_EVENT);

    WaitForUpdateToFinish();
  }

  void ManifestWrongMimeTypeTest() {
    ASSERT_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());

    MakeService();
    group_ = new AppCacheGroup(
        service_.get(), http_server_->TestServerPage("defaultresponse"),
        service_->storage()->NewGroupId());
    AppCacheUpdateJob* update = new AppCacheUpdateJob(service_.get(), group_);
    group_->update_job_ = update;

    MockFrontend* frontend = MakeMockFrontend();
    AppCacheHost* host = MakeHost(1, frontend);
    update->StartUpdate(host, GURL::EmptyGURL());
    EXPECT_TRUE(update->manifest_url_request_ != NULL);

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = false;  // bad mime type is like a failed request
    frontend->AddExpectedEvent(MockFrontend::HostIds(1, host->host_id()),
                               CHECKING_EVENT);

    WaitForUpdateToFinish();
  }

  void ManifestNotFoundTest() {
    ASSERT_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());

    MakeService();
    group_ = new AppCacheGroup(
        service_.get(), http_server_->TestServerPage("files/nosuchfile"),
        service_->storage()->NewGroupId());
    AppCacheUpdateJob* update = new AppCacheUpdateJob(service_.get(), group_);
    group_->update_job_ = update;

    AppCache* cache = MakeCacheForGroup(1, 111);
    MockFrontend* frontend1 = MakeMockFrontend();
    MockFrontend* frontend2 = MakeMockFrontend();
    AppCacheHost* host1 = MakeHost(1, frontend1);
    AppCacheHost* host2 = MakeHost(2, frontend2);
    host1->AssociateCache(cache);
    host2->AssociateCache(cache);

    update->StartUpdate(NULL, GURL::EmptyGURL());
    EXPECT_TRUE(update->manifest_url_request_ != NULL);

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = true;
    expect_group_has_cache_ = true;
    expect_newest_cache_ = cache;  // newest cache unaffected by update
    MockFrontend::HostIds ids1(1, host1->host_id());
    frontend1->AddExpectedEvent(ids1, CHECKING_EVENT);
    frontend1->AddExpectedEvent(ids1, OBSOLETE_EVENT);
    MockFrontend::HostIds ids2(1, host2->host_id());
    frontend2->AddExpectedEvent(ids2, CHECKING_EVENT);
    frontend2->AddExpectedEvent(ids2, OBSOLETE_EVENT);

    WaitForUpdateToFinish();
  }

  void ManifestGoneTest() {
    ASSERT_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());

    MakeService();
    group_ = new AppCacheGroup(
        service_.get(), http_server_->TestServerPage("files/gone"),
        service_->storage()->NewGroupId());
    AppCacheUpdateJob* update = new AppCacheUpdateJob(service_.get(), group_);
    group_->update_job_ = update;

    MockFrontend* frontend = MakeMockFrontend();
    AppCacheHost* host = MakeHost(1, frontend);
    update->StartUpdate(host, GURL::EmptyGURL());
    EXPECT_TRUE(update->manifest_url_request_ != NULL);

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = true;
    expect_group_has_cache_ = false;
    frontend->AddExpectedEvent(MockFrontend::HostIds(1, host->host_id()),
                               CHECKING_EVENT);

    WaitForUpdateToFinish();
  }

  void CacheAttemptNotModifiedTest() {
    ASSERT_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());

    MakeService();
    group_ = new AppCacheGroup(
        service_.get(), http_server_->TestServerPage("files/notmodified"),
        service_->storage()->NewGroupId());
    AppCacheUpdateJob* update = new AppCacheUpdateJob(service_.get(), group_);
    group_->update_job_ = update;

    MockFrontend* frontend = MakeMockFrontend();
    AppCacheHost* host = MakeHost(1, frontend);
    update->StartUpdate(host, GURL::EmptyGURL());
    EXPECT_TRUE(update->manifest_url_request_ != NULL);

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = false;  // treated like cache failure
    frontend->AddExpectedEvent(MockFrontend::HostIds(1, host->host_id()),
                               CHECKING_EVENT);

    WaitForUpdateToFinish();
  }

  void UpgradeNotModifiedTest() {
    ASSERT_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());

    MakeService();
    group_ = new AppCacheGroup(
        service_.get(), http_server_->TestServerPage("files/notmodified"),
        service_->storage()->NewGroupId());
    AppCacheUpdateJob* update = new AppCacheUpdateJob(service_.get(), group_);
    group_->update_job_ = update;

    AppCache* cache = MakeCacheForGroup(1, 111);
    MockFrontend* frontend1 = MakeMockFrontend();
    MockFrontend* frontend2 = MakeMockFrontend();
    AppCacheHost* host1 = MakeHost(1, frontend1);
    AppCacheHost* host2 = MakeHost(2, frontend2);
    host1->AssociateCache(cache);
    host2->AssociateCache(cache);

    update->StartUpdate(NULL, GURL::EmptyGURL());
    EXPECT_TRUE(update->manifest_url_request_ != NULL);

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = true;
    expect_newest_cache_ = cache;  // newest cache unaffected by update
    MockFrontend::HostIds ids1(1, host1->host_id());
    frontend1->AddExpectedEvent(ids1, CHECKING_EVENT);
    frontend1->AddExpectedEvent(ids1, NO_UPDATE_EVENT);
    MockFrontend::HostIds ids2(1, host2->host_id());
    frontend2->AddExpectedEvent(ids2, CHECKING_EVENT);
    frontend2->AddExpectedEvent(ids2, NO_UPDATE_EVENT);

    WaitForUpdateToFinish();
  }

  void UpgradeManifestDataUnchangedTest() {
    ASSERT_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());

    MakeService();
    group_ = new AppCacheGroup(
        service_.get(), http_server_->TestServerPage("files/manifest1"),
        service_->storage()->NewGroupId());
    AppCacheUpdateJob* update = new AppCacheUpdateJob(service_.get(), group_);
    group_->update_job_ = update;

    // Create response writer to get a response id.
    response_writer_.reset(
        service_->storage()->CreateResponseWriter(group_->manifest_url()));

    AppCache* cache = MakeCacheForGroup(1, response_writer_->response_id());
    MockFrontend* frontend1 = MakeMockFrontend();
    MockFrontend* frontend2 = MakeMockFrontend();
    AppCacheHost* host1 = MakeHost(1, frontend1);
    AppCacheHost* host2 = MakeHost(2, frontend2);
    host1->AssociateCache(cache);
    host2->AssociateCache(cache);

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = true;
    expect_newest_cache_ = cache;  // newest cache unaffected by update
    MockFrontend::HostIds ids1(1, host1->host_id());
    frontend1->AddExpectedEvent(ids1, CHECKING_EVENT);
    frontend1->AddExpectedEvent(ids1, NO_UPDATE_EVENT);
    MockFrontend::HostIds ids2(1, host2->host_id());
    frontend2->AddExpectedEvent(ids2, CHECKING_EVENT);
    frontend2->AddExpectedEvent(ids2, NO_UPDATE_EVENT);

    // Seed storage with expected manifest data.
    const std::string seed_data(
        "CACHE MANIFEST\n"
        "explicit1\n"
        "FALLBACK:\n"
        "fallback1 fallback1a\n"
        "NETWORK:\n"
        "*\n");
    scoped_refptr<net::StringIOBuffer> io_buffer =
        new net::StringIOBuffer(seed_data);
    write_callback_.reset(
        new net::CompletionCallbackImpl<AppCacheUpdateJobTest>(this,
            &AppCacheUpdateJobTest::StartUpdateAfterSeedingManifestData));
    response_writer_->WriteData(io_buffer, seed_data.length(),
                                write_callback_.get());

    // Start update after data write completes asynchronously.
  }

  void StartUpdateAfterSeedingManifestData(int result) {
    ASSERT_GT(result, 0);
    write_callback_.reset();
    response_writer_.reset();

    AppCacheUpdateJob* update = group_->update_job_;
    update->StartUpdate(NULL, GURL::EmptyGURL());
    EXPECT_TRUE(update->manifest_url_request_ != NULL);

    WaitForUpdateToFinish();
  }

  void BasicCacheAttemptSuccessTest() {
    ASSERT_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());

    MakeService();
    group_ = new AppCacheGroup(
        service_.get(), http_server_->TestServerPage("files/manifest1"),
        service_->storage()->NewGroupId());
    AppCacheUpdateJob* update = new AppCacheUpdateJob(service_.get(), group_);
    group_->update_job_ = update;

    MockFrontend* frontend = MakeMockFrontend();
    AppCacheHost* host = MakeHost(1, frontend);
    update->StartUpdate(host, GURL::EmptyGURL());
    EXPECT_TRUE(update->manifest_url_request_ != NULL);

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = true;
    tested_manifest_ = MANIFEST1;
    frontend->AddExpectedEvent(MockFrontend::HostIds(1, host->host_id()),
                               CHECKING_EVENT);

    WaitForUpdateToFinish();
  }

  void BasicUpgradeSuccessTest() {
    ASSERT_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());

    MakeService();
    group_ = new AppCacheGroup(
        service_.get(), http_server_->TestServerPage("files/manifest1"),
        service_->storage()->NewGroupId());
    AppCacheUpdateJob* update = new AppCacheUpdateJob(service_.get(), group_);
    group_->update_job_ = update;

    // Create a response writer to get a response id.
    response_writer_.reset(
        service_->storage()->CreateResponseWriter(group_->manifest_url()));

    AppCache* cache = MakeCacheForGroup(service_->storage()->NewCacheId(),
                                        response_writer_->response_id());
    MockFrontend* frontend1 = MakeMockFrontend();
    MockFrontend* frontend2 = MakeMockFrontend();
    AppCacheHost* host1 = MakeHost(1, frontend1);
    AppCacheHost* host2 = MakeHost(2, frontend2);
    host1->AssociateCache(cache);
    host2->AssociateCache(cache);

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = true;
    expect_old_cache_ = cache;
    tested_manifest_ = MANIFEST1;
    MockFrontend::HostIds ids1(1, host1->host_id());
    frontend1->AddExpectedEvent(ids1, CHECKING_EVENT);
    frontend1->AddExpectedEvent(ids1, DOWNLOADING_EVENT);
    frontend1->AddExpectedEvent(ids1, PROGRESS_EVENT);
    frontend1->AddExpectedEvent(ids1, PROGRESS_EVENT);
    frontend1->AddExpectedEvent(ids1, UPDATE_READY_EVENT);
    MockFrontend::HostIds ids2(1, host2->host_id());
    frontend2->AddExpectedEvent(ids2, CHECKING_EVENT);
    frontend2->AddExpectedEvent(ids2, DOWNLOADING_EVENT);
    frontend2->AddExpectedEvent(ids2, PROGRESS_EVENT);
    frontend2->AddExpectedEvent(ids2, PROGRESS_EVENT);
    frontend2->AddExpectedEvent(ids2, UPDATE_READY_EVENT);

    // Seed storage with expected manifest data different from manifest1.
    const std::string seed_data("different");
    scoped_refptr<net::StringIOBuffer> io_buffer =
        new net::StringIOBuffer(seed_data);
    write_callback_.reset(
        new net::CompletionCallbackImpl<AppCacheUpdateJobTest>(this,
            &AppCacheUpdateJobTest::StartUpdateAfterSeedingManifestData));
    response_writer_->WriteData(io_buffer, seed_data.length(),
                                write_callback_.get());

    // Start update after data write completes asynchronously.
  }

  void UpgradeSuccessMergedTypesTest() {
    ASSERT_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());

    MakeService();
    group_ = new AppCacheGroup(service_.get(),
        http_server_->TestServerPage("files/manifest-merged-types"),
        service_->storage()->NewGroupId());
    AppCacheUpdateJob* update = new AppCacheUpdateJob(service_.get(), group_);
    group_->update_job_ = update;

    AppCache* cache = MakeCacheForGroup(service_->storage()->NewCacheId(), 42);
    MockFrontend* frontend1 = MakeMockFrontend();
    MockFrontend* frontend2 = MakeMockFrontend();
    AppCacheHost* host1 = MakeHost(1, frontend1);
    AppCacheHost* host2 = MakeHost(2, frontend2);
    host1->AssociateCache(cache);
    host2->AssociateCache(cache);

    // Give the newest cache a master entry that is also one of the explicit
    // entries in the manifest.
    cache->AddEntry(http_server_->TestServerPage("files/explicit1"),
                    AppCacheEntry(AppCacheEntry::MASTER, 111));

    update->StartUpdate(NULL, GURL::EmptyGURL());
    EXPECT_TRUE(update->manifest_url_request_ != NULL);

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = true;
    expect_old_cache_ = cache;
    tested_manifest_ = MANIFEST_MERGED_TYPES;
    MockFrontend::HostIds ids1(1, host1->host_id());
    frontend1->AddExpectedEvent(ids1, CHECKING_EVENT);
    frontend1->AddExpectedEvent(ids1, DOWNLOADING_EVENT);
    frontend1->AddExpectedEvent(ids1, PROGRESS_EVENT);  // explicit1 (load)
    frontend1->AddExpectedEvent(ids1, PROGRESS_EVENT);  // manifest (load)
    frontend1->AddExpectedEvent(ids1, PROGRESS_EVENT);  // explicit1 (fetch)
    frontend1->AddExpectedEvent(ids1, PROGRESS_EVENT);  // manifest (fetch)
    frontend1->AddExpectedEvent(ids1, UPDATE_READY_EVENT);
    MockFrontend::HostIds ids2(1, host2->host_id());
    frontend2->AddExpectedEvent(ids2, CHECKING_EVENT);
    frontend2->AddExpectedEvent(ids2, DOWNLOADING_EVENT);
    frontend2->AddExpectedEvent(ids2, PROGRESS_EVENT);
    frontend2->AddExpectedEvent(ids2, PROGRESS_EVENT);
    frontend2->AddExpectedEvent(ids2, PROGRESS_EVENT);
    frontend2->AddExpectedEvent(ids2, PROGRESS_EVENT);
    frontend2->AddExpectedEvent(ids2, UPDATE_READY_EVENT);

    WaitForUpdateToFinish();
  }

  void CacheAttemptFailUrlFetchTest() {
    ASSERT_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());

    MakeService();
    group_ = new AppCacheGroup(service_.get(),
        http_server_->TestServerPage("files/manifest-with-404"),
        service_->storage()->NewGroupId());
    AppCacheUpdateJob* update = new AppCacheUpdateJob(service_.get(), group_);
    group_->update_job_ = update;

    MockFrontend* frontend = MakeMockFrontend();
    AppCacheHost* host = MakeHost(1, frontend);
    update->StartUpdate(host, GURL::EmptyGURL());
    EXPECT_TRUE(update->manifest_url_request_ != NULL);

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = false;  // 404 explicit url is cache failure
    frontend->AddExpectedEvent(MockFrontend::HostIds(1, host->host_id()),
                               CHECKING_EVENT);

    WaitForUpdateToFinish();
  }

  void UpgradeFailUrlFetchTest() {
    ASSERT_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());

    MakeService();
    group_ = new AppCacheGroup(service_.get(),
        http_server_->TestServerPage("files/manifest-fb-404"),
        service_->storage()->NewGroupId());
    AppCacheUpdateJob* update = new AppCacheUpdateJob(service_.get(), group_);
    group_->update_job_ = update;

    AppCache* cache = MakeCacheForGroup(service_->storage()->NewCacheId(), 99);
    MockFrontend* frontend1 = MakeMockFrontend();
    MockFrontend* frontend2 = MakeMockFrontend();
    AppCacheHost* host1 = MakeHost(1, frontend1);
    AppCacheHost* host2 = MakeHost(2, frontend2);
    host1->AssociateCache(cache);
    host2->AssociateCache(cache);

    update->StartUpdate(NULL, GURL::EmptyGURL());
    EXPECT_TRUE(update->manifest_url_request_ != NULL);

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = true;
    expect_newest_cache_ = cache;  // newest cache unaffectd by failed update
    MockFrontend::HostIds ids1(1, host1->host_id());
    frontend1->AddExpectedEvent(ids1, CHECKING_EVENT);
    frontend1->AddExpectedEvent(ids1, DOWNLOADING_EVENT);
    frontend1->AddExpectedEvent(ids1, PROGRESS_EVENT);
    frontend1->AddExpectedEvent(ids1, PROGRESS_EVENT);
    frontend1->AddExpectedEvent(ids1, PROGRESS_EVENT);
    frontend1->AddExpectedEvent(ids1, ERROR_EVENT);
    MockFrontend::HostIds ids2(1, host2->host_id());
    frontend2->AddExpectedEvent(ids2, CHECKING_EVENT);
    frontend2->AddExpectedEvent(ids2, DOWNLOADING_EVENT);
    frontend2->AddExpectedEvent(ids2, PROGRESS_EVENT);
    frontend2->AddExpectedEvent(ids2, PROGRESS_EVENT);
    frontend2->AddExpectedEvent(ids2, PROGRESS_EVENT);
    frontend2->AddExpectedEvent(ids2, ERROR_EVENT);

    WaitForUpdateToFinish();
  }

  void UpgradeFailMasterUrlFetchTest() {
    ASSERT_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());

    MakeService();
    group_ = new AppCacheGroup(
        service_.get(), http_server_->TestServerPage("files/manifest1"),
        service_->storage()->NewGroupId());
    AppCacheUpdateJob* update = new AppCacheUpdateJob(service_.get(), group_);
    group_->update_job_ = update;

    AppCache* cache = MakeCacheForGroup(service_->storage()->NewCacheId(), 25);
    MockFrontend* frontend1 = MakeMockFrontend();
    MockFrontend* frontend2 = MakeMockFrontend();
    AppCacheHost* host1 = MakeHost(1, frontend1);
    AppCacheHost* host2 = MakeHost(2, frontend2);
    host1->AssociateCache(cache);
    host2->AssociateCache(cache);

    // Give the newest cache some master entries; one will fail with a 404.
    cache->AddEntry(
        http_server_->TestServerPage("files/notfound"),
        AppCacheEntry(AppCacheEntry::MASTER, 222));
    cache->AddEntry(
        http_server_->TestServerPage("files/explicit2"),
        AppCacheEntry(AppCacheEntry::MASTER | AppCacheEntry::FOREIGN, 333));
    cache->AddEntry(
        http_server_->TestServerPage("files/servererror"),
        AppCacheEntry(AppCacheEntry::MASTER, 444));

    update->StartUpdate(NULL, GURL::EmptyGURL());
    EXPECT_TRUE(update->manifest_url_request_ != NULL);

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = true;
    expect_old_cache_ = cache;
    tested_manifest_ = MANIFEST1;
    expect_extra_entries_.insert(AppCache::EntryMap::value_type(
        http_server_->TestServerPage("files/explicit2"),
        AppCacheEntry(AppCacheEntry::MASTER)));  // foreign flag is dropped
    expect_extra_entries_.insert(AppCache::EntryMap::value_type(
        http_server_->TestServerPage("files/servererror"),
        AppCacheEntry(AppCacheEntry::MASTER)));
    MockFrontend::HostIds ids1(1, host1->host_id());
    frontend1->AddExpectedEvent(ids1, CHECKING_EVENT);
    frontend1->AddExpectedEvent(ids1, DOWNLOADING_EVENT);
    frontend1->AddExpectedEvent(ids1, PROGRESS_EVENT);  // explicit1
    frontend1->AddExpectedEvent(ids1, PROGRESS_EVENT);  // fallback1a
    frontend1->AddExpectedEvent(ids1, PROGRESS_EVENT);  // notfound (load)
    frontend1->AddExpectedEvent(ids1, PROGRESS_EVENT);  // notfound (fetch)
    frontend1->AddExpectedEvent(ids1, PROGRESS_EVENT);  // explicit2 (load)
    frontend1->AddExpectedEvent(ids1, PROGRESS_EVENT);  // explicit2 (fetch)
    frontend1->AddExpectedEvent(ids1, PROGRESS_EVENT);  // servererror (load)
    frontend1->AddExpectedEvent(ids1, PROGRESS_EVENT);  // servererror (fetch)
    frontend1->AddExpectedEvent(ids1, UPDATE_READY_EVENT);
    MockFrontend::HostIds ids2(1, host2->host_id());
    frontend2->AddExpectedEvent(ids2, CHECKING_EVENT);
    frontend2->AddExpectedEvent(ids2, DOWNLOADING_EVENT);
    frontend2->AddExpectedEvent(ids2, PROGRESS_EVENT);
    frontend2->AddExpectedEvent(ids2, PROGRESS_EVENT);
    frontend2->AddExpectedEvent(ids2, PROGRESS_EVENT);
    frontend2->AddExpectedEvent(ids2, PROGRESS_EVENT);
    frontend2->AddExpectedEvent(ids2, PROGRESS_EVENT);
    frontend2->AddExpectedEvent(ids2, PROGRESS_EVENT);
    frontend2->AddExpectedEvent(ids2, PROGRESS_EVENT);
    frontend2->AddExpectedEvent(ids2, PROGRESS_EVENT);
    frontend2->AddExpectedEvent(ids2, UPDATE_READY_EVENT);

    WaitForUpdateToFinish();
  }

  void EmptyManifestTest() {
    ASSERT_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());

    MakeService();
    group_ = new AppCacheGroup(
        service_.get(), http_server_->TestServerPage("files/empty-manifest"),
        service_->storage()->NewGroupId());
    AppCacheUpdateJob* update = new AppCacheUpdateJob(service_.get(), group_);
    group_->update_job_ = update;

    AppCache* cache = MakeCacheForGroup(service_->storage()->NewCacheId(), 33);
    MockFrontend* frontend1 = MakeMockFrontend();
    MockFrontend* frontend2 = MakeMockFrontend();
    AppCacheHost* host1 = MakeHost(1, frontend1);
    AppCacheHost* host2 = MakeHost(2, frontend2);
    host1->AssociateCache(cache);
    host2->AssociateCache(cache);

    update->StartUpdate(NULL, GURL::EmptyGURL());
    EXPECT_TRUE(update->manifest_url_request_ != NULL);

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = true;
    expect_old_cache_ = cache;
    tested_manifest_ = EMPTY_MANIFEST;
    MockFrontend::HostIds ids1(1, host1->host_id());
    frontend1->AddExpectedEvent(ids1, CHECKING_EVENT);
    frontend1->AddExpectedEvent(ids1, DOWNLOADING_EVENT);
    frontend1->AddExpectedEvent(ids1, UPDATE_READY_EVENT);
    MockFrontend::HostIds ids2(1, host2->host_id());
    frontend2->AddExpectedEvent(ids2, CHECKING_EVENT);
    frontend2->AddExpectedEvent(ids2, DOWNLOADING_EVENT);
    frontend2->AddExpectedEvent(ids2, UPDATE_READY_EVENT);

    WaitForUpdateToFinish();
  }

  void EmptyFileTest() {
    ASSERT_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());

    MakeService();
    group_ = new AppCacheGroup(service_.get(),
        http_server_->TestServerPage("files/empty-file-manifest"),
        service_->storage()->NewGroupId());
    AppCacheUpdateJob* update = new AppCacheUpdateJob(service_.get(), group_);
    group_->update_job_ = update;

    AppCache* cache = MakeCacheForGroup(service_->storage()->NewCacheId(), 22);
    MockFrontend* frontend = MakeMockFrontend();
    AppCacheHost* host = MakeHost(1, frontend);
    host->AssociateCache(cache);

    update->StartUpdate(host, GURL::EmptyGURL());
    EXPECT_TRUE(update->manifest_url_request_ != NULL);

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = true;
    tested_manifest_ = EMPTY_FILE_MANIFEST;
    MockFrontend::HostIds ids1(1, host->host_id());
    frontend->AddExpectedEvent(ids1, CHECKING_EVENT);
    frontend->AddExpectedEvent(ids1, DOWNLOADING_EVENT);
    frontend->AddExpectedEvent(ids1, PROGRESS_EVENT);
    frontend->AddExpectedEvent(ids1, UPDATE_READY_EVENT);

    WaitForUpdateToFinish();
  }

  void RetryRequestTest() {
    ASSERT_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());

    // Set some large number of times to return retry.
    // Expect 1 manifest fetch and 3 retries.
    RetryRequestTestJob::Initialize(5, RetryRequestTestJob::RETRY_AFTER_0, 4);
    old_factory_ = URLRequest::RegisterProtocolFactory(
        "http", RetryRequestTestJob::RetryFactory);
    registered_factory_ = true;

    MakeService();
    group_ = new AppCacheGroup(service_.get(), RetryRequestTestJob::kRetryUrl,
                               service_->storage()->NewGroupId());
    AppCacheUpdateJob* update = new AppCacheUpdateJob(service_.get(), group_);
    group_->update_job_ = update;

    MockFrontend* frontend = MakeMockFrontend();
    AppCacheHost* host = MakeHost(1, frontend);
    update->StartUpdate(host, GURL::EmptyGURL());
    EXPECT_TRUE(update->manifest_url_request_ != NULL);

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = false;
    frontend->AddExpectedEvent(MockFrontend::HostIds(1, host->host_id()),
                               CHECKING_EVENT);

    WaitForUpdateToFinish();
  }

  void RetryNoRetryAfterTest() {
    ASSERT_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());

    // Set some large number of times to return retry.
    // Expect 1 manifest fetch and 0 retries.
    RetryRequestTestJob::Initialize(5, RetryRequestTestJob::NO_RETRY_AFTER, 1);
    old_factory_ = URLRequest::RegisterProtocolFactory(
        "http", RetryRequestTestJob::RetryFactory);
    registered_factory_ = true;

    MakeService();
    group_ = new AppCacheGroup(service_.get(), RetryRequestTestJob::kRetryUrl,
                               service_->storage()->NewGroupId());
    AppCacheUpdateJob* update = new AppCacheUpdateJob(service_.get(), group_);
    group_->update_job_ = update;

    MockFrontend* frontend = MakeMockFrontend();
    AppCacheHost* host = MakeHost(1, frontend);
    update->StartUpdate(host, GURL::EmptyGURL());
    EXPECT_TRUE(update->manifest_url_request_ != NULL);

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = false;
    frontend->AddExpectedEvent(MockFrontend::HostIds(1, host->host_id()),
                               CHECKING_EVENT);

    WaitForUpdateToFinish();
  }

  void RetryNonzeroRetryAfterTest() {
    ASSERT_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());

    // Set some large number of times to return retry.
    // Expect 1 request and 0 retry attempts.
    RetryRequestTestJob::Initialize(
        5, RetryRequestTestJob::NONZERO_RETRY_AFTER, 1);
    old_factory_ = URLRequest::RegisterProtocolFactory(
        "http", RetryRequestTestJob::RetryFactory);
    registered_factory_ = true;

    MakeService();
    group_ = new AppCacheGroup(service_.get(), RetryRequestTestJob::kRetryUrl,
                               service_->storage()->NewGroupId());
    AppCacheUpdateJob* update = new AppCacheUpdateJob(service_.get(), group_);
    group_->update_job_ = update;

    MockFrontend* frontend = MakeMockFrontend();
    AppCacheHost* host = MakeHost(1, frontend);
    update->StartUpdate(host, GURL::EmptyGURL());
    EXPECT_TRUE(update->manifest_url_request_ != NULL);

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = false;
    frontend->AddExpectedEvent(MockFrontend::HostIds(1, host->host_id()),
                               CHECKING_EVENT);

    WaitForUpdateToFinish();
  }

  void RetrySuccessTest() {
    ASSERT_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());

    // Set 2 as the retry limit (does not exceed the max).
    // Expect 1 manifest fetch, 2 retries, 1 url fetch, 1 manifest refetch.
    RetryRequestTestJob::Initialize(2, RetryRequestTestJob::RETRY_AFTER_0, 5);
    old_factory_ = URLRequest::RegisterProtocolFactory(
        "http", RetryRequestTestJob::RetryFactory);
    registered_factory_ = true;

    MakeService();
    group_ = new AppCacheGroup(service_.get(), RetryRequestTestJob::kRetryUrl,
                               service_->storage()->NewGroupId());
    AppCacheUpdateJob* update = new AppCacheUpdateJob(service_.get(), group_);
    group_->update_job_ = update;

    MockFrontend* frontend = MakeMockFrontend();
    AppCacheHost* host = MakeHost(1, frontend);
    update->StartUpdate(host, GURL::EmptyGURL());
    EXPECT_TRUE(update->manifest_url_request_ != NULL);

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = true;
    frontend->AddExpectedEvent(MockFrontend::HostIds(1, host->host_id()),
                               CHECKING_EVENT);

    WaitForUpdateToFinish();
  }

  void RetryUrlTest() {
    ASSERT_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());

    // Set 1 as the retry limit (does not exceed the max).
    // Expect 1 manifest fetch, 1 url fetch, 1 url retry, 1 manifest refetch.
    RetryRequestTestJob::Initialize(1, RetryRequestTestJob::RETRY_AFTER_0, 4);
    old_factory_ = URLRequest::RegisterProtocolFactory(
        "http", RetryRequestTestJob::RetryFactory);
    registered_factory_ = true;

    MakeService();
    group_ = new AppCacheGroup(service_.get(), GURL("http://retryurl"),
                               service_->storage()->NewGroupId());
    AppCacheUpdateJob* update = new AppCacheUpdateJob(service_.get(), group_);
    group_->update_job_ = update;

    MockFrontend* frontend = MakeMockFrontend();
    AppCacheHost* host = MakeHost(1, frontend);
    update->StartUpdate(host, GURL::EmptyGURL());
    EXPECT_TRUE(update->manifest_url_request_ != NULL);

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = true;
    frontend->AddExpectedEvent(MockFrontend::HostIds(1, host->host_id()),
                               CHECKING_EVENT);

    WaitForUpdateToFinish();
  }

  void FailStoreNewestCacheTest() {
    ASSERT_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());

    MakeService();
    MockAppCacheStorage* storage =
        reinterpret_cast<MockAppCacheStorage*>(service_->storage());
    storage->SimulateStoreGroupAndNewestCacheFailure();

    group_ = new AppCacheGroup(
        service_.get(), http_server_->TestServerPage("files/manifest1"),
        service_->storage()->NewGroupId());
    AppCacheUpdateJob* update = new AppCacheUpdateJob(service_.get(), group_);
    group_->update_job_ = update;

    MockFrontend* frontend = MakeMockFrontend();
    AppCacheHost* host = MakeHost(1, frontend);
    update->StartUpdate(host, GURL::EmptyGURL());

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = false;  // storage failed
    frontend->AddExpectedEvent(MockFrontend::HostIds(1, host->host_id()),
                               CHECKING_EVENT);

    WaitForUpdateToFinish();
  }

  void UpgradeFailStoreNewestCacheTest() {
    ASSERT_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());

    MakeService();
    MockAppCacheStorage* storage =
        reinterpret_cast<MockAppCacheStorage*>(service_->storage());
    storage->SimulateStoreGroupAndNewestCacheFailure();

    group_ = new AppCacheGroup(
        service_.get(), http_server_->TestServerPage("files/manifest1"),
        service_->storage()->NewGroupId());
    AppCacheUpdateJob* update = new AppCacheUpdateJob(service_.get(), group_);
    group_->update_job_ = update;

    AppCache* cache = MakeCacheForGroup(service_->storage()->NewCacheId(), 11);
    MockFrontend* frontend1 = MakeMockFrontend();
    MockFrontend* frontend2 = MakeMockFrontend();
    AppCacheHost* host1 = MakeHost(1, frontend1);
    AppCacheHost* host2 = MakeHost(2, frontend2);
    host1->AssociateCache(cache);
    host2->AssociateCache(cache);

    update->StartUpdate(NULL, GURL::EmptyGURL());

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = true;
    expect_newest_cache_ = cache;  // unchanged
    MockFrontend::HostIds ids1(1, host1->host_id());
    frontend1->AddExpectedEvent(ids1, CHECKING_EVENT);
    frontend1->AddExpectedEvent(ids1, DOWNLOADING_EVENT);
    frontend1->AddExpectedEvent(ids1, PROGRESS_EVENT);
    frontend1->AddExpectedEvent(ids1, PROGRESS_EVENT);
    frontend1->AddExpectedEvent(ids1, ERROR_EVENT);
    MockFrontend::HostIds ids2(1, host2->host_id());
    frontend2->AddExpectedEvent(ids2, CHECKING_EVENT);
    frontend2->AddExpectedEvent(ids2, DOWNLOADING_EVENT);
    frontend2->AddExpectedEvent(ids2, PROGRESS_EVENT);
    frontend2->AddExpectedEvent(ids2, PROGRESS_EVENT);
    frontend2->AddExpectedEvent(ids2, ERROR_EVENT);

    WaitForUpdateToFinish();
  }

  void FailMakeGroupObsoleteTest() {
    ASSERT_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());

    MakeService();
    MockAppCacheStorage* storage =
        reinterpret_cast<MockAppCacheStorage*>(service_->storage());
    storage->SimulateMakeGroupObsoleteFailure();

    group_ = new AppCacheGroup(
        service_.get(), http_server_->TestServerPage("files/gone"),
        service_->storage()->NewGroupId());
    AppCacheUpdateJob* update = new AppCacheUpdateJob(service_.get(), group_);
    group_->update_job_ = update;

    MockFrontend* frontend = MakeMockFrontend();
    AppCacheHost* host = MakeHost(1, frontend);
    update->StartUpdate(host, GURL::EmptyGURL());
    EXPECT_TRUE(update->manifest_url_request_ != NULL);

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = false;
    frontend->AddExpectedEvent(MockFrontend::HostIds(1, host->host_id()),
                               CHECKING_EVENT);

    WaitForUpdateToFinish();
  }

  void UpgradeFailMakeGroupObsoleteTest() {
    ASSERT_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());

    MakeService();
    MockAppCacheStorage* storage =
        reinterpret_cast<MockAppCacheStorage*>(service_->storage());
    storage->SimulateMakeGroupObsoleteFailure();

    group_ = new AppCacheGroup(
        service_.get(), http_server_->TestServerPage("files/nosuchfile"),
        service_->storage()->NewGroupId());
    AppCacheUpdateJob* update = new AppCacheUpdateJob(service_.get(), group_);
    group_->update_job_ = update;

    AppCache* cache = MakeCacheForGroup(1, 111);
    MockFrontend* frontend1 = MakeMockFrontend();
    MockFrontend* frontend2 = MakeMockFrontend();
    AppCacheHost* host1 = MakeHost(1, frontend1);
    AppCacheHost* host2 = MakeHost(2, frontend2);
    host1->AssociateCache(cache);
    host2->AssociateCache(cache);

    update->StartUpdate(NULL, GURL::EmptyGURL());
    EXPECT_TRUE(update->manifest_url_request_ != NULL);

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = true;
    expect_newest_cache_ = cache;  // newest cache unaffected by update
    MockFrontend::HostIds ids1(1, host1->host_id());
    frontend1->AddExpectedEvent(ids1, CHECKING_EVENT);
    frontend1->AddExpectedEvent(ids1, ERROR_EVENT);
    MockFrontend::HostIds ids2(1, host2->host_id());
    frontend2->AddExpectedEvent(ids2, CHECKING_EVENT);
    frontend2->AddExpectedEvent(ids2, ERROR_EVENT);

    WaitForUpdateToFinish();
  }

  void MasterEntryFetchManifestFailTest() {
    ASSERT_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());

    MakeService();
    group_ = new AppCacheGroup(service_.get(), GURL("http://failme"), 111);
    AppCacheUpdateJob* update = new AppCacheUpdateJob(service_.get(), group_);
    group_->update_job_ = update;

    MockFrontend* frontend = MakeMockFrontend();
    AppCacheHost* host = MakeHost(1, frontend);
    host->new_master_entry_url_ = GURL("http://failme/blah");
    update->StartUpdate(host, host->new_master_entry_url_);
    EXPECT_TRUE(update->manifest_url_request_ != NULL);

    update->manifest_url_request_->SimulateError(-100);

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = false;
    MockFrontend::HostIds ids1(1, host->host_id());
    frontend->AddExpectedEvent(ids1, CHECKING_EVENT);
    frontend->AddExpectedEvent(ids1, ERROR_EVENT);

    WaitForUpdateToFinish();
  }

  void MasterEntryBadManifestTest() {
    ASSERT_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());

    MakeService();
    group_ = new AppCacheGroup(service_.get(),
        http_server_->TestServerPage("files/bad-manifest"), 111);
    AppCacheUpdateJob* update = new AppCacheUpdateJob(service_.get(), group_);
    group_->update_job_ = update;

    MockFrontend* frontend = MakeMockFrontend();
    AppCacheHost* host = MakeHost(1, frontend);
    host->new_master_entry_url_ = http_server_->TestServerPage("files/blah");
    update->StartUpdate(host, host->new_master_entry_url_);
    EXPECT_TRUE(update->manifest_url_request_ != NULL);

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = false;
    MockFrontend::HostIds ids1(1, host->host_id());
    frontend->AddExpectedEvent(ids1, CHECKING_EVENT);
    frontend->AddExpectedEvent(ids1, ERROR_EVENT);

    WaitForUpdateToFinish();
  }

  void MasterEntryManifestNotFoundTest() {
    ASSERT_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());

    MakeService();
    group_ = new AppCacheGroup(
        service_.get(), http_server_->TestServerPage("files/nosuchfile"), 111);
    AppCacheUpdateJob* update = new AppCacheUpdateJob(service_.get(), group_);
    group_->update_job_ = update;

    MockFrontend* frontend = MakeMockFrontend();
    AppCacheHost* host = MakeHost(1, frontend);
    host->new_master_entry_url_ = http_server_->TestServerPage("files/blah");

    update->StartUpdate(host, host->new_master_entry_url_);
    EXPECT_TRUE(update->manifest_url_request_ != NULL);

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = true;
    expect_group_has_cache_ = false;
    MockFrontend::HostIds ids1(1, host->host_id());
    frontend->AddExpectedEvent(ids1, CHECKING_EVENT);
    frontend->AddExpectedEvent(ids1, ERROR_EVENT);

    WaitForUpdateToFinish();
  }

  void MasterEntryFailUrlFetchTest() {
    ASSERT_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());

    MakeService();
    group_ = new AppCacheGroup(service_.get(),
        http_server_->TestServerPage("files/manifest-fb-404"), 111);
    AppCacheUpdateJob* update = new AppCacheUpdateJob(service_.get(), group_);
    group_->update_job_ = update;

    MockFrontend* frontend = MakeMockFrontend();
    AppCacheHost* host = MakeHost(1, frontend);
    host->new_master_entry_url_ =
        http_server_->TestServerPage("files/explicit1");

    update->StartUpdate(host, host->new_master_entry_url_);
    EXPECT_TRUE(update->manifest_url_request_ != NULL);

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = false;  // 404 fallback url is cache failure
    MockFrontend::HostIds ids1(1, host->host_id());
    frontend->AddExpectedEvent(ids1, CHECKING_EVENT);
    frontend->AddExpectedEvent(ids1, DOWNLOADING_EVENT);
    frontend->AddExpectedEvent(ids1, PROGRESS_EVENT);
    frontend->AddExpectedEvent(ids1, PROGRESS_EVENT);
    frontend->AddExpectedEvent(ids1, PROGRESS_EVENT);
    frontend->AddExpectedEvent(ids1, ERROR_EVENT);

    WaitForUpdateToFinish();
  }

  void MasterEntryAllFailTest() {
    ASSERT_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());

    MakeService();
    group_ = new AppCacheGroup(
        service_.get(), http_server_->TestServerPage("files/manifest1"), 111);
    AppCacheUpdateJob* update = new AppCacheUpdateJob(service_.get(), group_);
    group_->update_job_ = update;

    MockFrontend* frontend1 = MakeMockFrontend();
    AppCacheHost* host1 = MakeHost(1, frontend1);
    host1->new_master_entry_url_ =
        http_server_->TestServerPage("files/nosuchfile");
    update->StartUpdate(host1, host1->new_master_entry_url_);

    MockFrontend* frontend2 = MakeMockFrontend();
    AppCacheHost* host2 = MakeHost(2, frontend2);
    host2->new_master_entry_url_ =
        http_server_->TestServerPage("files/servererror");
    update->StartUpdate(host2, host2->new_master_entry_url_);

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = false;  // all pending masters failed
    MockFrontend::HostIds ids1(1, host1->host_id());
    frontend1->AddExpectedEvent(ids1, CHECKING_EVENT);
    frontend1->AddExpectedEvent(ids1, DOWNLOADING_EVENT);
    frontend1->AddExpectedEvent(ids1, PROGRESS_EVENT);
    frontend1->AddExpectedEvent(ids1, PROGRESS_EVENT);
    frontend1->AddExpectedEvent(ids1, ERROR_EVENT);
    MockFrontend::HostIds ids2(1, host2->host_id());
    frontend2->AddExpectedEvent(ids2, CHECKING_EVENT);
    frontend2->AddExpectedEvent(ids2, DOWNLOADING_EVENT);
    frontend2->AddExpectedEvent(ids2, PROGRESS_EVENT);
    frontend2->AddExpectedEvent(ids2, PROGRESS_EVENT);
    frontend2->AddExpectedEvent(ids2, ERROR_EVENT);

    WaitForUpdateToFinish();
  }

  void UpgradeMasterEntryAllFailTest() {
    ASSERT_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());

    MakeService();
    group_ = new AppCacheGroup(
        service_.get(), http_server_->TestServerPage("files/manifest1"), 111);
    AppCacheUpdateJob* update = new AppCacheUpdateJob(service_.get(), group_);
    group_->update_job_ = update;

    AppCache* cache = MakeCacheForGroup(service_->storage()->NewCacheId(), 42);
    MockFrontend* frontend1 = MakeMockFrontend();
    AppCacheHost* host1 = MakeHost(1, frontend1);
    host1->AssociateCache(cache);

    MockFrontend* frontend2 = MakeMockFrontend();
    AppCacheHost* host2 = MakeHost(2, frontend2);
    host2->new_master_entry_url_ =
        http_server_->TestServerPage("files/nosuchfile");
    update->StartUpdate(host2, host2->new_master_entry_url_);

    MockFrontend* frontend3 = MakeMockFrontend();
    AppCacheHost* host3 = MakeHost(3, frontend3);
    host3->new_master_entry_url_ =
        http_server_->TestServerPage("files/servererror");
    update->StartUpdate(host3, host3->new_master_entry_url_);

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = true;
    expect_old_cache_ = cache;
    tested_manifest_ = MANIFEST1;
    MockFrontend::HostIds ids1(1, host1->host_id());
    frontend1->AddExpectedEvent(ids1, CHECKING_EVENT);
    frontend1->AddExpectedEvent(ids1, DOWNLOADING_EVENT);
    frontend1->AddExpectedEvent(ids1, PROGRESS_EVENT);
    frontend1->AddExpectedEvent(ids1, PROGRESS_EVENT);
    frontend1->AddExpectedEvent(ids1, UPDATE_READY_EVENT);
    MockFrontend::HostIds ids2(1, host2->host_id());
    frontend2->AddExpectedEvent(ids2, DOWNLOADING_EVENT);
    frontend2->AddExpectedEvent(ids2, PROGRESS_EVENT);
    frontend2->AddExpectedEvent(ids2, PROGRESS_EVENT);
    frontend2->AddExpectedEvent(ids2, ERROR_EVENT);
    MockFrontend::HostIds ids3(1, host3->host_id());
    frontend3->AddExpectedEvent(ids3, CHECKING_EVENT);
    frontend3->AddExpectedEvent(ids3, DOWNLOADING_EVENT);
    frontend3->AddExpectedEvent(ids3, PROGRESS_EVENT);
    frontend3->AddExpectedEvent(ids3, PROGRESS_EVENT);
    frontend3->AddExpectedEvent(ids3, ERROR_EVENT);

    WaitForUpdateToFinish();
  }

  void MasterEntrySomeFailTest() {
    ASSERT_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());

    MakeService();
    group_ = new AppCacheGroup(
        service_.get(), http_server_->TestServerPage("files/manifest1"), 111);
    AppCacheUpdateJob* update = new AppCacheUpdateJob(service_.get(), group_);
    group_->update_job_ = update;

    MockFrontend* frontend1 = MakeMockFrontend();
    AppCacheHost* host1 = MakeHost(1, frontend1);
    host1->new_master_entry_url_ =
        http_server_->TestServerPage("files/nosuchfile");
    update->StartUpdate(host1, host1->new_master_entry_url_);

    MockFrontend* frontend2 = MakeMockFrontend();
    AppCacheHost* host2 = MakeHost(2, frontend2);
    host2->new_master_entry_url_ =
        http_server_->TestServerPage("files/explicit2");
    update->StartUpdate(host2, host2->new_master_entry_url_);

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = true;  // as long as one pending master succeeds
    tested_manifest_ = MANIFEST1;
    expect_extra_entries_.insert(AppCache::EntryMap::value_type(
        http_server_->TestServerPage("files/explicit2"),
        AppCacheEntry(AppCacheEntry::MASTER)));
    MockFrontend::HostIds ids1(1, host1->host_id());
    frontend1->AddExpectedEvent(ids1, CHECKING_EVENT);
    frontend1->AddExpectedEvent(ids1, DOWNLOADING_EVENT);
    frontend1->AddExpectedEvent(ids1, PROGRESS_EVENT);
    frontend1->AddExpectedEvent(ids1, PROGRESS_EVENT);
    frontend1->AddExpectedEvent(ids1, ERROR_EVENT);
    MockFrontend::HostIds ids2(1, host2->host_id());
    frontend2->AddExpectedEvent(ids2, CHECKING_EVENT);
    frontend2->AddExpectedEvent(ids2, DOWNLOADING_EVENT);
    frontend2->AddExpectedEvent(ids2, PROGRESS_EVENT);
    frontend2->AddExpectedEvent(ids2, PROGRESS_EVENT);
    frontend2->AddExpectedEvent(ids2, CACHED_EVENT);

    WaitForUpdateToFinish();
  }

  void UpgradeMasterEntrySomeFailTest() {
    ASSERT_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());

    MakeService();
    group_ = new AppCacheGroup(
        service_.get(), http_server_->TestServerPage("files/manifest1"), 111);
    AppCacheUpdateJob* update = new AppCacheUpdateJob(service_.get(), group_);
    group_->update_job_ = update;

    AppCache* cache = MakeCacheForGroup(service_->storage()->NewCacheId(), 42);
    MockFrontend* frontend1 = MakeMockFrontend();
    AppCacheHost* host1 = MakeHost(1, frontend1);
    host1->AssociateCache(cache);

    MockFrontend* frontend2 = MakeMockFrontend();
    AppCacheHost* host2 = MakeHost(2, frontend2);
    host2->new_master_entry_url_ =
        http_server_->TestServerPage("files/nosuchfile");
    update->StartUpdate(host2, host2->new_master_entry_url_);

    MockFrontend* frontend3 = MakeMockFrontend();
    AppCacheHost* host3 = MakeHost(3, frontend3);
    host3->new_master_entry_url_ =
        http_server_->TestServerPage("files/explicit2");
    update->StartUpdate(host3, host3->new_master_entry_url_);

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = true;
    expect_old_cache_ = cache;
    tested_manifest_ = MANIFEST1;
    expect_extra_entries_.insert(AppCache::EntryMap::value_type(
        http_server_->TestServerPage("files/explicit2"),
        AppCacheEntry(AppCacheEntry::MASTER)));
    MockFrontend::HostIds ids1(1, host1->host_id());
    frontend1->AddExpectedEvent(ids1, CHECKING_EVENT);
    frontend1->AddExpectedEvent(ids1, DOWNLOADING_EVENT);
    frontend1->AddExpectedEvent(ids1, PROGRESS_EVENT);
    frontend1->AddExpectedEvent(ids1, PROGRESS_EVENT);
    frontend1->AddExpectedEvent(ids1, UPDATE_READY_EVENT);
    MockFrontend::HostIds ids2(1, host2->host_id());
    frontend2->AddExpectedEvent(ids2, DOWNLOADING_EVENT);
    frontend2->AddExpectedEvent(ids2, PROGRESS_EVENT);
    frontend2->AddExpectedEvent(ids2, PROGRESS_EVENT);
    frontend2->AddExpectedEvent(ids2, ERROR_EVENT);
    MockFrontend::HostIds ids3(1, host3->host_id());
    frontend3->AddExpectedEvent(ids3, CHECKING_EVENT);
    frontend3->AddExpectedEvent(ids3, DOWNLOADING_EVENT);
    frontend3->AddExpectedEvent(ids3, PROGRESS_EVENT);
    frontend3->AddExpectedEvent(ids3, PROGRESS_EVENT);
    frontend3->AddExpectedEvent(ids3, UPDATE_READY_EVENT);

    WaitForUpdateToFinish();
  }

  void MasterEntryNoUpdateTest() {
    ASSERT_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());

    MakeService();
    group_ = new AppCacheGroup(service_.get(),
        http_server_->TestServerPage("files/notmodified"), 111);
    AppCacheUpdateJob* update = new AppCacheUpdateJob(service_.get(), group_);
    group_->update_job_ = update;

    AppCache* cache = MakeCacheForGroup(1, 111);
    MockFrontend* frontend1 = MakeMockFrontend();
    AppCacheHost* host1 = MakeHost(1, frontend1);
    host1->AssociateCache(cache);

    // Give cache an existing entry that can also be fetched.
    cache->AddEntry(http_server_->TestServerPage("files/explicit2"),
                    AppCacheEntry(AppCacheEntry::EXPLICIT, 222));

    MockFrontend* frontend2 = MakeMockFrontend();
    AppCacheHost* host2 = MakeHost(2, frontend2);
    host2->new_master_entry_url_ =
        http_server_->TestServerPage("files/explicit1");
    update->StartUpdate(host2, host2->new_master_entry_url_);

    AppCacheHost* host3 = MakeHost(3, frontend2);  // same frontend as host2
    host3->new_master_entry_url_ =
        http_server_->TestServerPage("files/explicit2");
    update->StartUpdate(host3, host3->new_master_entry_url_);

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = true;
    expect_newest_cache_ = cache;  // newest cache still the same cache
    tested_manifest_ = PENDING_MASTER_NO_UPDATE;
    MockFrontend::HostIds ids1(1, host1->host_id());
    frontend1->AddExpectedEvent(ids1, CHECKING_EVENT);
    frontend1->AddExpectedEvent(ids1, NO_UPDATE_EVENT);
    MockFrontend::HostIds ids3(1, host3->host_id());
    frontend2->AddExpectedEvent(ids3, CHECKING_EVENT);
    MockFrontend::HostIds ids2and3;
    ids2and3.push_back(host2->host_id());
    ids2and3.push_back(host3->host_id());
    frontend2->AddExpectedEvent(ids2and3, NO_UPDATE_EVENT);

    WaitForUpdateToFinish();
  }

  void StartUpdateMidCacheAttemptTest() {
    ASSERT_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());

    MakeService();
    group_ = new AppCacheGroup(
        service_.get(), http_server_->TestServerPage("files/manifest1"),
        service_->storage()->NewGroupId());
    AppCacheUpdateJob* update = new AppCacheUpdateJob(service_.get(), group_);
    group_->update_job_ = update;

    MockFrontend* frontend1 = MakeMockFrontend();
    AppCacheHost* host1 = MakeHost(1, frontend1);
    host1->new_master_entry_url_ =
        http_server_->TestServerPage("files/explicit2");
    update->StartUpdate(host1, host1->new_master_entry_url_);
    EXPECT_TRUE(update->manifest_url_request_ != NULL);

    // Set up additional updates to be started while update is in progress.
    MockFrontend* frontend2 = MakeMockFrontend();
    AppCacheHost* host2 = MakeHost(2, frontend2);
    host2->new_master_entry_url_ =
        http_server_->TestServerPage("files/nosuchfile");

    MockFrontend* frontend3 = MakeMockFrontend();
    AppCacheHost* host3 = MakeHost(3, frontend3);
    host3->new_master_entry_url_ =
        http_server_->TestServerPage("files/explicit1");

    MockFrontend* frontend4 = MakeMockFrontend();
    AppCacheHost* host4 = MakeHost(4, frontend4);
    host4->new_master_entry_url_ =
        http_server_->TestServerPage("files/explicit2");

    MockFrontend* frontend5 = MakeMockFrontend();
    AppCacheHost* host5 = MakeHost(5, frontend5);  // no master entry url

    frontend1->TriggerAdditionalUpdates(DOWNLOADING_EVENT, update);
    frontend1->AdditionalUpdateHost(host2);  // fetch will fail
    frontend1->AdditionalUpdateHost(host3);  // same as an explicit entry
    frontend1->AdditionalUpdateHost(host4);  // same as another master entry
    frontend1->AdditionalUpdateHost(NULL);   // no host
    frontend1->AdditionalUpdateHost(host5);  // no master entry url

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = true;
    tested_manifest_ = MANIFEST1;
    expect_extra_entries_.insert(AppCache::EntryMap::value_type(
        http_server_->TestServerPage("files/explicit2"),
        AppCacheEntry(AppCacheEntry::MASTER)));
    MockFrontend::HostIds ids1(1, host1->host_id());
    frontend1->AddExpectedEvent(ids1, CHECKING_EVENT);
    frontend1->AddExpectedEvent(ids1, DOWNLOADING_EVENT);
    frontend1->AddExpectedEvent(ids1, PROGRESS_EVENT);
    frontend1->AddExpectedEvent(ids1, PROGRESS_EVENT);
    frontend1->AddExpectedEvent(ids1, CACHED_EVENT);
    MockFrontend::HostIds ids2(1, host2->host_id());
    frontend2->AddExpectedEvent(ids2, CHECKING_EVENT);
    frontend2->AddExpectedEvent(ids2, DOWNLOADING_EVENT);
    frontend2->AddExpectedEvent(ids2, PROGRESS_EVENT);
    frontend2->AddExpectedEvent(ids2, PROGRESS_EVENT);
    frontend2->AddExpectedEvent(ids2, ERROR_EVENT);
    MockFrontend::HostIds ids3(1, host3->host_id());
    frontend3->AddExpectedEvent(ids3, CHECKING_EVENT);
    frontend3->AddExpectedEvent(ids3, DOWNLOADING_EVENT);
    frontend3->AddExpectedEvent(ids3, PROGRESS_EVENT);
    frontend3->AddExpectedEvent(ids3, PROGRESS_EVENT);
    frontend3->AddExpectedEvent(ids3, CACHED_EVENT);
    MockFrontend::HostIds ids4(1, host4->host_id());
    frontend4->AddExpectedEvent(ids4, CHECKING_EVENT);
    frontend4->AddExpectedEvent(ids4, DOWNLOADING_EVENT);
    frontend4->AddExpectedEvent(ids4, PROGRESS_EVENT);
    frontend4->AddExpectedEvent(ids4, PROGRESS_EVENT);
    frontend4->AddExpectedEvent(ids4, CACHED_EVENT);

    // Host 5 is not associated with cache so no progress/cached events.
    MockFrontend::HostIds ids5(1, host5->host_id());
    frontend5->AddExpectedEvent(ids5, CHECKING_EVENT);
    frontend5->AddExpectedEvent(ids5, DOWNLOADING_EVENT);

    WaitForUpdateToFinish();
  }

  void StartUpdateMidNoUpdateTest() {
    ASSERT_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());

    MakeService();
    group_ = new AppCacheGroup(
        service_.get(), http_server_->TestServerPage("files/notmodified"),
        service_->storage()->NewGroupId());
    AppCacheUpdateJob* update = new AppCacheUpdateJob(service_.get(), group_);
    group_->update_job_ = update;

    AppCache* cache = MakeCacheForGroup(1, 111);
    MockFrontend* frontend1 = MakeMockFrontend();
    AppCacheHost* host1 = MakeHost(1, frontend1);
    host1->AssociateCache(cache);

    // Give cache an existing entry.
    cache->AddEntry(http_server_->TestServerPage("files/explicit2"),
                    AppCacheEntry(AppCacheEntry::EXPLICIT, 222));

    // Start update with a pending master entry that will fail to give us an
    // event to trigger other updates.
    MockFrontend* frontend2 = MakeMockFrontend();
    AppCacheHost* host2 = MakeHost(2, frontend2);
    host2->new_master_entry_url_ =
        http_server_->TestServerPage("files/nosuchfile");
    update->StartUpdate(host2, host2->new_master_entry_url_);
    EXPECT_TRUE(update->manifest_url_request_ != NULL);

    // Set up additional updates to be started while update is in progress.
    MockFrontend* frontend3 = MakeMockFrontend();
    AppCacheHost* host3 = MakeHost(3, frontend3);
    host3->new_master_entry_url_ =
        http_server_->TestServerPage("files/explicit1");

    MockFrontend* frontend4 = MakeMockFrontend();
    AppCacheHost* host4 = MakeHost(4, frontend4);  // no master entry url

    MockFrontend* frontend5 = MakeMockFrontend();
    AppCacheHost* host5 = MakeHost(5, frontend5);
    host5->new_master_entry_url_ =
        http_server_->TestServerPage("files/explicit2");  // existing entry

    MockFrontend* frontend6 = MakeMockFrontend();
    AppCacheHost* host6 = MakeHost(6, frontend6);
    host6->new_master_entry_url_ =
        http_server_->TestServerPage("files/explicit1");

    frontend2->TriggerAdditionalUpdates(ERROR_EVENT, update);
    frontend2->AdditionalUpdateHost(host3);
    frontend2->AdditionalUpdateHost(NULL);   // no host
    frontend2->AdditionalUpdateHost(host4);  // no master entry url
    frontend2->AdditionalUpdateHost(host5);  // same as existing cache entry
    frontend2->AdditionalUpdateHost(host6);  // same as another master entry

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = true;
    expect_newest_cache_ = cache;  // newest cache unaffected by update
    tested_manifest_ = PENDING_MASTER_NO_UPDATE;
    MockFrontend::HostIds ids1(1, host1->host_id());  // prior associated host
    frontend1->AddExpectedEvent(ids1, CHECKING_EVENT);
    frontend1->AddExpectedEvent(ids1, NO_UPDATE_EVENT);
    MockFrontend::HostIds ids2(1, host2->host_id());
    frontend2->AddExpectedEvent(ids2, ERROR_EVENT);
    MockFrontend::HostIds ids3(1, host3->host_id());
    frontend3->AddExpectedEvent(ids3, CHECKING_EVENT);
    frontend3->AddExpectedEvent(ids3, NO_UPDATE_EVENT);
    MockFrontend::HostIds ids4(1, host4->host_id());  // unassociated w/cache
    frontend4->AddExpectedEvent(ids4, CHECKING_EVENT);
    MockFrontend::HostIds ids5(1, host5->host_id());
    frontend5->AddExpectedEvent(ids5, CHECKING_EVENT);
    frontend5->AddExpectedEvent(ids5, NO_UPDATE_EVENT);
    MockFrontend::HostIds ids6(1, host6->host_id());
    frontend6->AddExpectedEvent(ids6, CHECKING_EVENT);
    frontend6->AddExpectedEvent(ids6, NO_UPDATE_EVENT);

    WaitForUpdateToFinish();
  }

  void StartUpdateMidDownloadTest() {
    ASSERT_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());

    MakeService();
    group_ = new AppCacheGroup(
        service_.get(), http_server_->TestServerPage("files/manifest1"), 111);
    AppCacheUpdateJob* update = new AppCacheUpdateJob(service_.get(), group_);
    group_->update_job_ = update;

    AppCache* cache = MakeCacheForGroup(service_->storage()->NewCacheId(), 42);
    MockFrontend* frontend1 = MakeMockFrontend();
    AppCacheHost* host1 = MakeHost(1, frontend1);
    host1->AssociateCache(cache);

    update->StartUpdate(NULL, GURL::EmptyGURL());

    // Set up additional updates to be started while update is in progress.
    MockFrontend* frontend2 = MakeMockFrontend();
    AppCacheHost* host2 = MakeHost(2, frontend2);
    host2->new_master_entry_url_ =
        http_server_->TestServerPage("files/explicit1");

    MockFrontend* frontend3 = MakeMockFrontend();
    AppCacheHost* host3 = MakeHost(3, frontend3);
    host3->new_master_entry_url_ =
        http_server_->TestServerPage("files/explicit2");

    MockFrontend* frontend4 = MakeMockFrontend();
    AppCacheHost* host4 = MakeHost(4, frontend4);  // no master entry url

    MockFrontend* frontend5 = MakeMockFrontend();
    AppCacheHost* host5 = MakeHost(5, frontend5);
    host5->new_master_entry_url_ =
        http_server_->TestServerPage("files/explicit2");

    frontend1->TriggerAdditionalUpdates(PROGRESS_EVENT, update);
    frontend1->AdditionalUpdateHost(host2);  // same as entry in manifest
    frontend1->AdditionalUpdateHost(NULL);   // no host
    frontend1->AdditionalUpdateHost(host3);  // new master entry
    frontend1->AdditionalUpdateHost(host4);  // no master entry url
    frontend1->AdditionalUpdateHost(host5);  // same as another master entry

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = true;
    tested_manifest_ = MANIFEST1;
    expect_extra_entries_.insert(AppCache::EntryMap::value_type(
        http_server_->TestServerPage("files/explicit2"),
        AppCacheEntry(AppCacheEntry::MASTER)));
    MockFrontend::HostIds ids1(1, host1->host_id());  // prior associated host
    frontend1->AddExpectedEvent(ids1, CHECKING_EVENT);
    frontend1->AddExpectedEvent(ids1, DOWNLOADING_EVENT);
    frontend1->AddExpectedEvent(ids1, PROGRESS_EVENT);
    frontend1->AddExpectedEvent(ids1, PROGRESS_EVENT);
    frontend1->AddExpectedEvent(ids1, UPDATE_READY_EVENT);
    MockFrontend::HostIds ids2(1, host2->host_id());
    frontend2->AddExpectedEvent(ids2, CHECKING_EVENT);
    frontend2->AddExpectedEvent(ids2, DOWNLOADING_EVENT);
    frontend2->AddExpectedEvent(ids2, PROGRESS_EVENT);
    frontend2->AddExpectedEvent(ids2, UPDATE_READY_EVENT);
    MockFrontend::HostIds ids3(1, host3->host_id());
    frontend3->AddExpectedEvent(ids3, CHECKING_EVENT);
    frontend3->AddExpectedEvent(ids3, DOWNLOADING_EVENT);
    frontend3->AddExpectedEvent(ids3, PROGRESS_EVENT);
    frontend3->AddExpectedEvent(ids3, UPDATE_READY_EVENT);
    MockFrontend::HostIds ids4(1, host4->host_id());  // unassociated w/cache
    frontend4->AddExpectedEvent(ids4, CHECKING_EVENT);
    frontend4->AddExpectedEvent(ids4, DOWNLOADING_EVENT);
    MockFrontend::HostIds ids5(1, host5->host_id());
    frontend5->AddExpectedEvent(ids5, CHECKING_EVENT);
    frontend5->AddExpectedEvent(ids5, DOWNLOADING_EVENT);
    frontend5->AddExpectedEvent(ids5, PROGRESS_EVENT);
    frontend5->AddExpectedEvent(ids5, UPDATE_READY_EVENT);

    WaitForUpdateToFinish();
  }

  void WaitForUpdateToFinish() {
    if (group_->update_status() == AppCacheGroup::IDLE)
      UpdateFinished();
    else
      group_->AddUpdateObserver(this);
  }

  void OnUpdateComplete(AppCacheGroup* group) {
    ASSERT_EQ(group_, group);
    protect_newest_cache_ = group->newest_complete_cache();
    UpdateFinished();
  }

  void UpdateFinished() {
    // We unwind the stack prior to finishing up to let stack-based objects
    // get deleted.
    MessageLoop::current()->PostTask(FROM_HERE,
        method_factory_.NewRunnableMethod(
            &AppCacheUpdateJobTest::UpdateFinishedUnwound));
  }

  void UpdateFinishedUnwound() {
    EXPECT_EQ(AppCacheGroup::IDLE, group_->update_status());
    EXPECT_TRUE(group_->update_job() == NULL);
    if (do_checks_after_update_finished_)
      VerifyExpectations();

    // Clean up everything that was created on the IO thread.
    protect_newest_cache_ = NULL;
    group_ = NULL;
    STLDeleteContainerPointers(hosts_.begin(), hosts_.end());
    STLDeleteContainerPointers(frontends_.begin(), frontends_.end());
    service_.reset(NULL);
    if (registered_factory_)
      URLRequest::RegisterProtocolFactory("http", old_factory_);

    event_->Signal();
  }

  void MakeService() {
    service_.reset(new MockAppCacheService());
    request_context_ = new TestURLRequestContext();
    service_->set_request_context(request_context_);
  }

  AppCache* MakeCacheForGroup(int64 cache_id, int64 manifest_response_id) {
    AppCache* cache = new AppCache(service_.get(), cache_id);
    cache->set_complete(true);
    cache->set_update_time(base::TimeTicks::Now());
    group_->AddCache(cache);

    // Add manifest entry to cache.
    cache->AddEntry(group_->manifest_url(),
        AppCacheEntry(AppCacheEntry::MANIFEST, manifest_response_id));

    return cache;
  }

  AppCacheHost* MakeHost(int host_id, AppCacheFrontend* frontend) {
    AppCacheHost* host = new AppCacheHost(host_id, frontend, service_.get());
    hosts_.push_back(host);
    return host;
  }

  MockFrontend* MakeMockFrontend() {
    MockFrontend* frontend = new MockFrontend();
    frontends_.push_back(frontend);
    return frontend;
  }

  // Verifies conditions about the group and notifications after an update
  // has finished. Cannot verify update job internals as update is deleted.
  void VerifyExpectations() {
    RetryRequestTestJob::Verify();

    EXPECT_EQ(expect_group_obsolete_, group_->is_obsolete());

    if (expect_group_has_cache_) {
      EXPECT_TRUE(group_->newest_complete_cache() != NULL);
      if (expect_old_cache_) {
        EXPECT_NE(expect_old_cache_, group_->newest_complete_cache());
        EXPECT_TRUE(group_->old_caches().end() !=
            std::find(group_->old_caches().begin(),
                      group_->old_caches().end(), expect_old_cache_));
      }
      if (expect_newest_cache_) {
        EXPECT_EQ(expect_newest_cache_, group_->newest_complete_cache());
        EXPECT_TRUE(group_->old_caches().end() ==
            std::find(group_->old_caches().begin(),
                      group_->old_caches().end(), expect_newest_cache_));
      } else {
        // Tests that don't know which newest cache to expect contain updates
        // that succeed (because the update creates a new cache whose pointer
        // is unknown to the test). Check group and newest cache were stored
        // when update succeeds.
        MockAppCacheStorage* storage =
            reinterpret_cast<MockAppCacheStorage*>(service_->storage());
        EXPECT_TRUE(storage->IsGroupStored(group_));
        EXPECT_TRUE(storage->IsCacheStored(group_->newest_complete_cache()));

        // Check that all entries in the newest cache were stored.
        const AppCache::EntryMap& entries =
            group_->newest_complete_cache()->entries();
        for (AppCache::EntryMap::const_iterator it = entries.begin();
             it != entries.end(); ++it) {
          EXPECT_NE(kNoResponseId, it->second.response_id());
        }
      }
    } else {
      EXPECT_TRUE(group_->newest_complete_cache() == NULL);
    }

    // Check expected events.
    for (size_t i = 0; i < frontends_.size(); ++i) {
      MockFrontend* frontend = frontends_[i];

      MockFrontend::RaisedEvents& expected_events = frontend->expected_events_;
      MockFrontend::RaisedEvents& actual_events = frontend->raised_events_;
      EXPECT_EQ(expected_events.size(), actual_events.size());

      // Check each expected event.
      for (size_t j = 0;
           j < expected_events.size() && j < actual_events.size(); ++j) {
        EXPECT_EQ(expected_events[j].second, actual_events[j].second);

        MockFrontend::HostIds& expected_ids = expected_events[j].first;
        MockFrontend::HostIds& actual_ids = actual_events[j].first;
        EXPECT_EQ(expected_ids.size(), actual_ids.size());

        for (size_t k = 0; k < expected_ids.size(); ++k) {
          int id = expected_ids[k];
          EXPECT_TRUE(std::find(actual_ids.begin(), actual_ids.end(), id) !=
              actual_ids.end());
        }
      }
    }

    // Verify expected cache contents last as some checks are asserts
    // and will abort the test if they fail.
    if (tested_manifest_) {
      AppCache* cache = group_->newest_complete_cache();
      ASSERT_TRUE(cache != NULL);
      EXPECT_EQ(group_, cache->owning_group());
      EXPECT_TRUE(cache->is_complete());

      switch (tested_manifest_) {
        case MANIFEST1:
          VerifyManifest1(cache);
          break;
        case MANIFEST_MERGED_TYPES:
          VerifyManifestMergedTypes(cache);
          break;
        case EMPTY_MANIFEST:
          VerifyEmptyManifest(cache);
          break;
        case EMPTY_FILE_MANIFEST:
          VerifyEmptyFileManifest(cache);
          break;
        case PENDING_MASTER_NO_UPDATE:
          VerifyMasterEntryNoUpdate(cache);
          break;
        case NONE:
        default:
          break;
      }
    }
  }

  void VerifyManifest1(AppCache* cache) {
    size_t expected = 3 + expect_extra_entries_.size();
    EXPECT_EQ(expected, cache->entries().size());
    AppCacheEntry* entry =
        cache->GetEntry(http_server_->TestServerPage("files/manifest1"));
    ASSERT_TRUE(entry);
    EXPECT_EQ(AppCacheEntry::MANIFEST, entry->types());
    entry = cache->GetEntry(http_server_->TestServerPage("files/explicit1"));
    ASSERT_TRUE(entry);
    EXPECT_TRUE(entry->IsExplicit());
    entry = cache->GetEntry(
        http_server_->TestServerPage("files/fallback1a"));
    ASSERT_TRUE(entry);
    EXPECT_EQ(AppCacheEntry::FALLBACK, entry->types());

    for (AppCache::EntryMap::iterator i = expect_extra_entries_.begin();
         i != expect_extra_entries_.end(); ++i) {
      entry = cache->GetEntry(i->first);
      ASSERT_TRUE(entry);
      EXPECT_EQ(i->second.types(), entry->types());
      // TODO(jennb): if copied, check storage id in entry is as expected
    }

    expected = 1;
    EXPECT_EQ(expected, cache->fallback_namespaces_.size());
    EXPECT_TRUE(cache->fallback_namespaces_.end() !=
        std::find(cache->fallback_namespaces_.begin(),
                  cache->fallback_namespaces_.end(),
                  FallbackNamespace(
                      http_server_->TestServerPage("files/fallback1"),
                      http_server_->TestServerPage("files/fallback1a"))));

    EXPECT_TRUE(cache->online_whitelist_namespaces_.empty());
    EXPECT_TRUE(cache->online_whitelist_all_);

    EXPECT_TRUE(cache->update_time_ > base::TimeTicks());
  }

  void VerifyManifestMergedTypes(AppCache* cache) {
    size_t expected = 2;
    EXPECT_EQ(expected, cache->entries().size());
    AppCacheEntry* entry = cache->GetEntry(
        http_server_->TestServerPage("files/manifest-merged-types"));
    ASSERT_TRUE(entry);
    EXPECT_EQ(AppCacheEntry::EXPLICIT | AppCacheEntry::MANIFEST,
              entry->types());
    entry = cache->GetEntry(http_server_->TestServerPage("files/explicit1"));
    ASSERT_TRUE(entry);
    EXPECT_EQ(AppCacheEntry::EXPLICIT | AppCacheEntry::FALLBACK |
        AppCacheEntry::MASTER, entry->types());

    expected = 1;
    EXPECT_EQ(expected, cache->fallback_namespaces_.size());
    EXPECT_TRUE(cache->fallback_namespaces_.end() !=
        std::find(cache->fallback_namespaces_.begin(),
                  cache->fallback_namespaces_.end(),
                  FallbackNamespace(
                      http_server_->TestServerPage("files/fallback1"),
                      http_server_->TestServerPage("files/explicit1"))));

    EXPECT_EQ(expected, cache->online_whitelist_namespaces_.size());
    EXPECT_TRUE(cache->online_whitelist_namespaces_.end() !=
        std::find(cache->online_whitelist_namespaces_.begin(),
                  cache->online_whitelist_namespaces_.end(),
                  http_server_->TestServerPage("files/online1")));
    EXPECT_FALSE(cache->online_whitelist_all_);

    EXPECT_TRUE(cache->update_time_ > base::TimeTicks());
  }

  void VerifyEmptyManifest(AppCache* cache) {
    size_t expected = 1;
    EXPECT_EQ(expected, cache->entries().size());
    AppCacheEntry* entry = cache->GetEntry(
        http_server_->TestServerPage("files/empty-manifest"));
    ASSERT_TRUE(entry);
    EXPECT_EQ(AppCacheEntry::MANIFEST, entry->types());

    EXPECT_TRUE(cache->fallback_namespaces_.empty());
    EXPECT_TRUE(cache->online_whitelist_namespaces_.empty());
    EXPECT_FALSE(cache->online_whitelist_all_);

    EXPECT_TRUE(cache->update_time_ > base::TimeTicks());
  }

  void VerifyEmptyFileManifest(AppCache* cache) {
    EXPECT_EQ(size_t(2), cache->entries().size());
    AppCacheEntry* entry = cache->GetEntry(
        http_server_->TestServerPage("files/empty-file-manifest"));
    ASSERT_TRUE(entry);
    EXPECT_EQ(AppCacheEntry::MANIFEST, entry->types());

    entry = cache->GetEntry(
        http_server_->TestServerPage("files/empty1"));
    ASSERT_TRUE(entry);
    EXPECT_EQ(AppCacheEntry::EXPLICIT, entry->types());
    EXPECT_TRUE(entry->has_response_id());

    EXPECT_TRUE(cache->fallback_namespaces_.empty());
    EXPECT_TRUE(cache->online_whitelist_namespaces_.empty());
    EXPECT_FALSE(cache->online_whitelist_all_);

    EXPECT_TRUE(cache->update_time_ > base::TimeTicks());
  }

  void VerifyMasterEntryNoUpdate(AppCache* cache) {
    EXPECT_EQ(size_t(3), cache->entries().size());
    AppCacheEntry* entry = cache->GetEntry(
        http_server_->TestServerPage("files/notmodified"));
    ASSERT_TRUE(entry);
    EXPECT_EQ(AppCacheEntry::MANIFEST, entry->types());

    entry = cache->GetEntry(
        http_server_->TestServerPage("files/explicit1"));
    ASSERT_TRUE(entry);
    EXPECT_EQ(AppCacheEntry::MASTER, entry->types());
    EXPECT_TRUE(entry->has_response_id());

    entry = cache->GetEntry(
        http_server_->TestServerPage("files/explicit2"));
    ASSERT_TRUE(entry);
    EXPECT_EQ(AppCacheEntry::EXPLICIT | AppCacheEntry::MASTER, entry->types());
    EXPECT_TRUE(entry->has_response_id());

    EXPECT_TRUE(cache->fallback_namespaces_.empty());
    EXPECT_TRUE(cache->online_whitelist_namespaces_.empty());
    EXPECT_FALSE(cache->online_whitelist_all_);

    EXPECT_TRUE(cache->update_time_ > base::TimeTicks());
  }

 private:
  // Various manifest files used in this test.
  enum TestedManifest {
    NONE,
    MANIFEST1,
    MANIFEST_MERGED_TYPES,
    EMPTY_MANIFEST,
    EMPTY_FILE_MANIFEST,
    PENDING_MASTER_NO_UPDATE,
  };

  static scoped_ptr<base::Thread> io_thread_;
  static scoped_refptr<HTTPTestServer> http_server_;

  ScopedRunnableMethodFactory<AppCacheUpdateJobTest> method_factory_;
  scoped_ptr<MockAppCacheService> service_;
  scoped_refptr<TestURLRequestContext> request_context_;
  scoped_refptr<AppCacheGroup> group_;
  scoped_refptr<AppCache> protect_newest_cache_;
  scoped_ptr<base::WaitableEvent> event_;

  scoped_ptr<AppCacheResponseWriter> response_writer_;
  scoped_ptr<net::CompletionCallbackImpl<AppCacheUpdateJobTest> >
      write_callback_;

  // Hosts used by an async test that need to live until update job finishes.
  // Otherwise, test can put host on the stack instead of here.
  std::vector<AppCacheHost*> hosts_;

  // Flag indicating if test cares to verify the update after update finishes.
  bool do_checks_after_update_finished_;
  bool expect_group_obsolete_;
  bool expect_group_has_cache_;
  AppCache* expect_old_cache_;
  AppCache* expect_newest_cache_;
  std::vector<MockFrontend*> frontends_;  // to check expected events
  TestedManifest tested_manifest_;
  AppCache::EntryMap expect_extra_entries_;

  bool registered_factory_;
  URLRequest::ProtocolFactory* old_factory_;
};

// static
scoped_ptr<base::Thread> AppCacheUpdateJobTest::io_thread_;
scoped_refptr<HTTPTestServer> AppCacheUpdateJobTest::http_server_;

TEST_F(AppCacheUpdateJobTest, AlreadyChecking) {
  MockAppCacheService service;
  scoped_refptr<AppCacheGroup> group =
      new AppCacheGroup(&service, GURL("http://manifesturl.com"),
                        service.storage()->NewGroupId());

  AppCacheUpdateJob update(&service, group);

  // Pretend group is in checking state.
  group->update_job_ = &update;
  group->update_status_ = AppCacheGroup::CHECKING;

  update.StartUpdate(NULL, GURL::EmptyGURL());
  EXPECT_EQ(AppCacheGroup::CHECKING, group->update_status());

  MockFrontend mock_frontend;
  AppCacheHost host(1, &mock_frontend, &service);
  update.StartUpdate(&host, GURL::EmptyGURL());

  MockFrontend::RaisedEvents events = mock_frontend.raised_events_;
  size_t expected = 1;
  EXPECT_EQ(expected, events.size());
  EXPECT_EQ(expected, events[0].first.size());
  EXPECT_EQ(host.host_id(), events[0].first[0]);
  EXPECT_EQ(CHECKING_EVENT, events[0].second);
  EXPECT_EQ(AppCacheGroup::CHECKING, group->update_status());
}

TEST_F(AppCacheUpdateJobTest, AlreadyDownloading) {
  MockAppCacheService service;
  scoped_refptr<AppCacheGroup> group =
      new AppCacheGroup(&service, GURL("http://manifesturl.com"),
                        service.storage()->NewGroupId());

  AppCacheUpdateJob update(&service, group);

  // Pretend group is in downloading state.
  group->update_job_ = &update;
  group->update_status_ = AppCacheGroup::DOWNLOADING;

  update.StartUpdate(NULL, GURL::EmptyGURL());
  EXPECT_EQ(AppCacheGroup::DOWNLOADING, group->update_status());

  MockFrontend mock_frontend;
  AppCacheHost host(1, &mock_frontend, &service);
  update.StartUpdate(&host, GURL::EmptyGURL());

  MockFrontend::RaisedEvents events = mock_frontend.raised_events_;
  size_t expected = 2;
  EXPECT_EQ(expected, events.size());
  expected = 1;
  EXPECT_EQ(expected, events[0].first.size());
  EXPECT_EQ(host.host_id(), events[0].first[0]);
  EXPECT_EQ(CHECKING_EVENT, events[0].second);

  EXPECT_EQ(expected, events[1].first.size());
  EXPECT_EQ(host.host_id(), events[1].first[0]);
  EXPECT_EQ(appcache::DOWNLOADING_EVENT, events[1].second);

  EXPECT_EQ(AppCacheGroup::DOWNLOADING, group->update_status());
}

TEST_F(AppCacheUpdateJobTest, StartCacheAttempt) {
  RunTestOnIOThread(&AppCacheUpdateJobTest::StartCacheAttemptTest);
}

TEST_F(AppCacheUpdateJobTest, StartUpgradeAttempt) {
  RunTestOnIOThread(&AppCacheUpdateJobTest::StartUpgradeAttemptTest);
}

TEST_F(AppCacheUpdateJobTest, CacheAttemptFetchManifestFail) {
  RunTestOnIOThread(&AppCacheUpdateJobTest::CacheAttemptFetchManifestFailTest);
}

TEST_F(AppCacheUpdateJobTest, UpgradeFetchManifestFail) {
  RunTestOnIOThread(&AppCacheUpdateJobTest::UpgradeFetchManifestFailTest);
}

TEST_F(AppCacheUpdateJobTest, ManifestRedirect) {
  RunTestOnIOThread(&AppCacheUpdateJobTest::ManifestRedirectTest);
}

TEST_F(AppCacheUpdateJobTest, ManifestWrongMimeType) {
  RunTestOnIOThread(&AppCacheUpdateJobTest::ManifestWrongMimeTypeTest);
}

TEST_F(AppCacheUpdateJobTest, ManifestNotFound) {
  RunTestOnIOThread(&AppCacheUpdateJobTest::ManifestNotFoundTest);
}

TEST_F(AppCacheUpdateJobTest, ManifestGone) {
  RunTestOnIOThread(&AppCacheUpdateJobTest::ManifestGoneTest);
}

TEST_F(AppCacheUpdateJobTest, CacheAttemptNotModified) {
  RunTestOnIOThread(&AppCacheUpdateJobTest::CacheAttemptNotModifiedTest);
}

TEST_F(AppCacheUpdateJobTest, UpgradeNotModified) {
  RunTestOnIOThread(&AppCacheUpdateJobTest::UpgradeNotModifiedTest);
}

TEST_F(AppCacheUpdateJobTest, UpgradeManifestDataUnchanged) {
  RunTestOnIOThread(&AppCacheUpdateJobTest::UpgradeManifestDataUnchangedTest);
}

TEST_F(AppCacheUpdateJobTest, BasicCacheAttemptSuccess) {
  RunTestOnIOThread(&AppCacheUpdateJobTest::BasicCacheAttemptSuccessTest);
}

TEST_F(AppCacheUpdateJobTest, BasicUpgradeSuccess) {
  RunTestOnIOThread(&AppCacheUpdateJobTest::BasicUpgradeSuccessTest);
}

TEST_F(AppCacheUpdateJobTest, UpgradeSuccessMergedTypes) {
  RunTestOnIOThread(&AppCacheUpdateJobTest::UpgradeSuccessMergedTypesTest);
}

TEST_F(AppCacheUpdateJobTest, CacheAttemptFailUrlFetch) {
  RunTestOnIOThread(&AppCacheUpdateJobTest::CacheAttemptFailUrlFetchTest);
}

TEST_F(AppCacheUpdateJobTest, UpgradeFailUrlFetch) {
  RunTestOnIOThread(&AppCacheUpdateJobTest::UpgradeFailUrlFetchTest);
}

TEST_F(AppCacheUpdateJobTest, UpgradeFailMasterUrlFetch) {
  RunTestOnIOThread(&AppCacheUpdateJobTest::UpgradeFailMasterUrlFetchTest);
}

TEST_F(AppCacheUpdateJobTest, EmptyManifest) {
  RunTestOnIOThread(&AppCacheUpdateJobTest::EmptyManifestTest);
}

TEST_F(AppCacheUpdateJobTest, EmptyFile) {
  RunTestOnIOThread(&AppCacheUpdateJobTest::EmptyFileTest);
}

TEST_F(AppCacheUpdateJobTest, RetryRequest) {
  RunTestOnIOThread(&AppCacheUpdateJobTest::RetryRequestTest);
}

TEST_F(AppCacheUpdateJobTest, RetryNoRetryAfter) {
  RunTestOnIOThread(&AppCacheUpdateJobTest::RetryNoRetryAfterTest);
}

TEST_F(AppCacheUpdateJobTest, RetryNonzeroRetryAfter) {
  RunTestOnIOThread(&AppCacheUpdateJobTest::RetryNonzeroRetryAfterTest);
}

TEST_F(AppCacheUpdateJobTest, RetrySuccess) {
  RunTestOnIOThread(&AppCacheUpdateJobTest::RetrySuccessTest);
}

TEST_F(AppCacheUpdateJobTest, RetryUrl) {
  RunTestOnIOThread(&AppCacheUpdateJobTest::RetryUrlTest);
}

TEST_F(AppCacheUpdateJobTest, FailStoreNewestCache) {
  RunTestOnIOThread(&AppCacheUpdateJobTest::FailStoreNewestCacheTest);
}

TEST_F(AppCacheUpdateJobTest, UpgradeFailStoreNewestCache) {
  RunTestOnIOThread(&AppCacheUpdateJobTest::UpgradeFailStoreNewestCacheTest);
}

TEST_F(AppCacheUpdateJobTest, FailMakeGroupObsolete) {
  RunTestOnIOThread(&AppCacheUpdateJobTest::FailMakeGroupObsoleteTest);
}

TEST_F(AppCacheUpdateJobTest, UpgradeFailMakeGroupObsolete) {
  RunTestOnIOThread(&AppCacheUpdateJobTest::UpgradeFailMakeGroupObsoleteTest);
}

TEST_F(AppCacheUpdateJobTest, MasterEntryFetchManifestFail) {
  RunTestOnIOThread(&AppCacheUpdateJobTest::MasterEntryFetchManifestFailTest);
}

TEST_F(AppCacheUpdateJobTest, MasterEntryBadManifest) {
  RunTestOnIOThread(&AppCacheUpdateJobTest::MasterEntryBadManifestTest);
}

TEST_F(AppCacheUpdateJobTest, MasterEntryManifestNotFound) {
  RunTestOnIOThread(&AppCacheUpdateJobTest::MasterEntryManifestNotFoundTest);
}

TEST_F(AppCacheUpdateJobTest, MasterEntryFailUrlFetch) {
  RunTestOnIOThread(&AppCacheUpdateJobTest::MasterEntryFailUrlFetchTest);
}

TEST_F(AppCacheUpdateJobTest, MasterEntryAllFail) {
  RunTestOnIOThread(&AppCacheUpdateJobTest::MasterEntryAllFailTest);
}

TEST_F(AppCacheUpdateJobTest, UpgradeMasterEntryAllFail) {
  RunTestOnIOThread(&AppCacheUpdateJobTest::UpgradeMasterEntryAllFailTest);
}

TEST_F(AppCacheUpdateJobTest, MasterEntrySomeFail) {
  RunTestOnIOThread(&AppCacheUpdateJobTest::MasterEntrySomeFailTest);
}

TEST_F(AppCacheUpdateJobTest, UpgradeMasterEntrySomeFail) {
  RunTestOnIOThread(&AppCacheUpdateJobTest::UpgradeMasterEntrySomeFailTest);
}

TEST_F(AppCacheUpdateJobTest, MasterEntryNoUpdate) {
  RunTestOnIOThread(&AppCacheUpdateJobTest::MasterEntryNoUpdateTest);
}

TEST_F(AppCacheUpdateJobTest, StartUpdateMidCacheAttempt) {
  RunTestOnIOThread(&AppCacheUpdateJobTest::StartUpdateMidCacheAttemptTest);
}

TEST_F(AppCacheUpdateJobTest, StartUpdateMidNoUpdate) {
  RunTestOnIOThread(&AppCacheUpdateJobTest::StartUpdateMidNoUpdateTest);
}

TEST_F(AppCacheUpdateJobTest, StartUpdateMidDownload) {
  RunTestOnIOThread(&AppCacheUpdateJobTest::StartUpdateMidDownloadTest);
}

}  // namespace appcache
