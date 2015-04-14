// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_fetcher_impl.h"

#include <algorithm>
#include <string>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "crypto/nss_util.h"
#include "net/base/elements_upload_data_stream.h"
#include "net/base/network_change_notifier.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/base/upload_element_reader.h"
#include "net/base/upload_file_element_reader.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_response_headers.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_test_util.h"
#include "net/url_request/url_request_throttler_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(USE_NSS) || defined(OS_IOS)
#include "net/cert_net/nss_ocsp.h"
#endif

namespace net {

using base::Time;
using base::TimeDelta;

// TODO(eroman): Add a regression test for http://crbug.com/40505.

namespace {

// TODO(akalin): Move all the test data to somewhere under net/.
const base::FilePath::CharType kDocRoot[] =
    FILE_PATH_LITERAL("net/data/url_fetcher_impl_unittest");
const char kTestServerFilePrefix[] = "files/";

// Test server path and response body for the default URL used by many of the
// tests.
const char kDefaultResponsePath[] = "defaultresponse";
const char kDefaultResponseBody[] =
    "Default response given for path: /defaultresponse";

// Request body for streams created by CreateUploadStream.
const char kCreateUploadStreamBody[] = "rosebud";

base::FilePath GetUploadFileTestPath() {
  base::FilePath path;
  PathService::Get(base::DIR_SOURCE_ROOT, &path);
  return path.Append(
      FILE_PATH_LITERAL("net/data/url_request_unittest/BullRunSpeech.txt"));
}

// Simple URLRequestDelegate that waits for the specified fetcher to complete.
// Can only be used once.
class WaitingURLFetcherDelegate : public URLFetcherDelegate {
 public:
  WaitingURLFetcherDelegate() : did_complete_(false) {}

  // Creates a URLFetcher that runs network tasks on the current message loop.
  void CreateFetcherWithContext(const GURL& url,
                                URLFetcher::RequestType request_type,
                                net::URLRequestContext* context) {
    CreateFetcherWithContextGetter(
        url, request_type, new TrivialURLRequestContextGetter(
                               context, base::MessageLoopProxy::current()));
  }

  void CreateFetcherWithContextGetter(
      const GURL& url,
      URLFetcher::RequestType request_type,
      net::URLRequestContextGetter* context_getter) {
    fetcher_.reset(new URLFetcherImpl(url, request_type, this));
    fetcher_->SetRequestContext(context_getter);
  }

  URLFetcher* fetcher() const { return fetcher_.get(); }

  void StartFetcherAndWait() {
    fetcher_->Start();
    WaitForComplete();
  }

  // Wait until the request has completed, if it's already been started.
  void WaitForComplete() { run_loop_.Run(); }

  void OnURLFetchComplete(const URLFetcher* source) override {
    EXPECT_EQ(fetcher_.get(), source);
    did_complete_ = true;
    run_loop_.Quit();
  }

  bool did_complete() const { return did_complete_; }

 private:
  bool did_complete_;

  scoped_ptr<URLFetcherImpl> fetcher_;
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(WaitingURLFetcherDelegate);
};

class ThrottlingTestURLRequestContext : public TestURLRequestContext {
 public:
  ThrottlingTestURLRequestContext() : TestURLRequestContext(true) {
    set_throttler_manager(&throttler_manager_);
    Init();
    DCHECK(throttler_manager() != nullptr);
  }

 private:
  URLRequestThrottlerManager throttler_manager_;
};

class ThrottlingTestURLRequestContextGetter
    : public TestURLRequestContextGetter {
 public:
  ThrottlingTestURLRequestContextGetter(
      base::MessageLoopProxy* io_message_loop_proxy,
      TestURLRequestContext* request_context)
      : TestURLRequestContextGetter(io_message_loop_proxy),
        context_(request_context) {
  }

  // TestURLRequestContextGetter:
  TestURLRequestContext* GetURLRequestContext() override { return context_; }

 protected:
  ~ThrottlingTestURLRequestContextGetter() override {}

  TestURLRequestContext* const context_;
};

}  // namespace

class URLFetcherTest : public testing::Test,
                       public URLFetcherDelegate {
 public:
  URLFetcherTest()
      : io_message_loop_proxy_(base::MessageLoopProxy::current()),
        num_upload_streams_created_(0),
        fetcher_(nullptr),
        expected_status_code_(200) {}

  static int GetNumFetcherCores() {
    return URLFetcherImpl::GetNumFetcherCores();
  }

  // Creates a URLFetcher, using the program's main thread to do IO.
  virtual void CreateFetcher(const GURL& url);

  // URLFetcherDelegate:
  // Subclasses that override this should either call this function or
  // CleanupAfterFetchComplete() at the end of their processing, depending on
  // whether they want to check for a non-empty HTTP 200 response or not.
  void OnURLFetchComplete(const URLFetcher* source) override;

  // Deletes |fetcher| and terminates the message loop.
  void CleanupAfterFetchComplete();

  scoped_refptr<base::MessageLoopProxy> io_message_loop_proxy() {
    return io_message_loop_proxy_;
  }

  TestURLRequestContext* request_context() {
    return context_.get();
  }

  // Callback passed to URLFetcher to create upload stream by some tests.
  scoped_ptr<UploadDataStream> CreateUploadStream() {
    ++num_upload_streams_created_;
    std::vector<char> buffer(
        kCreateUploadStreamBody,
        kCreateUploadStreamBody + strlen(kCreateUploadStreamBody));
    return ElementsUploadDataStream::CreateWithReader(
        scoped_ptr<UploadElementReader>(
            new UploadOwnedBytesElementReader(&buffer)),
        0);
  }

  // Number of streams created by CreateUploadStream.
  size_t num_upload_streams_created() const {
    return num_upload_streams_created_;
  }

  // Returns a URL that hangs on DNS resolution.  Only hangs when using the
  // request context returned by request_context().
  const GURL& hanging_url() const { return hanging_url_; }

  MockHostResolver* resolver() { return &resolver_; }

  // testing::Test:
  void SetUp() override {
    SetUpServer();
    ASSERT_TRUE(test_server_->Start());

    // Set up host resolver so requests for |hanging_url_| block on an async DNS
    // resolver.  Calling resolver()->ResolveAllPending() will resume the hung
    // requests.
    resolver_.set_ondemand_mode(true);
    resolver_.rules()->AddRule("example.com", "127.0.0.1");
    hanging_url_ =
        GURL(base::StringPrintf("http://example.com:%d/defaultresponse",
                                test_server_->host_port_pair().port()));
    ASSERT_TRUE(hanging_url_.is_valid());

    context_.reset(new TestURLRequestContext(true));
    context_->set_host_resolver(&resolver_);
    context_->set_throttler_manager(&throttler_manager_);
    context_->Init();

#if defined(USE_NSS) || defined(OS_IOS)
    crypto::EnsureNSSInit();
    EnsureNSSHttpIOInit();
#endif
  }

  void TearDown() override {
#if defined(USE_NSS) || defined(OS_IOS)
    ShutdownNSSHttpIO();
#endif
  }

  // Initializes |test_server_| without starting it.  Allows subclasses to use
  // their own server configuration.
  virtual void SetUpServer() {
    test_server_.reset(new SpawnedTestServer(SpawnedTestServer::TYPE_HTTP,
                                             SpawnedTestServer::kLocalhost,
                                             base::FilePath(kDocRoot)));
  }

  // URLFetcher is designed to run on the main UI thread, but in our tests
  // we assume that the current thread is the IO thread where the URLFetcher
  // dispatches its requests to.  When we wish to simulate being used from
  // a UI thread, we dispatch a worker thread to do so.
  scoped_refptr<base::MessageLoopProxy> io_message_loop_proxy_;

  scoped_ptr<SpawnedTestServer> test_server_;
  GURL hanging_url_;

  size_t num_upload_streams_created_;

  URLFetcherImpl* fetcher_;

  MockHostResolver resolver_;
  URLRequestThrottlerManager throttler_manager_;
  scoped_ptr<TestURLRequestContext> context_;

  int expected_status_code_;
};

void URLFetcherTest::CreateFetcher(const GURL& url) {
  fetcher_ = new URLFetcherImpl(url, URLFetcher::GET, this);
  fetcher_->SetRequestContext(new ThrottlingTestURLRequestContextGetter(
      io_message_loop_proxy().get(), request_context()));
  fetcher_->Start();
}

void URLFetcherTest::OnURLFetchComplete(const URLFetcher* source) {
  EXPECT_TRUE(source->GetStatus().is_success());
  EXPECT_EQ(expected_status_code_, source->GetResponseCode());  // HTTP OK

  std::string data;
  EXPECT_TRUE(source->GetResponseAsString(&data));
  EXPECT_FALSE(data.empty());

  CleanupAfterFetchComplete();
}

void URLFetcherTest::CleanupAfterFetchComplete() {
  delete fetcher_;  // Have to delete this here and not in the destructor,
                    // because the destructor won't necessarily run on the
                    // same thread that CreateFetcher() did.

  io_message_loop_proxy()->PostTask(FROM_HERE,
                                    base::MessageLoop::QuitClosure());
  // If the current message loop is not the IO loop, it will be shut down when
  // the main loop returns and this thread subsequently goes out of scope.
}

namespace {

// Version of URLFetcherTest that tests download progress reports.
class URLFetcherDownloadProgressTest : public URLFetcherTest {
 public:
  URLFetcherDownloadProgressTest()
      : previous_progress_(0),
        expected_total_(0) {
  }

