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
  virtual void OnCacheSelected(int host_id, int64 cache_id,
                               Status status) {
  }

  virtual void OnStatusChanged(const std::vector<int>& host_ids,
                               Status status) {
  }

  virtual void OnEventRaised(const std::vector<int>& host_ids,
                             EventID event_id) {
    raised_events_.push_back(RaisedEvent(host_ids, event_id));
  }

  void AddExpectedEvent(const std::vector<int>& host_ids, EventID event_id) {
    expected_events_.push_back(RaisedEvent(host_ids, event_id));
  }

  typedef std::vector<int> HostIds;
  typedef std::pair<HostIds, EventID> RaisedEvent;
  typedef std::vector<RaisedEvent> RaisedEvents;
  RaisedEvents raised_events_;

  // Set the expected events if verification needs to happen asynchronously.
  RaisedEvents expected_events_;
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
    group_ = new AppCacheGroup(service_.get(), GURL("http://failme"));

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
      group_ = new AppCacheGroup(service_.get(), GURL("http://failme"));

      // Give the group some existing caches.
      AppCache* cache1 = MakeCacheForGroup(1);
      AppCache* cache2 = MakeCacheForGroup(2);

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
    group_ = new AppCacheGroup(service_.get(), GURL("http://failme"));
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
    group_ = new AppCacheGroup(service_.get(), GURL("http://failme"));
    AppCacheUpdateJob* update = new AppCacheUpdateJob(service_.get(), group_);
    group_->update_job_ = update;

    AppCache* cache = MakeCacheForGroup(1);
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
    group_ = new AppCacheGroup(service_.get(), GURL("http://testme"));
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
        service_.get(), http_server_->TestServerPage("defaultresponse"));
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
        service_.get(), http_server_->TestServerPage("files/nosuchfile"));
    AppCacheUpdateJob* update = new AppCacheUpdateJob(service_.get(), group_);
    group_->update_job_ = update;

    AppCache* cache = MakeCacheForGroup(1);
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
        service_.get(), http_server_->TestServerPage("files/gone"));
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
        service_.get(), http_server_->TestServerPage("files/notmodified"));
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
        service_.get(), http_server_->TestServerPage("files/notmodified"));
    AppCacheUpdateJob* update = new AppCacheUpdateJob(service_.get(), group_);
    group_->update_job_ = update;

    AppCache* cache = MakeCacheForGroup(1);
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
        service_.get(), http_server_->TestServerPage("files/manifest1"));
    AppCacheUpdateJob* update = new AppCacheUpdateJob(service_.get(), group_);
    group_->update_job_ = update;

    AppCache* cache = MakeCacheForGroup(1);
    MockFrontend* frontend1 = MakeMockFrontend();
    MockFrontend* frontend2 = MakeMockFrontend();
    AppCacheHost* host1 = MakeHost(1, frontend1);
    AppCacheHost* host2 = MakeHost(2, frontend2);
    host1->AssociateCache(cache);
    host2->AssociateCache(cache);

    // TODO(jennb): simulate this by mocking storage behavior instead
    update->SimulateManifestChanged(false);  // unchanged

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

  void BasicCacheAttemptSuccessTest() {
    ASSERT_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());

    MakeService();
    group_ = new AppCacheGroup(
        service_.get(), http_server_->TestServerPage("files/manifest1"));
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
        service_.get(), http_server_->TestServerPage("files/manifest1"));
    AppCacheUpdateJob* update = new AppCacheUpdateJob(service_.get(), group_);
    group_->update_job_ = update;

    AppCache* cache = MakeCacheForGroup(service_->storage()->NewCacheId());
    MockFrontend* frontend1 = MakeMockFrontend();
    MockFrontend* frontend2 = MakeMockFrontend();
    AppCacheHost* host1 = MakeHost(1, frontend1);
    AppCacheHost* host2 = MakeHost(2, frontend2);
    host1->AssociateCache(cache);
    host2->AssociateCache(cache);

    // TODO(jennb): simulate this by mocking storage behavior instead
    update->SimulateManifestChanged(true);  // changed

    update->StartUpdate(NULL, GURL::EmptyGURL());
    EXPECT_TRUE(update->manifest_url_request_ != NULL);

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

    WaitForUpdateToFinish();
  }

  void UpgradeSuccessMergedTypesTest() {
    ASSERT_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());

    MakeService();
    group_ = new AppCacheGroup(service_.get(),
        http_server_->TestServerPage("files/manifest-merged-types"));
    AppCacheUpdateJob* update = new AppCacheUpdateJob(service_.get(), group_);
    group_->update_job_ = update;

    AppCache* cache = MakeCacheForGroup(service_->storage()->NewCacheId());
    MockFrontend* frontend1 = MakeMockFrontend();
    MockFrontend* frontend2 = MakeMockFrontend();
    AppCacheHost* host1 = MakeHost(1, frontend1);
    AppCacheHost* host2 = MakeHost(2, frontend2);
    host1->AssociateCache(cache);
    host2->AssociateCache(cache);

    // Give the newest cache a master entry that is also one of the explicit
    // entries in the manifest.
    cache->AddEntry(http_server_->TestServerPage("files/explicit1"),
                    AppCacheEntry(AppCacheEntry::MASTER));

    // TODO(jennb): simulate this by mocking storage behavior instead
    update->SimulateManifestChanged(true);  // changed

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
    frontend1->AddExpectedEvent(ids1, PROGRESS_EVENT);  // explicit1 (fetch)
    frontend1->AddExpectedEvent(ids1, PROGRESS_EVENT);  // manifest
    frontend1->AddExpectedEvent(ids1, UPDATE_READY_EVENT);
    MockFrontend::HostIds ids2(1, host2->host_id());
    frontend2->AddExpectedEvent(ids2, CHECKING_EVENT);
    frontend2->AddExpectedEvent(ids2, DOWNLOADING_EVENT);
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
        http_server_->TestServerPage("files/manifest-with-404"));
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
        http_server_->TestServerPage("files/manifest-fb-404"));
    AppCacheUpdateJob* update = new AppCacheUpdateJob(service_.get(), group_);
    group_->update_job_ = update;

    AppCache* cache = MakeCacheForGroup(service_->storage()->NewCacheId());
    MockFrontend* frontend1 = MakeMockFrontend();
    MockFrontend* frontend2 = MakeMockFrontend();
    AppCacheHost* host1 = MakeHost(1, frontend1);
    AppCacheHost* host2 = MakeHost(2, frontend2);
    host1->AssociateCache(cache);
    host2->AssociateCache(cache);

    // TODO(jennb): simulate this by mocking storage behavior instead
    update->SimulateManifestChanged(true);  // changed

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
        service_.get(), http_server_->TestServerPage("files/manifest1"));
    AppCacheUpdateJob* update = new AppCacheUpdateJob(service_.get(), group_);
    group_->update_job_ = update;

    AppCache* cache = MakeCacheForGroup(service_->storage()->NewCacheId());
    MockFrontend* frontend1 = MakeMockFrontend();
    MockFrontend* frontend2 = MakeMockFrontend();
    AppCacheHost* host1 = MakeHost(1, frontend1);
    AppCacheHost* host2 = MakeHost(2, frontend2);
    host1->AssociateCache(cache);
    host2->AssociateCache(cache);

    // Give the newest cache some master entries; one will fail with a 404.
    cache->AddEntry(
        http_server_->TestServerPage("files/notfound"),
        AppCacheEntry(AppCacheEntry::MASTER));
    cache->AddEntry(
        http_server_->TestServerPage("files/explicit2"),
        AppCacheEntry(AppCacheEntry::MASTER | AppCacheEntry::FOREIGN));
    cache->AddEntry(
        http_server_->TestServerPage("files/servererror"),
        AppCacheEntry(AppCacheEntry::MASTER));

    // TODO(jennb): simulate this by mocking storage behavior instead
    update->SimulateManifestChanged(true);  // changed

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
        AppCacheEntry(AppCacheEntry::MASTER)));  // foreign flag is dropped
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
        service_.get(), http_server_->TestServerPage("files/empty-manifest"));
    AppCacheUpdateJob* update = new AppCacheUpdateJob(service_.get(), group_);
    group_->update_job_ = update;

    AppCache* cache = MakeCacheForGroup(service_->storage()->NewCacheId());
    MockFrontend* frontend1 = MakeMockFrontend();
    MockFrontend* frontend2 = MakeMockFrontend();
    AppCacheHost* host1 = MakeHost(1, frontend1);
    AppCacheHost* host2 = MakeHost(2, frontend2);
    host1->AssociateCache(cache);
    host2->AssociateCache(cache);

    // TODO(jennb): simulate this by mocking storage behavior instead
    update->SimulateManifestChanged(true);  // changed

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

  void RetryRequestTest() {
    ASSERT_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());

    // Set some large number of times to return retry.
    // Expect 1 manifest fetch and 3 retries.
    RetryRequestTestJob::Initialize(5, RetryRequestTestJob::RETRY_AFTER_0, 4);
    old_factory_ = URLRequest::RegisterProtocolFactory(
        "http", RetryRequestTestJob::RetryFactory);
    registered_factory_ = true;

    MakeService();
    group_ = new AppCacheGroup(service_.get(), RetryRequestTestJob::kRetryUrl);
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
    group_ = new AppCacheGroup(service_.get(), RetryRequestTestJob::kRetryUrl);
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
    group_ = new AppCacheGroup(service_.get(), RetryRequestTestJob::kRetryUrl);
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
    group_ = new AppCacheGroup(service_.get(), RetryRequestTestJob::kRetryUrl);
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
    // Set 1 as the retry limit (does not exceed the max).
    // Expect 1 manifest fetch, 1 url fetch, 1 url retry, 1 manifest refetch.
    RetryRequestTestJob::Initialize(1, RetryRequestTestJob::RETRY_AFTER_0, 4);
    old_factory_ = URLRequest::RegisterProtocolFactory(
        "http", RetryRequestTestJob::RetryFactory);
    registered_factory_ = true;

    MakeService();
    group_ = new AppCacheGroup(service_.get(), GURL("http://retryurl"));
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
    // We unwind the stack prior to finishing up to let stack
    // based objects get deleted.
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

  AppCache* MakeCacheForGroup(int64 cache_id) {
    AppCache* cache = new AppCache(service_.get(), cache_id);
    cache->set_complete(true);
    cache->set_update_time(base::TimeTicks::Now());
    group_->AddCache(cache);
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
      if (expect_newest_cache_)
        EXPECT_EQ(expect_newest_cache_, group_->newest_complete_cache());
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
    switch (tested_manifest_) {
      case MANIFEST1:
        VerifyManifest1(group_->newest_complete_cache());
        break;
      case MANIFEST_MERGED_TYPES:
        VerifyManifestMergedTypes(group_->newest_complete_cache());
        break;
      case EMPTY_MANIFEST:
        VerifyEmptyManifest(group_->newest_complete_cache());
        break;
      case NONE:
      default:
        break;
    }
  }

  void VerifyManifest1(AppCache* cache) {
    ASSERT_TRUE(cache != NULL);
    EXPECT_EQ(group_, cache->owning_group());
    EXPECT_TRUE(cache->is_complete());

    size_t expected = 3 + expect_extra_entries_.size();
    EXPECT_EQ(expected, cache->entries().size());
    AppCacheEntry* entry =
        cache->GetEntry(http_server_->TestServerPage("files/manifest1"));
    ASSERT_TRUE(entry);
    EXPECT_EQ(AppCacheEntry::MANIFEST, entry->types());
    entry = cache->GetEntry(http_server_->TestServerPage("files/explicit1"));
    ASSERT_TRUE(entry);
    EXPECT_EQ(AppCacheEntry::EXPLICIT, entry->types());
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
    ASSERT_TRUE(cache != NULL);
    EXPECT_EQ(group_, cache->owning_group());
    EXPECT_TRUE(cache->is_complete());

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
    ASSERT_TRUE(cache!= NULL);
    EXPECT_EQ(group_, cache->owning_group());
    EXPECT_TRUE(cache->is_complete());

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

 private:
  // Various manifest files used in this test.
  enum TestedManifest {
    NONE,
    MANIFEST1,
    MANIFEST_MERGED_TYPES,
    EMPTY_MANIFEST,
  };

  static scoped_ptr<base::Thread> io_thread_;
  static scoped_refptr<HTTPTestServer> http_server_;

  ScopedRunnableMethodFactory<AppCacheUpdateJobTest> method_factory_;
  scoped_ptr<MockAppCacheService> service_;
  scoped_refptr<TestURLRequestContext> request_context_;
  scoped_refptr<AppCacheGroup> group_;
  scoped_refptr<AppCache> protect_newest_cache_;
  scoped_ptr<base::WaitableEvent> event_;

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
      new AppCacheGroup(&service, GURL("http://manifesturl.com"));

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
      new AppCacheGroup(&service, GURL("http://manifesturl.com"));

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

TEST_F(AppCacheUpdateJobTest, DISABLED_CacheAttemptFailUrlFetch) {
  RunTestOnIOThread(&AppCacheUpdateJobTest::CacheAttemptFailUrlFetchTest);
}

TEST_F(AppCacheUpdateJobTest, DISABLED_UpgradeFailUrlFetch) {
  RunTestOnIOThread(&AppCacheUpdateJobTest::UpgradeFailUrlFetchTest);
}

TEST_F(AppCacheUpdateJobTest, UpgradeFailMasterUrlFetch) {
  RunTestOnIOThread(&AppCacheUpdateJobTest::UpgradeFailMasterUrlFetchTest);
}

TEST_F(AppCacheUpdateJobTest, EmptyManifest) {
  RunTestOnIOThread(&AppCacheUpdateJobTest::EmptyManifestTest);
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

}  // namespace appcache
