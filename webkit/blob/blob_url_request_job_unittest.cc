// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stack>
#include <utility>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/message_loop_proxy.h"
#include "base/scoped_temp_dir.h"
#include "base/task.h"
#include "base/threading/thread.h"
#include "base/time.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/synchronization/waitable_event.h"
#include "net/base/file_stream.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_error_job.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/blob/blob_data.h"
#include "webkit/blob/blob_url_request_job.h"

namespace webkit_blob {

static const int kBufferSize = 1024;
static const char kTestData1[] = "Hello";
static const char kTestData2[] = "Here it is data.";
static const char kTestFileData1[] = "0123456789";
static const char kTestFileData2[] = "This is sample file.";
static const char kTestContentType[] = "foo/bar";
static const char kTestContentDisposition[] = "attachment; filename=foo.txt";

class BlobURLRequestJobTest : public testing::Test {
 public:

  // Test Harness -------------------------------------------------------------
  // TODO(jianli): share this test harness with AppCacheURLRequestJobTest

  class MockURLRequestDelegate : public net::URLRequest::Delegate {
   public:
    explicit MockURLRequestDelegate(BlobURLRequestJobTest* test)
        : test_(test),
          received_data_(new net::IOBuffer(kBufferSize)) {
    }

    virtual void OnResponseStarted(net::URLRequest* request) {
      if (request->status().is_success()) {
        EXPECT_TRUE(request->response_headers());
        ReadSome(request);
      } else {
        RequestComplete();
      }
    }

    virtual void OnReadCompleted(net::URLRequest* request, int bytes_read) {
       if (bytes_read > 0)
         ReceiveData(request, bytes_read);
       else
         RequestComplete();
    }

    const std::string& response_data() const { return response_data_; }

   private:
    void ReadSome(net::URLRequest* request) {
      if (request->job()->is_done()) {
        RequestComplete();
        return;
      }

      int bytes_read = 0;
      if (!request->Read(received_data_.get(), kBufferSize, &bytes_read)) {
        if (!request->status().is_io_pending()) {
          RequestComplete();
        }
        return;
      }

      ReceiveData(request, bytes_read);
    }

    void ReceiveData(net::URLRequest* request, int bytes_read) {
      if (bytes_read) {
        response_data_.append(received_data_->data(),
                              static_cast<size_t>(bytes_read));
        ReadSome(request);
      } else {
        RequestComplete();
      }
    }

    void RequestComplete() {
      test_->ScheduleNextTask();
    }

    BlobURLRequestJobTest* test_;
    scoped_refptr<net::IOBuffer> received_data_;
    std::string response_data_;
  };

  // Helper class run a test on our io_thread. The io_thread
  // is spun up once and reused for all tests.
  template <class Method>
  class WrapperTask : public Task {
   public:
    WrapperTask(BlobURLRequestJobTest* test, Method method)
        : test_(test), method_(method) {
    }

    virtual void Run() {
      test_->SetUpTest();
      (test_->*method_)();
    }

   private:
    BlobURLRequestJobTest* test_;
    Method method_;
  };

  void SetUp() {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    temp_file1_ = temp_dir_.path().AppendASCII("BlobFile1.dat");
    ASSERT_EQ(static_cast<int>(arraysize(kTestFileData1) - 1),
              file_util::WriteFile(temp_file1_, kTestFileData1,
                                   arraysize(kTestFileData1) - 1));
    base::PlatformFileInfo file_info1;
    file_util::GetFileInfo(temp_file1_, &file_info1);
    temp_file_modification_time1_ = file_info1.last_modified;

    temp_file2_ = temp_dir_.path().AppendASCII("BlobFile2.dat");
    ASSERT_EQ(static_cast<int>(arraysize(kTestFileData2) - 1),
              file_util::WriteFile(temp_file2_, kTestFileData2,
                                   arraysize(kTestFileData2) - 1));
    base::PlatformFileInfo file_info2;
    file_util::GetFileInfo(temp_file2_, &file_info2);
    temp_file_modification_time2_ = file_info2.last_modified;

    io_thread_.reset(new base::Thread("BlobRLRequestJobTest Thread"));
    base::Thread::Options options(MessageLoop::TYPE_IO, 0);
    io_thread_->StartWithOptions(options);
  }

