// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string.h>
#include <algorithm>
#include <string>
#include <vector>
#include "base/message_loop.h"
#include "googleurl/src/gurl.h"
#include "net/base/filter.h"
#include "net/base/io_buffer.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_job.h"
#include "net/url_request/url_request_job_tracker.h"
#include "net/url_request/url_request_status.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

using testing::Eq;
using testing::InSequence;
using testing::NotNull;
using testing::StrictMock;

namespace net {
namespace {

const char kBasic[] = "Hello\n";

// The above string "Hello\n", gzip compressed.
const unsigned char kCompressed[] = {
  0x1f, 0x8b, 0x08, 0x08, 0x38, 0x18, 0x2e, 0x4c, 0x00, 0x03, 0x63,
  0x6f, 0x6d, 0x70, 0x72, 0x65, 0x73, 0x73, 0x65, 0x64, 0x2e, 0x68,
  0x74, 0x6d, 0x6c, 0x00, 0xf3, 0x48, 0xcd, 0xc9, 0xc9, 0xe7, 0x02,
  0x00, 0x16, 0x35, 0x96, 0x31, 0x06, 0x00, 0x00, 0x00
};

bool GetResponseBody(const GURL& url, std::string* out_body) {
  if (url.spec() == "test:basic") {
    *out_body = kBasic;
  } else if (url.spec() == "test:compressed") {
    out_body->assign(reinterpret_cast<const char*>(kCompressed),
                     sizeof(kCompressed));
  } else {
    return false;
  }

  return true;
}

class MockJobObserver : public URLRequestJobTracker::JobObserver {
 public:
  MOCK_METHOD1(OnJobAdded, void(URLRequestJob* job));
  MOCK_METHOD1(OnJobRemoved, void(URLRequestJob* job));
  MOCK_METHOD2(OnJobDone, void(URLRequestJob* job,
                               const URLRequestStatus& status));
  MOCK_METHOD3(OnJobRedirect, void(URLRequestJob* job,
                                   const GURL& location,
                                   int status_code));
  MOCK_METHOD3(OnBytesRead, void(URLRequestJob* job,
                                 const char* buf,
                                 int byte_count));
};

// A URLRequestJob that returns static content for given URLs. We do
// not use URLRequestTestJob here because URLRequestTestJob fakes
// async operations by calling ReadRawData synchronously in an async
// callback. This test requires a URLRequestJob that returns false for
// async reads, in order to exercise the real async read codepath.
class URLRequestJobTrackerTestJob : public URLRequestJob {
 public:
  URLRequestJobTrackerTestJob(URLRequest* request, bool async_reads)
      : URLRequestJob(request), async_reads_(async_reads) {}

  void Start() {
    ASSERT_TRUE(GetResponseBody(request_->url(), &response_data_));

    // Start reading asynchronously so that all error reporting and data
    // callbacks happen as they would for network requests.
    MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(
        this, &URLRequestJobTrackerTestJob::NotifyHeadersComplete));
  }

  bool ReadRawData(IOBuffer* buf, int buf_size,
                   int *bytes_read) {
    const size_t bytes_to_read = std::min(
        response_data_.size(), static_cast<size_t>(buf_size));

    // Regardless of whether we're performing a sync or async read,
    // copy the data into the caller's buffer now. That way we don't
    // have to hold on to the buffers in the async case.
    memcpy(buf->data(), response_data_.data(), bytes_to_read);
    response_data_.erase(0, bytes_to_read);

    if (async_reads_) {
      SetStatus(URLRequestStatus(URLRequestStatus::IO_PENDING, 0));
      MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(
          this, &URLRequestJobTrackerTestJob::OnReadCompleted,
          bytes_to_read));
    } else {
      SetStatus(URLRequestStatus());
      *bytes_read = bytes_to_read;
    }
    return !async_reads_;
  }

