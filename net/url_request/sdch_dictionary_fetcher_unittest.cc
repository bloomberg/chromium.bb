// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/sdch_dictionary_fetcher.h"

#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/thread_task_runner_handle.h"
#include "net/base/sdch_manager.h"
#include "net/url_request/url_request_data_job.h"
#include "net/url_request/url_request_filter.h"
#include "net/url_request/url_request_interceptor.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace net {

namespace {

const char kSampleBufferContext[] = "This is a sample buffer.";
const char kTestDomain[] = "top.domain.test";

class URLRequestSpecifiedResponseJob : public URLRequestSimpleJob {
 public:
  URLRequestSpecifiedResponseJob(URLRequest* request,
                                 NetworkDelegate* network_delegate,
                                 const base::Closure& destruction_callback)
      : URLRequestSimpleJob(request, network_delegate),
        destruction_callback_(destruction_callback) {
    DCHECK(!destruction_callback.is_null());
  }

  static std::string ExpectedResponseForURL(const GURL& url) {
    return base::StringPrintf("Response for %s\n%s\nEnd Response for %s\n",
                              url.spec().c_str(),
                              kSampleBufferContext,
                              url.spec().c_str());
  }

 private:
  ~URLRequestSpecifiedResponseJob() override { destruction_callback_.Run(); }

  // URLRequestSimpleJob implementation:
  int GetData(std::string* mime_type,
              std::string* charset,
              std::string* data,
              const CompletionCallback& callback) const override {
    GURL url(request_->url());
    *data = ExpectedResponseForURL(url);
    return OK;
  }

  const base::Closure destruction_callback_;
};

class SpecifiedResponseJobInterceptor : public URLRequestInterceptor {
 public:
  // A callback to be called whenever a URLRequestSpecifiedResponseJob is
  // created or destroyed.  The argument will be the change in number of
  // jobs (i.e. +1 for created, -1 for destroyed).
  typedef base::Callback<void(int outstanding_job_delta)> LifecycleCallback;

  explicit SpecifiedResponseJobInterceptor(
      const LifecycleCallback& lifecycle_callback)
      : lifecycle_callback_(lifecycle_callback), factory_(this) {
    DCHECK(!lifecycle_callback_.is_null());
  }
  ~SpecifiedResponseJobInterceptor() override {}

  URLRequestJob* MaybeInterceptRequest(
      URLRequest* request,
      NetworkDelegate* network_delegate) const override {
    if (!lifecycle_callback_.is_null())
      lifecycle_callback_.Run(1);

    return new URLRequestSpecifiedResponseJob(
        request, network_delegate, base::Bind(lifecycle_callback_, -1));
  }

  // The caller must ensure that the callback is valid to call for the
  // lifetime of the SpecifiedResponseJobInterceptor (i.e. until
  // Unregister() is called).
  static void RegisterWithFilter(const LifecycleCallback& lifecycle_callback) {
    scoped_ptr<SpecifiedResponseJobInterceptor> interceptor(
        new SpecifiedResponseJobInterceptor(lifecycle_callback));

    net::URLRequestFilter::GetInstance()->AddHostnameInterceptor(
        "http", kTestDomain, interceptor.Pass());
  }

  static void Unregister() {
    net::URLRequestFilter::GetInstance()->RemoveHostnameHandler("http",
                                                                kTestDomain);
  }

 private:
  void OnSpecfiedResponseJobDestruction() const {
    if (!lifecycle_callback_.is_null())
      lifecycle_callback_.Run(-1);
  }

  LifecycleCallback lifecycle_callback_;
  mutable base::WeakPtrFactory<SpecifiedResponseJobInterceptor> factory_;
};

// Local test infrastructure
// * URLRequestSpecifiedResponseJob: A URLRequestJob that returns
//   a different but derivable response for each URL (used for all
//   url requests in this file).  The class provides interfaces to
//   signal whenever the total number of jobs transitions to zero.
// * SdchDictionaryFetcherTest: Registers a callback with the above
//   class, and provides blocking interfaces for a transition to zero jobs.
//   Contains an SdchDictionaryFetcher, and tracks fetcher dictionary
//   addition callbacks.
// Most tests schedule a dictionary fetch, wait for no jobs outstanding,
// then test that the fetch results are as expected.

class SdchDictionaryFetcherTest : public ::testing::Test {
 public:
  struct DictionaryAdditions {
    DictionaryAdditions(const std::string& dictionary_text,
                        const GURL& dictionary_url)
        : dictionary_text(dictionary_text), dictionary_url(dictionary_url) {}

    std::string dictionary_text;
    GURL dictionary_url;
  };

  SdchDictionaryFetcherTest()
      : jobs_requested_(0),
        jobs_outstanding_(0),
        context_(new TestURLRequestContext),
        fetcher_(new SdchDictionaryFetcher(
            context_.get(),
            base::Bind(&SdchDictionaryFetcherTest::OnDictionaryFetched,
                       base::Unretained(this)))),
        factory_(this) {
    SpecifiedResponseJobInterceptor::RegisterWithFilter(
        base::Bind(&SdchDictionaryFetcherTest::OnNumberJobsChanged,
                   factory_.GetWeakPtr()));
  }