  // URLFetcherTest:
  void CreateFetcher(const GURL& url) override;

  // URLFetcherDelegate:
  void OnURLFetchDownloadProgress(const URLFetcher* source,
                                  int64 current,
                                  int64 total) override;

 protected:
  // Download progress returned by the previous callback.
  int64 previous_progress_;
  // Size of the file being downloaded, known in advance (provided by each test
  // case).
  int64 expected_total_;
};

// Version of URLFetcherTest that tests progress reports at cancellation.
class URLFetcherDownloadProgressCancelTest : public URLFetcherTest {
 public:
  // URLFetcherTest:
  void CreateFetcher(const GURL& url) override;

  // URLFetcherDelegate:
  void OnURLFetchComplete(const URLFetcher* source) override;
  void OnURLFetchDownloadProgress(const URLFetcher* source,
                                  int64 current,
                                  int64 total) override;

 protected:
  bool cancelled_;
};

// Version of URLFetcherTest that tests upload progress reports.
class URLFetcherUploadProgressTest : public URLFetcherTest {
 public:
  // URLFetcherTest:
  void CreateFetcher(const GURL& url) override;

  // URLFetcherDelegate:
  void OnURLFetchUploadProgress(const URLFetcher* source,
                                int64 current,
                                int64 total) override;

 protected:
  int64 previous_progress_;
  std::string chunk_;
  int64 number_of_chunks_added_;
};

// Version of URLFetcherTest that tests bad HTTPS requests.
class URLFetcherBadHTTPSTest : public URLFetcherTest {
 public:
  URLFetcherBadHTTPSTest();

  // URLFetcherTest:
  void SetUpServer() override;

  // URLFetcherDelegate:
  void OnURLFetchComplete(const URLFetcher* source) override;

 private:
  base::FilePath cert_dir_;
};

// Version of URLFetcherTest that tests request cancellation on shutdown.
class URLFetcherCancelTest : public URLFetcherTest {
 public:
  // URLFetcherTest:
  void CreateFetcher(const GURL& url) override;

  // URLFetcherDelegate:
  void OnURLFetchComplete(const URLFetcher* source) override;

  void CancelRequest();
};

// Version of TestURLRequestContext that posts a Quit task to the IO
// thread once it is deleted.
class CancelTestURLRequestContext : public ThrottlingTestURLRequestContext {
 public:
  explicit CancelTestURLRequestContext() {
  }

 private:
  ~CancelTestURLRequestContext() override {
    // The d'tor should execute in the IO thread. Post the quit task to the
    // current thread.
    base::MessageLoop::current()->PostTask(FROM_HERE,
                                           base::MessageLoop::QuitClosure());
  }
};

class CancelTestURLRequestContextGetter
    : public TestURLRequestContextGetter {
 public:
  CancelTestURLRequestContextGetter(
      base::MessageLoopProxy* io_message_loop_proxy,
      const GURL& throttle_for_url)
      : TestURLRequestContextGetter(io_message_loop_proxy),
        io_message_loop_proxy_(io_message_loop_proxy),
        context_created_(false, false),
        throttle_for_url_(throttle_for_url) {
  }

  // TestURLRequestContextGetter:
  TestURLRequestContext* GetURLRequestContext() override {
    if (!context_.get()) {
      context_.reset(new CancelTestURLRequestContext());
      DCHECK(context_->throttler_manager());

      // Registers an entry for test url. The backoff time is calculated by:
      //     new_backoff = 2.0 * old_backoff + 0
      // The initial backoff is 2 seconds and maximum backoff is 4 seconds.
      // Maximum retries allowed is set to 2.
      scoped_refptr<URLRequestThrottlerEntry> entry(
          new URLRequestThrottlerEntry(context_->throttler_manager(),
                                       std::string(),
                                       200,
                                       3,
                                       2000,
                                       2.0,
                                       0.0,
                                       4000));
      context_->throttler_manager()
          ->OverrideEntryForTests(throttle_for_url_, entry.get());

      context_created_.Signal();
    }
    return context_.get();
  }

  virtual scoped_refptr<base::MessageLoopProxy> GetIOMessageLoopProxy() const {
    return io_message_loop_proxy_;
  }

  void WaitForContextCreation() {
    context_created_.Wait();
  }

 protected:
  ~CancelTestURLRequestContextGetter() override {}

 private:
  scoped_ptr<TestURLRequestContext> context_;
  scoped_refptr<base::MessageLoopProxy> io_message_loop_proxy_;
  base::WaitableEvent context_created_;
  GURL throttle_for_url_;
};

// Version of URLFetcherTest that tests retying the same request twice.
class URLFetcherMultipleAttemptTest : public URLFetcherTest {
 public:
  // URLFetcherDelegate:
  void OnURLFetchComplete(const URLFetcher* source) override;