  void OnReadCompleted(int status) {
    if (status == 0) {
      NotifyDone(URLRequestStatus());
    } else if (status > 0) {
      SetStatus(URLRequestStatus());
    } else {
      ASSERT_FALSE(true) << "Unexpected OnReadCompleted callback.";
    }

    NotifyReadComplete(status);
  }

  bool GetContentEncodings(
      std::vector<Filter::FilterType>* encoding_types) {
    if (request_->url().spec() == "test:basic") {
      return false;
    } else if (request_->url().spec() == "test:compressed") {
      encoding_types->push_back(Filter::FILTER_TYPE_GZIP);
      return true;
    } else {
      return URLRequestJob::GetContentEncodings(encoding_types);
    }
  }

  // The data to send, will be set in Start().
  std::string response_data_;

  // Should reads be synchronous or asynchronous?
  const bool async_reads_;
};

// Google Mock Matcher to check two URLRequestStatus instances for
// equality.
MATCHER_P(StatusEq, other, "") {
  return (arg.status() == other.status() &&
          arg.os_error() == other.os_error());
}

// Google Mock Matcher to check that two blocks of memory are equal.
MATCHER_P2(MemEq, other, len, "") {
  return memcmp(arg, other, len) == 0;
}

class URLRequestJobTrackerTest : public PlatformTest {
 protected:
  static void SetUpTestCase() {
    URLRequest::RegisterProtocolFactory("test", &Factory);
  }

  virtual void SetUp() {
    g_async_reads = true;
  }

  void AssertJobTrackerCallbacks(const char* url) {
    InSequence seq;
    testing::StrictMock<MockJobObserver> observer;

    const GURL gurl(url);
    std::string body;
    ASSERT_TRUE(GetResponseBody(gurl, &body));

    // We expect to receive one call for each method on the JobObserver,
    // in the following order:
    EXPECT_CALL(observer, OnJobAdded(NotNull()));
    EXPECT_CALL(observer, OnBytesRead(NotNull(),
                                      MemEq(body.data(), body.size()),
                                      Eq(static_cast<int>(body.size()))));
    EXPECT_CALL(observer, OnJobDone(NotNull(),
                StatusEq(URLRequestStatus())));
    EXPECT_CALL(observer, OnJobRemoved(NotNull()));

    // Attach our observer and perform the resource fetch.
    g_url_request_job_tracker.AddObserver(&observer);
    Fetch(gurl);
    g_url_request_job_tracker.RemoveObserver(&observer);
  }

  void Fetch(const GURL& url) {
    TestDelegate d;
    {
      URLRequest request(url, &d);
      request.Start();
      MessageLoop::current()->RunAllPending();
    }

    // A few sanity checks to make sure that the delegate also
    // receives the expected callbacks.
    EXPECT_EQ(1, d.response_started_count());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_STREQ(kBasic, d.data_received().c_str());
  }

  static URLRequest::ProtocolFactory Factory;
  static bool g_async_reads;
};

// static
URLRequestJob* URLRequestJobTrackerTest::Factory(
    URLRequest* request,
    const std::string& scheme) {
  return new URLRequestJobTrackerTestJob(request, g_async_reads);
}

// static
bool URLRequestJobTrackerTest::g_async_reads = true;

TEST_F(URLRequestJobTrackerTest, BasicAsync) {
  g_async_reads = true;
  AssertJobTrackerCallbacks("test:basic");
}

TEST_F(URLRequestJobTrackerTest, BasicSync) {
  g_async_reads = false;
  AssertJobTrackerCallbacks("test:basic");
}

TEST_F(URLRequestJobTrackerTest, CompressedAsync) {
  g_async_reads = true;
  AssertJobTrackerCallbacks("test:compressed");
}

TEST_F(URLRequestJobTrackerTest, CompressedSync) {
  g_async_reads = false;
  AssertJobTrackerCallbacks("test:compressed");
}

}  // namespace
}  // namespace net
