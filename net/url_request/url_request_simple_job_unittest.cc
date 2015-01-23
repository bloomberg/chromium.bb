// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/threading/worker_pool.h"
#include "net/base/request_priority.h"
#include "net/url_request/url_request_job.h"
#include "net/url_request/url_request_job_factory.h"
#include "net/url_request/url_request_job_factory_impl.h"
#include "net/url_request/url_request_simple_job.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

const char kTestData[] = "Huge data array";
const int kRangeFirstPosition = 5;
const int kRangeLastPosition = 8;
static_assert(kRangeFirstPosition > 0 &&
                  kRangeFirstPosition < kRangeLastPosition &&
                  kRangeLastPosition <
                      static_cast<int>(arraysize(kTestData) - 1),
              "invalid range");

// This function does nothing.
void DoNothing() {
}

class MockSimpleJob : public URLRequestSimpleJob {
 public:
  MockSimpleJob(URLRequest* request,
                NetworkDelegate* network_delegate,
                scoped_refptr<base::TaskRunner> task_runner,
                std::string data)
      : URLRequestSimpleJob(request, network_delegate),
        data_(data),
        task_runner_(task_runner) {}

 protected:
  // URLRequestSimpleJob implementation:
  int GetData(std::string* mime_type,
              std::string* charset,
              std::string* data,
              const CompletionCallback& callback) const override {
    mime_type->assign("text/plain");
    charset->assign("US-ASCII");
    data->assign(data_);
    return OK;
  }

  base::TaskRunner* GetTaskRunner() const override {
    return task_runner_.get();
  }

 private:
  ~MockSimpleJob() override {}

  const std::string data_;

  scoped_refptr<base::TaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(MockSimpleJob);
};

class CancelURLRequestDelegate : public net::URLRequest::Delegate {
 public:
  explicit CancelURLRequestDelegate()
      : buf_(new IOBuffer(kBufferSize)), run_loop_(new base::RunLoop) {}

  void OnResponseStarted(net::URLRequest* request) override {
    int bytes_read = 0;
    EXPECT_FALSE(request->Read(buf_.get(), kBufferSize, &bytes_read));
    EXPECT_TRUE(request->status().is_io_pending());
    request->Cancel();
    run_loop_->Quit();
  }

  void OnReadCompleted(URLRequest* request, int bytes_read) override {}

  void WaitUntilHeadersReceived() const { run_loop_->Run(); }

 private:
  static const int kBufferSize = 4096;
  scoped_refptr<IOBuffer> buf_;
  scoped_ptr<base::RunLoop> run_loop_;
};

class SimpleJobProtocolHandler :
    public URLRequestJobFactory::ProtocolHandler {
 public:
  SimpleJobProtocolHandler(scoped_refptr<base::TaskRunner> task_runner)
      : task_runner_(task_runner) {}
  URLRequestJob* MaybeCreateJob(
      URLRequest* request,
      NetworkDelegate* network_delegate) const override {
    if (request->url().spec() == "data:empty")
      return new MockSimpleJob(request, network_delegate, task_runner_, "");
    return new MockSimpleJob(request, network_delegate, task_runner_,
                             kTestData);
  }

 private:
  scoped_refptr<base::TaskRunner> task_runner_;

  ~SimpleJobProtocolHandler() override {}
};

class URLRequestSimpleJobTest : public ::testing::Test {
 public:
  URLRequestSimpleJobTest()
      : worker_pool_(
            new base::SequencedWorkerPool(1, "URLRequestSimpleJobTest")),
        task_runner_(worker_pool_->GetSequencedTaskRunnerWithShutdownBehavior(
            worker_pool_->GetSequenceToken(),
            base::SequencedWorkerPool::SKIP_ON_SHUTDOWN)),
        context_(true) {
    job_factory_.SetProtocolHandler("data",
                                    new SimpleJobProtocolHandler(task_runner_));
    context_.set_job_factory(&job_factory_);
    context_.Init();

    request_ = context_.CreateRequest(
        GURL("data:test"), DEFAULT_PRIORITY, &delegate_, NULL);
  }