 private:
  std::string data_;
};

class URLFetcherFileTest : public URLFetcherTest {
 public:
  URLFetcherFileTest() : take_ownership_of_file_(false),
                         expected_file_error_(OK) {}

  void CreateFetcherForFile(const GURL& url, const base::FilePath& file_path);
  void CreateFetcherForTempFile(const GURL& url);

  // URLFetcherDelegate:
  void OnURLFetchComplete(const URLFetcher* source) override;

 protected:
  base::FilePath expected_file_;
  base::FilePath file_path_;

  // Set by the test. Used in OnURLFetchComplete() to decide if
  // the URLFetcher should own the temp file, so that we can test
  // disowning prevents the file from being deleted.
  bool take_ownership_of_file_;

  // Expected file error code for the test.  OK when expecting success.
  int expected_file_error_;
};

void URLFetcherDownloadProgressTest::CreateFetcher(const GURL& url) {
  fetcher_ = new URLFetcherImpl(url, URLFetcher::GET, this);
  fetcher_->SetRequestContext(new ThrottlingTestURLRequestContextGetter(
      io_message_loop_proxy().get(), request_context()));
  fetcher_->Start();
}

void URLFetcherDownloadProgressTest::OnURLFetchDownloadProgress(
    const URLFetcher* source, int64 progress, int64 total) {
  // Increasing between 0 and total.
  EXPECT_LE(0, progress);
  EXPECT_GE(total, progress);
  EXPECT_LE(previous_progress_, progress);
  EXPECT_EQ(expected_total_, total);
  previous_progress_ = progress;
}

void URLFetcherDownloadProgressCancelTest::CreateFetcher(const GURL& url) {
  fetcher_ = new URLFetcherImpl(url, URLFetcher::GET, this);
  fetcher_->SetRequestContext(new ThrottlingTestURLRequestContextGetter(
      io_message_loop_proxy().get(), request_context()));
  cancelled_ = false;
  fetcher_->Start();
}

void URLFetcherDownloadProgressCancelTest::OnURLFetchDownloadProgress(
    const URLFetcher* source, int64 current, int64 total) {
  EXPECT_FALSE(cancelled_);
  if (!cancelled_) {
    cancelled_ = true;
    CleanupAfterFetchComplete();
  }
}

void URLFetcherDownloadProgressCancelTest::OnURLFetchComplete(
    const URLFetcher* source) {
  // Should have been cancelled.
  ADD_FAILURE();
  CleanupAfterFetchComplete();
}

void URLFetcherUploadProgressTest::CreateFetcher(const GURL& url) {
  fetcher_ = new URLFetcherImpl(url, URLFetcher::POST, this);
  fetcher_->SetRequestContext(new ThrottlingTestURLRequestContextGetter(
      io_message_loop_proxy().get(), request_context()));
  previous_progress_ = 0;
  // Large enough data to require more than one read from UploadDataStream.
  chunk_.assign(1<<16, 'a');
  // Use chunked upload to wait for a timer event of progress notification.
  fetcher_->SetChunkedUpload("application/x-www-form-urlencoded");
  fetcher_->Start();
  number_of_chunks_added_ = 1;
  fetcher_->AppendChunkToUpload(chunk_, false);
}

void URLFetcherUploadProgressTest::OnURLFetchUploadProgress(
    const URLFetcher* source, int64 current, int64 total) {
  // Increasing between 0 and total.
  EXPECT_LE(0, current);
  EXPECT_GE(static_cast<int64>(chunk_.size()) * number_of_chunks_added_,
            current);
  EXPECT_LE(previous_progress_, current);
  previous_progress_ = current;
  EXPECT_EQ(-1, total);

  if (number_of_chunks_added_ < 2) {
    number_of_chunks_added_ += 1;
    fetcher_->AppendChunkToUpload(chunk_, true);
  }
}

URLFetcherBadHTTPSTest::URLFetcherBadHTTPSTest() {
  PathService::Get(base::DIR_SOURCE_ROOT, &cert_dir_);
  cert_dir_ = cert_dir_.AppendASCII("chrome");
  cert_dir_ = cert_dir_.AppendASCII("test");
  cert_dir_ = cert_dir_.AppendASCII("data");
  cert_dir_ = cert_dir_.AppendASCII("ssl");
  cert_dir_ = cert_dir_.AppendASCII("certificates");
}

void URLFetcherBadHTTPSTest::SetUpServer() {
  SpawnedTestServer::SSLOptions ssl_options(
      SpawnedTestServer::SSLOptions::CERT_EXPIRED);
  test_server_.reset(new SpawnedTestServer(
      SpawnedTestServer::TYPE_HTTPS, ssl_options, base::FilePath(kDocRoot)));
}

// The "server certificate expired" error should result in automatic
// cancellation of the request by
// URLRequest::Delegate::OnSSLCertificateError.
void URLFetcherBadHTTPSTest::OnURLFetchComplete(
    const URLFetcher* source) {
  // This part is different from URLFetcherTest::OnURLFetchComplete
  // because this test expects the request to be cancelled.
  EXPECT_EQ(URLRequestStatus::CANCELED, source->GetStatus().status());
  EXPECT_EQ(ERR_ABORTED, source->GetStatus().error());
  EXPECT_EQ(-1, source->GetResponseCode());
  EXPECT_TRUE(source->GetCookies().empty());
  std::string data;
  EXPECT_TRUE(source->GetResponseAsString(&data));
  EXPECT_TRUE(data.empty());
  CleanupAfterFetchComplete();
}

void URLFetcherCancelTest::CreateFetcher(const GURL& url) {
  fetcher_ = new URLFetcherImpl(url, URLFetcher::GET, this);
  CancelTestURLRequestContextGetter* context_getter =
      new CancelTestURLRequestContextGetter(io_message_loop_proxy().get(), url);
  fetcher_->SetRequestContext(context_getter);
  fetcher_->SetMaxRetriesOn5xx(2);
  fetcher_->Start();
  // We need to wait for the creation of the URLRequestContext, since we
  // rely on it being destroyed as a signal to end the test.
  context_getter->WaitForContextCreation();
  CancelRequest();
}

void URLFetcherCancelTest::OnURLFetchComplete(
    const URLFetcher* source) {
  // We should have cancelled the request before completion.
  ADD_FAILURE();
  CleanupAfterFetchComplete();
}

void URLFetcherCancelTest::CancelRequest() {
  delete fetcher_;
  // The URLFetcher's test context will post a Quit task once it is
  // deleted. So if this test simply hangs, it means cancellation
  // did not work.
}

void URLFetcherMultipleAttemptTest::OnURLFetchComplete(
    const URLFetcher* source) {
  EXPECT_TRUE(source->GetStatus().is_success());
  EXPECT_EQ(200, source->GetResponseCode());  // HTTP OK
  std::string data;
  EXPECT_TRUE(source->GetResponseAsString(&data));
  EXPECT_FALSE(data.empty());
  if (!data.empty() && data_.empty()) {
    data_ = data;
    fetcher_->SetRequestContext(new ThrottlingTestURLRequestContextGetter(
        io_message_loop_proxy().get(), request_context()));
    fetcher_->Start();
  } else {
    EXPECT_EQ(data, data_);
    CleanupAfterFetchComplete();
  }
}

void URLFetcherFileTest::CreateFetcherForFile(const GURL& url,
                                              const base::FilePath& file_path) {
  fetcher_ = new URLFetcherImpl(url, URLFetcher::GET, this);
  fetcher_->SetRequestContext(new ThrottlingTestURLRequestContextGetter(
      io_message_loop_proxy().get(), request_context()));

  // Use the IO message loop to do the file operations in this test.
  fetcher_->SaveResponseToFileAtPath(file_path, io_message_loop_proxy());
  fetcher_->Start();
}

void URLFetcherFileTest::CreateFetcherForTempFile(const GURL& url) {
  fetcher_ = new URLFetcherImpl(url, URLFetcher::GET, this);
  fetcher_->SetRequestContext(new ThrottlingTestURLRequestContextGetter(
      io_message_loop_proxy().get(), request_context()));

  // Use the IO message loop to do the file operations in this test.
  fetcher_->SaveResponseToTemporaryFile(io_message_loop_proxy());
  fetcher_->Start();
}

void URLFetcherFileTest::OnURLFetchComplete(const URLFetcher* source) {
  if (expected_file_error_ == OK) {
    EXPECT_TRUE(source->GetStatus().is_success());
    EXPECT_EQ(OK, source->GetStatus().error());
    EXPECT_EQ(200, source->GetResponseCode());

    EXPECT_TRUE(source->GetResponseAsFilePath(
        take_ownership_of_file_, &file_path_));

    EXPECT_TRUE(base::ContentsEqual(expected_file_, file_path_));
  } else {
    EXPECT_FALSE(source->GetStatus().is_success());
    EXPECT_EQ(expected_file_error_, source->GetStatus().error());
  }
  CleanupAfterFetchComplete();
}

// Create the fetcher on the main thread.  Since network IO will happen on the
// main thread, this will test URLFetcher's ability to do everything on one
// thread.
TEST_F(URLFetcherTest, SameThreadTest) {
  WaitingURLFetcherDelegate delegate;
  delegate.CreateFetcherWithContext(test_server_->GetURL(kDefaultResponsePath),
                                    URLFetcher::GET, request_context());
  delegate.StartFetcherAndWait();

  EXPECT_TRUE(delegate.fetcher()->GetStatus().is_success());
  EXPECT_EQ(200, delegate.fetcher()->GetResponseCode());
  std::string data;
  ASSERT_TRUE(delegate.fetcher()->GetResponseAsString(&data));
  EXPECT_EQ(kDefaultResponseBody, data);
}

// Create a separate thread that will create the URLFetcher.  A separate thread
// acts as the network thread.
TEST_F(URLFetcherTest, DifferentThreadsTest) {
  base::Thread network_thread("network thread");
  base::Thread::Options network_thread_options;
  network_thread_options.message_loop_type = base::MessageLoop::TYPE_IO;
  ASSERT_TRUE(network_thread.StartWithOptions(network_thread_options));

  scoped_refptr<TestURLRequestContextGetter> context_getter(
      new TestURLRequestContextGetter(network_thread.task_runner()));
  WaitingURLFetcherDelegate delegate;
  delegate.CreateFetcherWithContextGetter(
      test_server_->GetURL(kDefaultResponsePath), URLFetcher::GET,
      new TestURLRequestContextGetter(network_thread.task_runner()));
  delegate.StartFetcherAndWait();

  EXPECT_TRUE(delegate.fetcher()->GetStatus().is_success());
  EXPECT_EQ(200, delegate.fetcher()->GetResponseCode());
  std::string data;
  ASSERT_TRUE(delegate.fetcher()->GetResponseAsString(&data));
  EXPECT_EQ(kDefaultResponseBody, data);
}

// Tests to make sure CancelAll() will successfully cancel existing URLFetchers.
TEST_F(URLFetcherTest, CancelAll) {
  EXPECT_EQ(0, GetNumFetcherCores());
  WaitingURLFetcherDelegate delegate;
  delegate.CreateFetcherWithContext(hanging_url(), URLFetcher::GET,
                                    request_context());
  delegate.fetcher()->Start();
  // Wait for the request to reach the mock resolver and hang, to ensure the
  // request has actually started.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(resolver_.has_pending_requests());

  EXPECT_EQ(1, URLFetcherTest::GetNumFetcherCores());
  URLFetcherImpl::CancelAll();
  EXPECT_EQ(0, URLFetcherTest::GetNumFetcherCores());
}

TEST_F(URLFetcherTest, DontRetryOnNetworkChangedByDefault) {
  EXPECT_EQ(0, GetNumFetcherCores());
  WaitingURLFetcherDelegate delegate;
  delegate.CreateFetcherWithContext(hanging_url(), URLFetcher::GET,
                                    request_context());
  EXPECT_FALSE(resolver_.has_pending_requests());

  // This posts a task to start the fetcher.
  delegate.fetcher()->Start();
  base::RunLoop().RunUntilIdle();

  // The fetcher is now running, but is pending the host resolve.
  EXPECT_EQ(1, GetNumFetcherCores());
  EXPECT_TRUE(resolver_.has_pending_requests());
  ASSERT_FALSE(delegate.did_complete());

  // A network change notification aborts the connect job.
  NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();
  delegate.WaitForComplete();
  EXPECT_FALSE(resolver_.has_pending_requests());

  // And the owner of the fetcher gets the ERR_NETWORK_CHANGED error.
  EXPECT_EQ(hanging_url(), delegate.fetcher()->GetOriginalURL());
  ASSERT_FALSE(delegate.fetcher()->GetStatus().is_success());
  EXPECT_EQ(ERR_NETWORK_CHANGED, delegate.fetcher()->GetStatus().error());
}

TEST_F(URLFetcherTest, RetryOnNetworkChangedAndFail) {
  EXPECT_EQ(0, GetNumFetcherCores());
  WaitingURLFetcherDelegate delegate;
  delegate.CreateFetcherWithContext(hanging_url(), URLFetcher::GET,
                                    request_context());
  delegate.fetcher()->SetAutomaticallyRetryOnNetworkChanges(3);
  EXPECT_FALSE(resolver_.has_pending_requests());

  // This posts a task to start the fetcher.
  delegate.fetcher()->Start();
  base::RunLoop().RunUntilIdle();

  // The fetcher is now running, but is pending the host resolve.
  EXPECT_EQ(1, GetNumFetcherCores());
  EXPECT_TRUE(resolver_.has_pending_requests());
  ASSERT_FALSE(delegate.did_complete());

  // Make it fail 3 times.
  for (int i = 0; i < 3; ++i) {
    // A network change notification aborts the connect job.
    NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();
    base::RunLoop().RunUntilIdle();

    // But the fetcher retries automatically.
    EXPECT_EQ(1, GetNumFetcherCores());
    EXPECT_TRUE(resolver_.has_pending_requests());
    ASSERT_FALSE(delegate.did_complete());
  }

  // A 4th failure doesn't trigger another retry, and propagates the error
  // to the owner of the fetcher.
  NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();
  delegate.WaitForComplete();
  EXPECT_FALSE(resolver_.has_pending_requests());

  // And the owner of the fetcher gets the ERR_NETWORK_CHANGED error.
  EXPECT_EQ(hanging_url(), delegate.fetcher()->GetOriginalURL());
  ASSERT_FALSE(delegate.fetcher()->GetStatus().is_success());
  EXPECT_EQ(ERR_NETWORK_CHANGED, delegate.fetcher()->GetStatus().error());
}

TEST_F(URLFetcherTest, RetryOnNetworkChangedAndSucceed) {
  EXPECT_EQ(0, GetNumFetcherCores());
  WaitingURLFetcherDelegate delegate;
  delegate.CreateFetcherWithContext(hanging_url(), URLFetcher::GET,
                                    request_context());
  delegate.fetcher()->SetAutomaticallyRetryOnNetworkChanges(3);
  EXPECT_FALSE(resolver_.has_pending_requests());

  // This posts a task to start the fetcher.
  delegate.fetcher()->Start();
  base::RunLoop().RunUntilIdle();

  // The fetcher is now running, but is pending the host resolve.
  EXPECT_EQ(1, GetNumFetcherCores());
  EXPECT_TRUE(resolver_.has_pending_requests());
  ASSERT_FALSE(delegate.did_complete());

  // Make it fail 3 times.
  for (int i = 0; i < 3; ++i) {
    // A network change notification aborts the connect job.
    NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();
    base::RunLoop().RunUntilIdle();

    // But the fetcher retries automatically.
    EXPECT_EQ(1, GetNumFetcherCores());
    EXPECT_TRUE(resolver_.has_pending_requests());
    ASSERT_FALSE(delegate.did_complete());
  }

  // Now let it succeed by resolving the pending request.
  resolver_.ResolveAllPending();
  delegate.WaitForComplete();
  EXPECT_FALSE(resolver_.has_pending_requests());

  // This time the request succeeded.
  EXPECT_EQ(hanging_url(), delegate.fetcher()->GetOriginalURL());
  EXPECT_TRUE(delegate.fetcher()->GetStatus().is_success());
  EXPECT_EQ(200, delegate.fetcher()->GetResponseCode());

  std::string data;
  ASSERT_TRUE(delegate.fetcher()->GetResponseAsString(&data));
  EXPECT_EQ(kDefaultResponseBody, data);
}

TEST_F(URLFetcherTest, PostString) {
  const char kUploadData[] = "bobsyeruncle";

  WaitingURLFetcherDelegate delegate;
  delegate.CreateFetcherWithContext(test_server_->GetURL("echo"),
                                    URLFetcher::POST, request_context());
  delegate.fetcher()->SetUploadData("application/x-www-form-urlencoded",
                                    kUploadData);
  delegate.StartFetcherAndWait();

  EXPECT_TRUE(delegate.fetcher()->GetStatus().is_success());
  EXPECT_EQ(200, delegate.fetcher()->GetResponseCode());
  std::string data;
  ASSERT_TRUE(delegate.fetcher()->GetResponseAsString(&data));
  EXPECT_EQ(kUploadData, data);
}

TEST_F(URLFetcherTest, PostEmptyString) {
  const char kUploadData[] = "";

  WaitingURLFetcherDelegate delegate;
  delegate.CreateFetcherWithContext(test_server_->GetURL("echo"),
                                    URLFetcher::POST, request_context());
  delegate.fetcher()->SetUploadData("application/x-www-form-urlencoded",
                                    kUploadData);
  delegate.StartFetcherAndWait();

  EXPECT_TRUE(delegate.fetcher()->GetStatus().is_success());
  EXPECT_EQ(200, delegate.fetcher()->GetResponseCode());
  std::string data;
  ASSERT_TRUE(delegate.fetcher()->GetResponseAsString(&data));
  EXPECT_EQ(kUploadData, data);
}

TEST_F(URLFetcherTest, PostEntireFile) {
  base::FilePath upload_path = GetUploadFileTestPath();

  WaitingURLFetcherDelegate delegate;
  delegate.CreateFetcherWithContext(test_server_->GetURL("echo"),
                                    URLFetcher::POST, request_context());
  delegate.fetcher()->SetUploadFilePath("application/x-www-form-urlencoded",
                                        upload_path, 0, kuint64max,
                                        base::MessageLoopProxy::current());
  delegate.StartFetcherAndWait();

  EXPECT_TRUE(delegate.fetcher()->GetStatus().is_success());
  EXPECT_EQ(200, delegate.fetcher()->GetResponseCode());

  std::string expected;
  ASSERT_TRUE(base::ReadFileToString(upload_path, &expected));
  std::string data;
  ASSERT_TRUE(delegate.fetcher()->GetResponseAsString(&data));
  EXPECT_EQ(expected, data);
}

TEST_F(URLFetcherTest, PostFileRange) {
  const size_t kRangeStart = 30;
  const size_t kRangeLength = 100;
  base::FilePath upload_path = GetUploadFileTestPath();

  WaitingURLFetcherDelegate delegate;
  delegate.CreateFetcherWithContext(test_server_->GetURL("echo"),
                                    URLFetcher::POST, request_context());
  delegate.fetcher()->SetUploadFilePath("application/x-www-form-urlencoded",
                                        upload_path, kRangeStart, kRangeLength,
                                        base::MessageLoopProxy::current());
  delegate.StartFetcherAndWait();

  EXPECT_TRUE(delegate.fetcher()->GetStatus().is_success());
  EXPECT_EQ(200, delegate.fetcher()->GetResponseCode());

  std::string expected;
  ASSERT_TRUE(base::ReadFileToString(upload_path, &expected));
  std::string data;
  ASSERT_TRUE(delegate.fetcher()->GetResponseAsString(&data));
  EXPECT_EQ(expected.substr(kRangeStart, kRangeLength), data);
}

TEST_F(URLFetcherTest, PostWithUploadStreamFactory) {
  WaitingURLFetcherDelegate delegate;
  delegate.CreateFetcherWithContext(test_server_->GetURL("echo"),
                                    URLFetcher::POST, request_context());
  delegate.fetcher()->SetUploadStreamFactory(
      "text/plain",
      base::Bind(&URLFetcherTest::CreateUploadStream, base::Unretained(this)));
  delegate.StartFetcherAndWait();

  EXPECT_TRUE(delegate.fetcher()->GetStatus().is_success());
  EXPECT_EQ(200, delegate.fetcher()->GetResponseCode());
  std::string data;
  ASSERT_TRUE(delegate.fetcher()->GetResponseAsString(&data));
  EXPECT_EQ(kCreateUploadStreamBody, data);
  EXPECT_EQ(1u, num_upload_streams_created());
}

TEST_F(URLFetcherTest, PostWithUploadStreamFactoryAndRetries) {
  WaitingURLFetcherDelegate delegate;
  delegate.CreateFetcherWithContext(test_server_->GetURL("echo?status=500"),
                                    URLFetcher::POST, request_context());
  delegate.fetcher()->SetAutomaticallyRetryOn5xx(true);
  delegate.fetcher()->SetMaxRetriesOn5xx(1);
  delegate.fetcher()->SetUploadStreamFactory(
      "text/plain",
      base::Bind(&URLFetcherTest::CreateUploadStream, base::Unretained(this)));
  delegate.StartFetcherAndWait();

  EXPECT_TRUE(delegate.fetcher()->GetStatus().is_success());
  EXPECT_EQ(500, delegate.fetcher()->GetResponseCode());
  std::string data;
  ASSERT_TRUE(delegate.fetcher()->GetResponseAsString(&data));
  EXPECT_EQ(kCreateUploadStreamBody, data);
  EXPECT_EQ(2u, num_upload_streams_created());
}

TEST_F(URLFetcherUploadProgressTest, Basic) {
  CreateFetcher(test_server_->GetURL("echo"));
  base::MessageLoop::current()->Run();
}

TEST_F(URLFetcherDownloadProgressTest, Basic) {
  // Get a file large enough to require more than one read into
  // URLFetcher::Core's IOBuffer.
  static const char kFileToFetch[] = "animate1.gif";
  // Hardcoded file size - it cannot be easily fetched when a remote http server
  // is used for testing.
  static const int64 kFileSize = 19021;

  expected_total_ = kFileSize;

  CreateFetcher(
      test_server_->GetURL(std::string(kTestServerFilePrefix) + kFileToFetch));

  base::MessageLoop::current()->Run();
}

TEST_F(URLFetcherDownloadProgressCancelTest, CancelWhileProgressReport) {
  // Get a file large enough to require more than one read into
  // URLFetcher::Core's IOBuffer.
  static const char kFileToFetch[] = "animate1.gif";
  CreateFetcher(
      test_server_->GetURL(std::string(kTestServerFilePrefix) + kFileToFetch));

  base::MessageLoop::current()->Run();
}

TEST_F(URLFetcherTest, Headers) {
  WaitingURLFetcherDelegate delegate;
  delegate.CreateFetcherWithContext(
      test_server_->GetURL("set-header?cache-control: private"),
      URLFetcher::GET, request_context());
  delegate.StartFetcherAndWait();

  EXPECT_TRUE(delegate.fetcher()->GetStatus().is_success());
  EXPECT_EQ(200, delegate.fetcher()->GetResponseCode());
  std::string header;
  ASSERT_TRUE(delegate.fetcher()->GetResponseHeaders()->GetNormalizedHeader(
      "cache-control", &header));
  EXPECT_EQ("private", header);
}

TEST_F(URLFetcherTest, SocketAddress) {
  WaitingURLFetcherDelegate delegate;
  delegate.CreateFetcherWithContext(test_server_->GetURL(kDefaultResponsePath),
                                    URLFetcher::GET, request_context());
  delegate.StartFetcherAndWait();

  EXPECT_TRUE(delegate.fetcher()->GetStatus().is_success());
  EXPECT_EQ(200, delegate.fetcher()->GetResponseCode());
  EXPECT_EQ(test_server_->host_port_pair().port(),
            delegate.fetcher()->GetSocketAddress().port());
  EXPECT_EQ(test_server_->host_port_pair().host(),
            delegate.fetcher()->GetSocketAddress().host());
}

TEST_F(URLFetcherTest, StopOnRedirect) {
  const char kRedirectTarget[] = "http://redirect.target.com";

  WaitingURLFetcherDelegate delegate;
  delegate.CreateFetcherWithContext(
      test_server_->GetURL(std::string("server-redirect?") + kRedirectTarget),
      URLFetcher::GET, request_context());
  delegate.fetcher()->SetStopOnRedirect(true);
  delegate.StartFetcherAndWait();

  EXPECT_EQ(GURL(kRedirectTarget), delegate.fetcher()->GetURL());
  EXPECT_EQ(URLRequestStatus::CANCELED,
            delegate.fetcher()->GetStatus().status());
  EXPECT_EQ(ERR_ABORTED, delegate.fetcher()->GetStatus().error());
  EXPECT_EQ(301, delegate.fetcher()->GetResponseCode());
}

TEST_F(URLFetcherTest, ThrottleOnRepeatedFetches) {
  base::Time start_time = Time::Now();
  GURL url(test_server_->GetURL(kDefaultResponsePath));

  // Registers an entry for test url. It only allows 3 requests to be sent
  // in 200 milliseconds.
  scoped_refptr<URLRequestThrottlerEntry> entry(new URLRequestThrottlerEntry(
      request_context()->throttler_manager(), std::string() /* url_id */,
      200 /* sliding_window_period_ms */, 3 /* max_send_threshold */,
      1 /* initial_backoff_ms */, 2.0 /* multiply_factor */,
      0.0 /* jitter_factor */, 256 /* maximum_backoff_ms */));

  request_context()->throttler_manager()
      ->OverrideEntryForTests(url, entry.get());

  for (int i = 0; i < 20; ++i) {
    WaitingURLFetcherDelegate delegate;
    delegate.CreateFetcherWithContext(url, URLFetcher::GET, request_context());
    delegate.StartFetcherAndWait();

    EXPECT_TRUE(delegate.fetcher()->GetStatus().is_success());
    EXPECT_EQ(200, delegate.fetcher()->GetResponseCode());
  }

  // 20 requests were sent. Due to throttling, they should have collectively
  // taken over 1 second.
  EXPECT_GE(Time::Now() - start_time, base::TimeDelta::FromSeconds(1));
}

TEST_F(URLFetcherTest, ThrottleOn5xxRetries) {
  base::Time start_time = Time::Now();
  GURL url(test_server_->GetURL("files/server-unavailable.html"));

  // Registers an entry for test url. The backoff time is calculated by:
  //     new_backoff = 2.0 * old_backoff + 0
  // and maximum backoff time is 256 milliseconds.
  // Maximum retries allowed is set to 11.
  scoped_refptr<URLRequestThrottlerEntry> entry(new URLRequestThrottlerEntry(
      request_context()->throttler_manager(), std::string() /* url_id */,
      200 /* sliding_window_period_ms */, 3 /* max_send_threshold */,
      1 /* initial_backoff_ms */, 2.0 /* multiply_factor */,
      0.0 /* jitter_factor */, 256 /* maximum_backoff_ms */));
  request_context()->throttler_manager()
      ->OverrideEntryForTests(url, entry.get());

  request_context()->throttler_manager()->OverrideEntryForTests(url,
                                                                entry.get());

  WaitingURLFetcherDelegate delegate;
  delegate.CreateFetcherWithContext(url, URLFetcher::GET, request_context());
  delegate.fetcher()->SetAutomaticallyRetryOn5xx(true);
  delegate.fetcher()->SetMaxRetriesOn5xx(11);
  delegate.StartFetcherAndWait();

  EXPECT_TRUE(delegate.fetcher()->GetStatus().is_success());
  EXPECT_EQ(503, delegate.fetcher()->GetResponseCode());
  std::string data;
  ASSERT_TRUE(delegate.fetcher()->GetResponseAsString(&data));
  EXPECT_FALSE(data.empty());

  // The request should have been retried 11 times (12 times including the first
  // attempt).  Due to throttling, they should have collectively taken over 1
  // second.
  EXPECT_GE(Time::Now() - start_time, base::TimeDelta::FromSeconds(1));
}

// Tests overload protection, when responses passed through.
TEST_F(URLFetcherTest, ProtectTestPassedThrough) {
  base::Time start_time = Time::Now();
  GURL url(test_server_->GetURL("files/server-unavailable.html"));

  // Registers an entry for test url. The backoff time is calculated by:
  //     new_backoff = 2.0 * old_backoff + 0
  // and maximum backoff time is 150000 milliseconds.
  // Maximum retries allowed is set to 11.
  scoped_refptr<URLRequestThrottlerEntry> entry(new URLRequestThrottlerEntry(
      request_context()->throttler_manager(), std::string() /* url_id */,
      200 /* sliding_window_period_ms */, 3 /* max_send_threshold */,
      10000 /* initial_backoff_ms */, 2.0 /* multiply_factor */,
      0.0 /* jitter_factor */, 150000 /* maximum_backoff_ms */));
  // Total time if *not* for not doing automatic backoff would be 150s.
  // In reality it should be "as soon as server responds".
  request_context()->throttler_manager()
      ->OverrideEntryForTests(url, entry.get());

  WaitingURLFetcherDelegate delegate;
  delegate.CreateFetcherWithContext(url, URLFetcher::GET, request_context());
  delegate.fetcher()->SetAutomaticallyRetryOn5xx(false);
  delegate.fetcher()->SetMaxRetriesOn5xx(11);
  delegate.StartFetcherAndWait();

  EXPECT_TRUE(delegate.fetcher()->GetStatus().is_success());
  EXPECT_EQ(503, delegate.fetcher()->GetResponseCode());
  std::string data;
  ASSERT_TRUE(delegate.fetcher()->GetResponseAsString(&data));
  EXPECT_FALSE(data.empty());
  EXPECT_GT(delegate.fetcher()->GetBackoffDelay().InMicroseconds(), 0);

  // The request should not have been retried at all.  If it had attempted all
  // 11 retries, that should have taken 2.5 minutes.
  EXPECT_TRUE(Time::Now() - start_time < TimeDelta::FromMinutes(1));
}

TEST_F(URLFetcherBadHTTPSTest, BadHTTPSTest) {
  CreateFetcher(test_server_->GetURL(kDefaultResponsePath));
  base::MessageLoop::current()->Run();
}

TEST_F(URLFetcherCancelTest, ReleasesContext) {
  GURL url(test_server_->GetURL("files/server-unavailable.html"));

  // Create a separate thread that will create the URLFetcher.  The current
  // (main) thread will do the IO, and when the fetch is complete it will
  // terminate the main thread's message loop; then the other thread's
  // message loop will be shut down automatically as the thread goes out of
  // scope.
  base::Thread t("URLFetcher test thread");
  ASSERT_TRUE(t.Start());
  t.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&URLFetcherCancelTest::CreateFetcher,
                 base::Unretained(this), url));