  void TearDown() {
    io_thread_.reset(NULL);
  }

  static net::URLRequestJob* BlobURLRequestJobFactory(
      net::URLRequest* request,
      const std::string& scheme) {
    BlobURLRequestJob* temp = blob_url_request_job_;
    blob_url_request_job_ = NULL;
    return temp;
  }

  BlobURLRequestJobTest()
      : expected_status_code_(0) {
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

    net::URLRequest::RegisterProtocolFactory("blob", &BlobURLRequestJobFactory);
    url_request_delegate_.reset(new MockURLRequestDelegate(this));
  }

  void TearDownTest() {
    DCHECK(MessageLoop::current() == io_thread_->message_loop());

    request_.reset();
    url_request_delegate_.reset();

    DCHECK(!blob_url_request_job_);
    net::URLRequest::RegisterProtocolFactory("blob", NULL);
  }

  void TestFinished() {
    // We unwind the stack prior to finishing up to let stack
    // based objects get deleted.
    DCHECK(MessageLoop::current() == io_thread_->message_loop());
    MessageLoop::current()->PostTask(FROM_HERE,
        NewRunnableMethod(
            this, &BlobURLRequestJobTest::TestFinishedUnwound));
  }

  void TestFinishedUnwound() {
    TearDownTest();
    test_finished_event_->Signal();
  }

  void PushNextTask(Task* task) {
    task_stack_.push(std::pair<Task*, bool>(task, false));
  }

  void PushNextTaskAsImmediate(Task* task) {
    task_stack_.push(std::pair<Task*, bool>(task, true));
  }

  void ScheduleNextTask() {
    DCHECK(MessageLoop::current() == io_thread_->message_loop());
    if (task_stack_.empty()) {
      TestFinished();
      return;
    }
    scoped_ptr<Task> task(task_stack_.top().first);
    bool immediate = task_stack_.top().second;
    task_stack_.pop();
    if (immediate)
      task->Run();
    else
      MessageLoop::current()->PostTask(FROM_HERE, task.release());
  }

  void TestSuccessRequest(BlobData* blob_data,
                          const std::string& expected_response) {
    PushNextTask(NewRunnableMethod(
        this, &BlobURLRequestJobTest::VerifyResponse));
    expected_status_code_ = 200;
    expected_response_ = expected_response;
    return TestRequest("GET", net::HttpRequestHeaders(), blob_data);
  }

  void TestErrorRequest(BlobData* blob_data,
                        int expected_status_code) {
    PushNextTask(NewRunnableMethod(
        this, &BlobURLRequestJobTest::VerifyResponse));
    expected_status_code_ = expected_status_code;
    expected_response_ = "";
    return TestRequest("GET", net::HttpRequestHeaders(), blob_data);
  }

  void TestRequest(const std::string& method,
                   const net::HttpRequestHeaders& extra_headers,
                   BlobData* blob_data) {
    // This test has async steps.
    request_.reset(new net::URLRequest(GURL("blob:blah"),
                                       url_request_delegate_.get()));
    request_->set_method(method);
    blob_url_request_job_ = new BlobURLRequestJob(
        request_.get(),
        blob_data,
        base::MessageLoopProxy::CreateForCurrentThread());

    // Start the request.
    if (!extra_headers.IsEmpty())
      request_->SetExtraRequestHeaders(extra_headers);
    request_->Start();

    // Completion is async.
  }

  void VerifyResponse() {
    EXPECT_TRUE(request_->status().is_success());
    EXPECT_EQ(request_->response_headers()->response_code(),
              expected_status_code_);
    EXPECT_STREQ(url_request_delegate_->response_data().c_str(),
                 expected_response_.c_str());
    TestFinished();
  }

  // Test Cases ---------------------------------------------------------------

  void TestGetSimpleDataRequest() {
    scoped_refptr<BlobData> blob_data(new BlobData());
    blob_data->AppendData(kTestData1);
    TestSuccessRequest(blob_data, kTestData1);
  }

  void TestGetSimpleFileRequest() {
    scoped_refptr<BlobData> blob_data(new BlobData());
    blob_data->AppendFile(temp_file1_, 0, -1, base::Time());
    TestSuccessRequest(blob_data, kTestFileData1);
  }