  ~SdchDictionaryFetcherTest() override {
    SpecifiedResponseJobInterceptor::Unregister();
  }

  void OnDictionaryFetched(const std::string& dictionary_text,
                           const GURL& dictionary_url,
                           const BoundNetLog& net_log) {
    dictionary_additions.push_back(
        DictionaryAdditions(dictionary_text, dictionary_url));
  }

  // Return (in |*out|) all dictionary additions since the last time
  // this function was called.
  void GetDictionaryAdditions(std::vector<DictionaryAdditions>* out) {
    out->swap(dictionary_additions);
    dictionary_additions.clear();
  }

  SdchDictionaryFetcher* fetcher() { return fetcher_.get(); }

  // May not be called outside the SetUp()/TearDown() interval.
  int jobs_requested() const { return jobs_requested_; }

  GURL PathToGurl(const char* path) {
    std::string gurl_string("http://");
    gurl_string += kTestDomain;
    gurl_string += "/";
    gurl_string += path;
    return GURL(gurl_string);
  }

  // Block until there are no outstanding URLRequestSpecifiedResponseJobs.
  void WaitForNoJobs() {
    if (jobs_outstanding_ == 0)
      return;

    run_loop_.reset(new base::RunLoop);
    run_loop_->Run();
    run_loop_.reset();
  }

 private:
  void OnNumberJobsChanged(int outstanding_jobs_delta) {
    if (outstanding_jobs_delta > 0)
      jobs_requested_ += outstanding_jobs_delta;
    jobs_outstanding_ += outstanding_jobs_delta;
    if (jobs_outstanding_ == 0 && run_loop_)
      run_loop_->Quit();
  }

  int jobs_requested_;
  int jobs_outstanding_;
  scoped_ptr<base::RunLoop> run_loop_;
  scoped_ptr<TestURLRequestContext> context_;
  scoped_ptr<SdchDictionaryFetcher> fetcher_;
  std::vector<DictionaryAdditions> dictionary_additions;
  base::WeakPtrFactory<SdchDictionaryFetcherTest> factory_;
};

// Schedule a fetch and make sure it happens.
TEST_F(SdchDictionaryFetcherTest, Basic) {
  GURL dictionary_url(PathToGurl("dictionary"));
  fetcher()->Schedule(dictionary_url);
  WaitForNoJobs();

  EXPECT_EQ(1, jobs_requested());
  std::vector<DictionaryAdditions> additions;
  GetDictionaryAdditions(&additions);
  ASSERT_EQ(1u, additions.size());
  EXPECT_EQ(
      URLRequestSpecifiedResponseJob::ExpectedResponseForURL(dictionary_url),
      additions[0].dictionary_text);
}

// Multiple fetches of the same URL should result in only one request.
TEST_F(SdchDictionaryFetcherTest, Multiple) {
  GURL dictionary_url(PathToGurl("dictionary"));
  fetcher()->Schedule(dictionary_url);
  fetcher()->Schedule(dictionary_url);
  fetcher()->Schedule(dictionary_url);
  WaitForNoJobs();

  EXPECT_EQ(1, jobs_requested());
  std::vector<DictionaryAdditions> additions;
  GetDictionaryAdditions(&additions);
  ASSERT_EQ(1u, additions.size());
  EXPECT_EQ(
      URLRequestSpecifiedResponseJob::ExpectedResponseForURL(dictionary_url),
      additions[0].dictionary_text);
}

// A cancel should result in no actual requests being generated.
TEST_F(SdchDictionaryFetcherTest, Cancel) {
  GURL dictionary_url_1(PathToGurl("dictionary_1"));
  GURL dictionary_url_2(PathToGurl("dictionary_2"));
  GURL dictionary_url_3(PathToGurl("dictionary_3"));

  fetcher()->Schedule(dictionary_url_1);
  fetcher()->Schedule(dictionary_url_2);
  fetcher()->Schedule(dictionary_url_3);
  fetcher()->Cancel();
  WaitForNoJobs();

  // Synchronous execution may have resulted in a single job being scheduled.
  EXPECT_GE(1, jobs_requested());
}

// Attempt to confuse the fetcher loop processing by scheduling a
// dictionary addition while another fetch is in process.
TEST_F(SdchDictionaryFetcherTest, LoopRace) {
  GURL dictionary0_url(PathToGurl("dictionary0"));
  GURL dictionary1_url(PathToGurl("dictionary1"));
  fetcher()->Schedule(dictionary0_url);
  fetcher()->Schedule(dictionary1_url);
  WaitForNoJobs();

  ASSERT_EQ(2, jobs_requested());
  std::vector<DictionaryAdditions> additions;
  GetDictionaryAdditions(&additions);
  ASSERT_EQ(2u, additions.size());
  EXPECT_EQ(
      URLRequestSpecifiedResponseJob::ExpectedResponseForURL(dictionary0_url),
      additions[0].dictionary_text);
  EXPECT_EQ(
      URLRequestSpecifiedResponseJob::ExpectedResponseForURL(dictionary1_url),
      additions[1].dictionary_text);
}

}  // namespace

}  // namespace net