  base::MessageLoop::current()->Run();
}

TEST_F(URLFetcherCancelTest, CancelWhileDelayedStartTaskPending) {
  GURL url(test_server_->GetURL("files/server-unavailable.html"));

  // Register an entry for test url.
  // Using a sliding window of 4 seconds, and max of 1 request, under a fast
  // run we expect to have a 4 second delay when posting the Start task.
  scoped_refptr<URLRequestThrottlerEntry> entry(
      new URLRequestThrottlerEntry(request_context()->throttler_manager(),
                                   std::string(),
                                   4000,
                                   1,
                                   2000,
                                   2.0,
                                   0.0,
                                   4000));
  request_context()->throttler_manager()
      ->OverrideEntryForTests(url, entry.get());
  // Fake that a request has just started.
  entry->ReserveSendingTimeForNextRequest(base::TimeTicks());

  // The next request we try to send will be delayed by ~4 seconds.
  // The slower the test runs, the less the delay will be (since it takes the
  // time difference from now).

  base::Thread t("URLFetcher test thread");
  ASSERT_TRUE(t.Start());
  t.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&URLFetcherTest::CreateFetcher, base::Unretained(this), url));

  base::MessageLoop::current()->Run();
}

TEST_F(URLFetcherMultipleAttemptTest, SameData) {
  // Create the fetcher on the main thread.  Since IO will happen on the main
  // thread, this will test URLFetcher's ability to do everything on one
  // thread.
  CreateFetcher(test_server_->GetURL(kDefaultResponsePath));

  base::MessageLoop::current()->Run();
}