  void TestGetLargeFileRequest() {
    scoped_refptr<BlobData> blob_data(new BlobData());
    FilePath large_temp_file = temp_dir_.path().AppendASCII("LargeBlob.dat");
    std::string large_data;
    large_data.reserve(kBufferSize * 5);
    for (int i = 0; i < kBufferSize * 5; ++i)
      large_data.append(1, static_cast<char>(i % 256));
    ASSERT_EQ(static_cast<int>(large_data.size()),
              file_util::WriteFile(large_temp_file, large_data.data(),
                                   large_data.size()));
    blob_data->AppendFile(large_temp_file, 0, -1, base::Time());
    TestSuccessRequest(blob_data, large_data);
  }

  void TestGetNonExistentFileRequest() {
    FilePath non_existent_file =
        temp_file1_.InsertBeforeExtension(FILE_PATH_LITERAL("-na"));
    scoped_refptr<BlobData> blob_data(new BlobData());
    blob_data->AppendFile(non_existent_file, 0, -1, base::Time());
    TestErrorRequest(blob_data, 404);
  }

  void TestGetChangedFileRequest() {
    scoped_refptr<BlobData> blob_data(new BlobData());
    base::Time old_time =
        temp_file_modification_time1_ - base::TimeDelta::FromSeconds(10);
    blob_data->AppendFile(temp_file1_, 0, 3, old_time);
    TestErrorRequest(blob_data, 404);
  }

  void TestGetSlicedDataRequest() {
    scoped_refptr<BlobData> blob_data(new BlobData());
    blob_data->AppendData(kTestData2, 2, 4);
    std::string result(kTestData2 + 2, 4);
    TestSuccessRequest(blob_data, result);
  }

  void TestGetSlicedFileRequest() {
    scoped_refptr<BlobData> blob_data(new BlobData());
    blob_data->AppendFile(temp_file1_, 2, 4, temp_file_modification_time1_);
    std::string result(kTestFileData1 + 2, 4);
    TestSuccessRequest(blob_data, result);
  }

  scoped_refptr<BlobData> BuildComplicatedData(std::string* expected_result) {
    scoped_refptr<BlobData> blob_data(new BlobData());
    blob_data->AppendData(kTestData1, 1, 2);
    blob_data->AppendFile(temp_file1_, 2, 3, temp_file_modification_time1_);
    blob_data->AppendData(kTestData2, 3, 4);
    blob_data->AppendFile(temp_file2_, 4, 5, temp_file_modification_time2_);
    *expected_result = std::string(kTestData1 + 1, 2);
    *expected_result += std::string(kTestFileData1 + 2, 3);
    *expected_result += std::string(kTestData2 + 3, 4);
    *expected_result += std::string(kTestFileData2 + 4, 5);
    return blob_data;
  }

  void TestGetComplicatedDataAndFileRequest() {
    std::string result;
    scoped_refptr<BlobData> blob_data = BuildComplicatedData(&result);
    TestSuccessRequest(blob_data, result);
  }

  void TestGetRangeRequest1() {
    std::string result;
    scoped_refptr<BlobData> blob_data = BuildComplicatedData(&result);
    net::HttpRequestHeaders extra_headers;
    extra_headers.SetHeader(net::HttpRequestHeaders::kRange, "bytes=5-10");
    PushNextTask(NewRunnableMethod(
        this, &BlobURLRequestJobTest::VerifyResponse));
    expected_status_code_ = 206;
    expected_response_ = result.substr(5, 10 - 5 + 1);
    return TestRequest("GET", extra_headers, blob_data);
  }

  void TestGetRangeRequest2() {
    std::string result;
    scoped_refptr<BlobData> blob_data = BuildComplicatedData(&result);
    net::HttpRequestHeaders extra_headers;
    extra_headers.SetHeader(net::HttpRequestHeaders::kRange, "bytes=-10");
    PushNextTask(NewRunnableMethod(
        this, &BlobURLRequestJobTest::VerifyResponse));
    expected_status_code_ = 206;
    expected_response_ = result.substr(result.length() - 10);
    return TestRequest("GET", extra_headers, blob_data);
  }