  ~URLRequestSimpleJobTest() override { worker_pool_->Shutdown(); }

  void StartRequest(const HttpRequestHeaders* headers) {
    if (headers)
      request_->SetExtraRequestHeaders(*headers);
    request_->Start();

    EXPECT_TRUE(request_->is_pending());
    base::RunLoop().Run();
    EXPECT_FALSE(request_->is_pending());
  }

  void TearDown() override { worker_pool_->Shutdown(); }

 protected:
  scoped_refptr<base::SequencedWorkerPool> worker_pool_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  TestURLRequestContext context_;
  URLRequestJobFactoryImpl job_factory_;
  TestDelegate delegate_;
  scoped_ptr<URLRequest> request_;
};

}  // namespace

TEST_F(URLRequestSimpleJobTest, SimpleRequest) {
  StartRequest(NULL);
  ASSERT_TRUE(request_->status().is_success());
  EXPECT_EQ(kTestData, delegate_.data_received());
}

TEST_F(URLRequestSimpleJobTest, RangeRequest) {
  const std::string kExpectedBody = std::string(
      kTestData + kRangeFirstPosition, kTestData + kRangeLastPosition + 1);
  HttpRequestHeaders headers;
  headers.SetHeader(
      HttpRequestHeaders::kRange,
      HttpByteRange::Bounded(kRangeFirstPosition, kRangeLastPosition)
          .GetHeaderValue());

  StartRequest(&headers);

  ASSERT_TRUE(request_->status().is_success());
  EXPECT_EQ(kExpectedBody, delegate_.data_received());
}

TEST_F(URLRequestSimpleJobTest, MultipleRangeRequest) {
  HttpRequestHeaders headers;
  int middle_pos = (kRangeFirstPosition + kRangeLastPosition)/2;
  std::string range = base::StringPrintf("bytes=%d-%d,%d-%d",
                                         kRangeFirstPosition,
                                         middle_pos,
                                         middle_pos + 1,
                                         kRangeLastPosition);
  headers.SetHeader(HttpRequestHeaders::kRange, range);

  StartRequest(&headers);

  EXPECT_TRUE(delegate_.request_failed());
  EXPECT_EQ(ERR_REQUEST_RANGE_NOT_SATISFIABLE, request_->status().error());
}

TEST_F(URLRequestSimpleJobTest, InvalidRangeRequest) {
  HttpRequestHeaders headers;
  std::string range = base::StringPrintf(
      "bytes=%d-%d", kRangeLastPosition, kRangeFirstPosition);
  headers.SetHeader(HttpRequestHeaders::kRange, range);

  StartRequest(&headers);

  ASSERT_TRUE(request_->status().is_success());
  EXPECT_EQ(kTestData, delegate_.data_received());
}

TEST_F(URLRequestSimpleJobTest, EmptyDataRequest) {
  request_ = context_.CreateRequest(GURL("data:empty"), DEFAULT_PRIORITY,
                                    &delegate_, NULL);
  StartRequest(nullptr);
  ASSERT_TRUE(request_->status().is_success());
  EXPECT_EQ("", delegate_.data_received());
}

TEST_F(URLRequestSimpleJobTest, CancelAfterFirstRead) {
  scoped_ptr<CancelURLRequestDelegate> cancel_delegate(
      new CancelURLRequestDelegate());
  request_ = context_.CreateRequest(GURL("data:cancel"), DEFAULT_PRIORITY,
                                    cancel_delegate.get(), NULL);
  request_->Start();
  cancel_delegate->WaitUntilHeadersReceived();

  // Feed a dummy task to the SequencedTaskRunner to make sure that the
  // callbacks which are invoked in ReadRawData have completed safely.
  base::RunLoop run_loop;
  EXPECT_TRUE(task_runner_->PostTaskAndReply(FROM_HERE, base::Bind(&DoNothing),
                                             run_loop.QuitClosure()));
  run_loop.Run();
}

}  // namespace net