TEST_F(URLFetcherFileTest, SmallGet) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  // Get a small file.
  static const char kFileToFetch[] = "simple.html";
  expected_file_ = test_server_->GetDocumentRoot().AppendASCII(kFileToFetch);
  CreateFetcherForFile(
      test_server_->GetURL(std::string(kTestServerFilePrefix) + kFileToFetch),
      temp_dir.path().AppendASCII(kFileToFetch));

  base::MessageLoop::current()->Run();  // OnURLFetchComplete() will Quit().

  ASSERT_FALSE(base::PathExists(file_path_))
      << file_path_.value() << " not removed.";
}

TEST_F(URLFetcherFileTest, LargeGet) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  // Get a file large enough to require more than one read into
  // URLFetcher::Core's IOBuffer.
  static const char kFileToFetch[] = "animate1.gif";
  expected_file_ = test_server_->GetDocumentRoot().AppendASCII(kFileToFetch);
  CreateFetcherForFile(
      test_server_->GetURL(std::string(kTestServerFilePrefix) + kFileToFetch),
      temp_dir.path().AppendASCII(kFileToFetch));

  base::MessageLoop::current()->Run();  // OnURLFetchComplete() will Quit().
}

TEST_F(URLFetcherFileTest, SavedOutputFileOwnerhisp) {
  // If the caller takes the ownership of the output file, the file should
  // persist even after URLFetcher is gone. If not, the file must be deleted.
  const bool kTake[] = {false, true};
  for (size_t i = 0; i < arraysize(kTake); ++i) {
    take_ownership_of_file_ = kTake[i];
    base::ScopedTempDir temp_dir;
    ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

    // Get a small file.
    static const char kFileToFetch[] = "simple.html";
    expected_file_ = test_server_->GetDocumentRoot().AppendASCII(kFileToFetch);
    CreateFetcherForFile(
        test_server_->GetURL(std::string(kTestServerFilePrefix) + kFileToFetch),
        temp_dir.path().AppendASCII(kFileToFetch));

    base::MessageLoop::current()->Run();  // OnURLFetchComplete() will Quit().

    base::MessageLoop::current()->RunUntilIdle();
    ASSERT_EQ(kTake[i], base::PathExists(file_path_)) <<
        "FilePath: " << file_path_.value();
  }
}