  void TestExtraHeaders() {
    scoped_refptr<BlobData> blob_data(new BlobData());
    blob_data->set_content_type(kTestContentType);
    blob_data->set_content_disposition(kTestContentDisposition);
    blob_data->AppendData(kTestData1);
    PushNextTask(NewRunnableMethod(
        this, &BlobURLRequestJobTest::VerifyResponseForTestExtraHeaders));
    TestRequest("GET", net::HttpRequestHeaders(), blob_data);
  }

  void VerifyResponseForTestExtraHeaders() {
    EXPECT_TRUE(request_->status().is_success());
    EXPECT_EQ(request_->response_headers()->response_code(), 200);
    EXPECT_STREQ(url_request_delegate_->response_data().c_str(), kTestData1);
    std::string content_type;
    EXPECT_TRUE(request_->response_headers()->GetMimeType(&content_type));
    EXPECT_STREQ(content_type.c_str(), kTestContentType);
    void* iter = NULL;
    std::string content_disposition;
    EXPECT_TRUE(request_->response_headers()->EnumerateHeader(
        &iter, "Content-Disposition", &content_disposition));
    EXPECT_STREQ(content_disposition.c_str(), kTestContentDisposition);
    TestFinished();
  }

 private:
  ScopedTempDir temp_dir_;
  FilePath temp_file1_;
  FilePath temp_file2_;
  base::Time temp_file_modification_time1_;
  base::Time temp_file_modification_time2_;
  scoped_ptr<base::Thread> io_thread_;
  static BlobURLRequestJob* blob_url_request_job_;

  scoped_ptr<base::WaitableEvent> test_finished_event_;
  std::stack<std::pair<Task*, bool> > task_stack_;
  scoped_ptr<net::URLRequest> request_;
  scoped_ptr<MockURLRequestDelegate> url_request_delegate_;
  int expected_status_code_;
  std::string expected_response_;
};

// static
BlobURLRequestJob* BlobURLRequestJobTest::blob_url_request_job_ = NULL;

TEST_F(BlobURLRequestJobTest, TestGetSimpleDataRequest) {
  RunTestOnIOThread(&BlobURLRequestJobTest::TestGetSimpleDataRequest);
}

TEST_F(BlobURLRequestJobTest, TestGetSimpleFileRequest) {
  RunTestOnIOThread(&BlobURLRequestJobTest::TestGetSimpleFileRequest);
}

TEST_F(BlobURLRequestJobTest, TestGetLargeFileRequest) {
  RunTestOnIOThread(&BlobURLRequestJobTest::TestGetLargeFileRequest);
}

TEST_F(BlobURLRequestJobTest, TestGetSlicedDataRequest) {
  RunTestOnIOThread(&BlobURLRequestJobTest::TestGetSlicedDataRequest);
}

TEST_F(BlobURLRequestJobTest, TestGetSlicedFileRequest) {
  RunTestOnIOThread(&BlobURLRequestJobTest::TestGetSlicedFileRequest);
}

TEST_F(BlobURLRequestJobTest, TestGetNonExistentFileRequest) {
  RunTestOnIOThread(&BlobURLRequestJobTest::TestGetNonExistentFileRequest);
}

TEST_F(BlobURLRequestJobTest, TestGetChangedFileRequest) {
  RunTestOnIOThread(&BlobURLRequestJobTest::TestGetChangedFileRequest);
}

TEST_F(BlobURLRequestJobTest, TestGetComplicatedDataAndFileRequest) {
  RunTestOnIOThread(
      &BlobURLRequestJobTest::TestGetComplicatedDataAndFileRequest);
}

TEST_F(BlobURLRequestJobTest, TestGetRangeRequest1) {
  RunTestOnIOThread(&BlobURLRequestJobTest::TestGetRangeRequest1);
}

TEST_F(BlobURLRequestJobTest, TestGetRangeRequest2) {
  RunTestOnIOThread(&BlobURLRequestJobTest::TestGetRangeRequest2);
}

TEST_F(BlobURLRequestJobTest, TestExtraHeaders) {
  RunTestOnIOThread(&BlobURLRequestJobTest::TestExtraHeaders);
}

}  // namespace webkit_blob

// BlobURLRequestJobTest is expected to always live longer than the
// runnable methods.  This lets us call NewRunnableMethod on its instances.
DISABLE_RUNNABLE_METHOD_REFCOUNT(webkit_blob::BlobURLRequestJobTest);