TEST_F(URLFetcherFileTest, OverwriteExistingFile) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  // Create a file before trying to fetch.
  static const char kFileToFetch[] = "simple.html";
  std::string data(10000, '?');  // Meant to be larger than simple.html.
  file_path_ = temp_dir.path().AppendASCII(kFileToFetch);
  ASSERT_EQ(static_cast<int>(data.size()),
            base::WriteFile(file_path_, data.data(), data.size()));
  ASSERT_TRUE(base::PathExists(file_path_));
  expected_file_ = test_server_->GetDocumentRoot().AppendASCII(kFileToFetch);
  ASSERT_FALSE(base::ContentsEqual(file_path_, expected_file_));

  // Get a small file.
  CreateFetcherForFile(
      test_server_->GetURL(std::string(kTestServerFilePrefix) + kFileToFetch),
      file_path_);

  base::MessageLoop::current()->Run();  // OnURLFetchComplete() will Quit().
}

TEST_F(URLFetcherFileTest, TryToOverwriteDirectory) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  // Create a directory before trying to fetch.
  static const char kFileToFetch[] = "simple.html";
  file_path_ = temp_dir.path().AppendASCII(kFileToFetch);
  ASSERT_TRUE(base::CreateDirectory(file_path_));
  ASSERT_TRUE(base::PathExists(file_path_));

  // Get a small file.
  expected_file_error_ = ERR_ACCESS_DENIED;
  expected_file_ = test_server_->GetDocumentRoot().AppendASCII(kFileToFetch);
  CreateFetcherForFile(
      test_server_->GetURL(std::string(kTestServerFilePrefix) + kFileToFetch),
      file_path_);

  base::MessageLoop::current()->Run();  // OnURLFetchComplete() will Quit().

  base::MessageLoop::current()->RunUntilIdle();
}

TEST_F(URLFetcherFileTest, SmallGetToTempFile) {
  // Get a small file.
  static const char kFileToFetch[] = "simple.html";
  expected_file_ = test_server_->GetDocumentRoot().AppendASCII(kFileToFetch);
  CreateFetcherForTempFile(
      test_server_->GetURL(std::string(kTestServerFilePrefix) + kFileToFetch));

  base::MessageLoop::current()->Run();  // OnURLFetchComplete() will Quit().

  ASSERT_FALSE(base::PathExists(file_path_))
      << file_path_.value() << " not removed.";
}

TEST_F(URLFetcherFileTest, LargeGetToTempFile) {
  // Get a file large enough to require more than one read into
  // URLFetcher::Core's IOBuffer.
  static const char kFileToFetch[] = "animate1.gif";
  expected_file_ = test_server_->GetDocumentRoot().AppendASCII(kFileToFetch);
  CreateFetcherForTempFile(
      test_server_->GetURL(std::string(kTestServerFilePrefix) + kFileToFetch));

  base::MessageLoop::current()->Run();  // OnURLFetchComplete() will Quit().
}

TEST_F(URLFetcherFileTest, SavedOutputTempFileOwnerhisp) {
  // If the caller takes the ownership of the temp file, the file should persist
  // even after URLFetcher is gone. If not, the file must be deleted.
  const bool kTake[] = {false, true};
  for (size_t i = 0; i < arraysize(kTake); ++i) {
    take_ownership_of_file_ = kTake[i];

    // Get a small file.
    static const char kFileToFetch[] = "simple.html";
    expected_file_ = test_server_->GetDocumentRoot().AppendASCII(kFileToFetch);
    CreateFetcherForTempFile(test_server_->GetURL(
        std::string(kTestServerFilePrefix) + kFileToFetch));

    base::MessageLoop::current()->Run();  // OnURLFetchComplete() will Quit().

    base::MessageLoop::current()->RunUntilIdle();
    ASSERT_EQ(kTake[i], base::PathExists(file_path_)) <<
        "FilePath: " << file_path_.value();
  }
}

}  // namespace

}  // namespace net
