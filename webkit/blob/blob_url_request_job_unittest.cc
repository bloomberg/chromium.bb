// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/time.h"
#include "net/base/io_buffer.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/blob/blob_data.h"
#include "webkit/blob/blob_url_request_job.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_file_util.h"
#include "webkit/fileapi/file_system_operation_context.h"
#include "webkit/fileapi/file_system_task_runners.h"
#include "webkit/fileapi/file_system_url.h"
#include "webkit/fileapi/mock_file_system_options.h"

namespace webkit_blob {

namespace {

const int kBufferSize = 1024;
const char kTestData1[] = "Hello";
const char kTestData2[] = "Here it is data.";
const char kTestFileData1[] = "0123456789";
const char kTestFileData2[] = "This is sample file.";
const char kTestFileSystemFileData1[] = "abcdefghijklmnop";
const char kTestFileSystemFileData2[] = "File system file test data.";
const char kTestContentType[] = "foo/bar";
const char kTestContentDisposition[] = "attachment; filename=foo.txt";

const char kFileSystemURLOrigin[] = "http://remote";
const fileapi::FileSystemType kFileSystemType =
    fileapi::kFileSystemTypeTemporary;

}  // namespace

class BlobURLRequestJobTest : public testing::Test {
 public:

  // Test Harness -------------------------------------------------------------
  // TODO(jianli): share this test harness with AppCacheURLRequestJobTest

  class MockURLRequestDelegate : public net::URLRequest::Delegate {
   public:
    MockURLRequestDelegate()
        : received_data_(new net::IOBuffer(kBufferSize)) {}

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
      if (!request->is_pending()) {
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
      MessageLoop::current()->Quit();
    }

    scoped_refptr<net::IOBuffer> received_data_;
    std::string response_data_;
  };

  BlobURLRequestJobTest()
      : message_loop_(MessageLoop::TYPE_IO),
        expected_status_code_(0) {
  }

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

    net::URLRequest::Deprecated::RegisterProtocolFactory(
        "blob", &BlobURLRequestJobFactory);
  }

  void TearDown() {
    DCHECK(!blob_url_request_job_);
    net::URLRequest::Deprecated::RegisterProtocolFactory("blob", NULL);
  }

  static net::URLRequestJob* BlobURLRequestJobFactory(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate,
      const std::string& scheme) {
    BlobURLRequestJob* temp = blob_url_request_job_;
    blob_url_request_job_ = NULL;
    return temp;
  }

  void SetUpFileSystem() {
    // Prepare file system.
    file_system_context_ = new fileapi::FileSystemContext(
        fileapi::FileSystemTaskRunners::CreateMockTaskRunners(),
        NULL,
        NULL,
        temp_dir_.path(),
        fileapi::CreateDisallowFileAccessOptions());

    file_system_context_->OpenFileSystem(
        GURL(kFileSystemURLOrigin),
        kFileSystemType,
        true,  // create
        base::Bind(&BlobURLRequestJobTest::OnValidateFileSystem,
                   base::Unretained(this)));
    MessageLoop::current()->RunAllPending();
    ASSERT_TRUE(file_system_root_url_.is_valid());

    // Prepare files on file system.
    const char kFilename1[] = "FileSystemFile1.dat";
    temp_file_system_file1_ = GetFileSystemURL(kFilename1);
    WriteFileSystemFile(kFilename1, kTestFileSystemFileData1,
                        arraysize(kTestFileSystemFileData1),
                        &temp_file_system_file_modification_time1_);
    const char kFilename2[] = "FileSystemFile2.dat";
    temp_file_system_file2_ = GetFileSystemURL(kFilename2);
    WriteFileSystemFile(kFilename2, kTestFileSystemFileData2,
                        arraysize(kTestFileSystemFileData2),
                        &temp_file_system_file_modification_time2_);
  }

  GURL GetFileSystemURL(const std::string& filename) {
    return GURL(file_system_root_url_.spec() + filename);
  }

  void WriteFileSystemFile(const std::string& filename,
                           const char* buf, int buf_size,
                           base::Time* modification_time) {
    fileapi::FileSystemURL url(GURL(kFileSystemURLOrigin),
                               kFileSystemType,
                               FilePath().AppendASCII(filename));

    fileapi::FileSystemFileUtil* file_util =
        file_system_context_->GetFileUtil(kFileSystemType);

    fileapi::FileSystemOperationContext context(file_system_context_);
    context.set_allowed_bytes_growth(1024);

    base::PlatformFile handle = base::kInvalidPlatformFileValue;
    bool created = false;
    ASSERT_EQ(base::PLATFORM_FILE_OK, file_util->CreateOrOpen(
        &context,
        url,
        base::PLATFORM_FILE_CREATE | base::PLATFORM_FILE_WRITE,
        &handle,
        &created));
    EXPECT_TRUE(created);
    ASSERT_NE(base::kInvalidPlatformFileValue, handle);
    ASSERT_EQ(buf_size,
        base::WritePlatformFile(handle, 0 /* offset */, buf, buf_size));
    base::ClosePlatformFile(handle);

    base::PlatformFileInfo file_info;
    FilePath platform_path;
    ASSERT_EQ(base::PLATFORM_FILE_OK,
              file_util->GetFileInfo(&context, url, &file_info,
                                     &platform_path));
    if (modification_time)
      *modification_time = file_info.last_modified;
  }

  void OnValidateFileSystem(base::PlatformFileError result,
                            const std::string& name,
                            const GURL& root) {
    ASSERT_EQ(base::PLATFORM_FILE_OK, result);
    ASSERT_TRUE(root.is_valid());
    file_system_root_url_ = root;
  }

  void TestSuccessRequest(BlobData* blob_data,
                          const std::string& expected_response) {
    expected_status_code_ = 200;
    expected_response_ = expected_response;
    TestRequest("GET", net::HttpRequestHeaders(), blob_data);
  }

  void TestErrorRequest(BlobData* blob_data,
                        int expected_status_code) {
    expected_status_code_ = expected_status_code;
    expected_response_ = "";
    TestRequest("GET", net::HttpRequestHeaders(), blob_data);
  }

  void TestRequest(const std::string& method,
                   const net::HttpRequestHeaders& extra_headers,
                   BlobData* blob_data) {
    MockURLRequestDelegate url_request_delegate;

    request_.reset(empty_context_.CreateRequest(
        GURL("blob:blah"), &url_request_delegate));
    request_->set_method(method);
    blob_url_request_job_ = new BlobURLRequestJob(
        request_.get(),
        empty_context_.network_delegate(),
        blob_data,
        file_system_context_,
        base::MessageLoopProxy::current());

    // Start the request.
    if (!extra_headers.IsEmpty())
      request_->SetExtraRequestHeaders(extra_headers);
    request_->Start();

    MessageLoop::current()->Run();

    // Verify response.
    EXPECT_TRUE(request_->status().is_success());
    EXPECT_EQ(expected_status_code_,
              request_->response_headers()->response_code());
    EXPECT_STREQ(expected_response_.c_str(),
                 url_request_delegate.response_data().c_str());
  }

  scoped_refptr<BlobData> BuildComplicatedData(std::string* expected_result) {
    scoped_refptr<BlobData> blob_data(new BlobData());
    blob_data->AppendData(kTestData1 + 1, 2);
    blob_data->AppendFile(temp_file1_, 2, 3, temp_file_modification_time1_);
    blob_data->AppendFileSystemFile(temp_file_system_file1_, 3, 4,
                                    temp_file_system_file_modification_time1_);
    blob_data->AppendData(kTestData2 + 4, 5);
    blob_data->AppendFile(temp_file2_, 5, 6, temp_file_modification_time2_);
    blob_data->AppendFileSystemFile(temp_file_system_file2_, 6, 7,
                                    temp_file_system_file_modification_time2_);
    *expected_result = std::string(kTestData1 + 1, 2);
    *expected_result += std::string(kTestFileData1 + 2, 3);
    *expected_result += std::string(kTestFileSystemFileData1 + 3, 4);
    *expected_result += std::string(kTestData2 + 4, 5);
    *expected_result += std::string(kTestFileData2 + 5, 6);
    *expected_result += std::string(kTestFileSystemFileData2 + 6, 7);
    return blob_data;
  }

 protected:
  base::ScopedTempDir temp_dir_;
  FilePath temp_file1_;
  FilePath temp_file2_;
  base::Time temp_file_modification_time1_;
  base::Time temp_file_modification_time2_;
  GURL file_system_root_url_;
  GURL temp_file_system_file1_;
  GURL temp_file_system_file2_;
  base::Time temp_file_system_file_modification_time1_;
  base::Time temp_file_system_file_modification_time2_;
  MessageLoop message_loop_;
  static BlobURLRequestJob* blob_url_request_job_;

  net::URLRequestContext empty_context_;
  scoped_ptr<net::URLRequest> request_;
  scoped_refptr<fileapi::FileSystemContext> file_system_context_;
  int expected_status_code_;
  std::string expected_response_;
};

// static
BlobURLRequestJob* BlobURLRequestJobTest::blob_url_request_job_ = NULL;

TEST_F(BlobURLRequestJobTest, TestGetSimpleDataRequest) {
  scoped_refptr<BlobData> blob_data(new BlobData());
  blob_data->AppendData(kTestData1);
  TestSuccessRequest(blob_data, kTestData1);
}

TEST_F(BlobURLRequestJobTest, TestGetSimpleFileRequest) {
  scoped_refptr<BlobData> blob_data(new BlobData());
  blob_data->AppendFile(temp_file1_, 0, -1, base::Time());
  TestSuccessRequest(blob_data, kTestFileData1);
}

TEST_F(BlobURLRequestJobTest, TestGetLargeFileRequest) {
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

TEST_F(BlobURLRequestJobTest, TestGetNonExistentFileRequest) {
  FilePath non_existent_file =
      temp_file1_.InsertBeforeExtension(FILE_PATH_LITERAL("-na"));
  scoped_refptr<BlobData> blob_data(new BlobData());
  blob_data->AppendFile(non_existent_file, 0, -1, base::Time());
  TestErrorRequest(blob_data, 404);
}

TEST_F(BlobURLRequestJobTest, TestGetChangedFileRequest) {
  scoped_refptr<BlobData> blob_data(new BlobData());
  base::Time old_time =
      temp_file_modification_time1_ - base::TimeDelta::FromSeconds(10);
  blob_data->AppendFile(temp_file1_, 0, 3, old_time);
  TestErrorRequest(blob_data, 404);
}

TEST_F(BlobURLRequestJobTest, TestGetSlicedFileRequest) {
  scoped_refptr<BlobData> blob_data(new BlobData());
  blob_data->AppendFile(temp_file1_, 2, 4, temp_file_modification_time1_);
  std::string result(kTestFileData1 + 2, 4);
  TestSuccessRequest(blob_data, result);
}

TEST_F(BlobURLRequestJobTest, TestGetSimpleFileSystemFileRequest) {
  SetUpFileSystem();
  scoped_refptr<BlobData> blob_data(new BlobData());
  blob_data->AppendFileSystemFile(temp_file_system_file1_, 0, -1,
                                  base::Time());
  TestSuccessRequest(blob_data, kTestFileSystemFileData1);
}

TEST_F(BlobURLRequestJobTest, TestGetLargeFileSystemFileRequest) {
  SetUpFileSystem();
  std::string large_data;
  large_data.reserve(kBufferSize * 5);
  for (int i = 0; i < kBufferSize * 5; ++i)
    large_data.append(1, static_cast<char>(i % 256));

  const char kFilename[] = "LargeBlob.dat";
  WriteFileSystemFile(kFilename, large_data.data(), large_data.size(), NULL);

  scoped_refptr<BlobData> blob_data(new BlobData());
  blob_data->AppendFileSystemFile(GetFileSystemURL(kFilename),
                                  0, -1, base::Time());
  TestSuccessRequest(blob_data, large_data);
}

TEST_F(BlobURLRequestJobTest, TestGetNonExistentFileSystemFileRequest) {
  SetUpFileSystem();
  GURL non_existent_file = GetFileSystemURL("non-existent.dat");
  scoped_refptr<BlobData> blob_data(new BlobData());
  blob_data->AppendFileSystemFile(non_existent_file, 0, -1, base::Time());
  TestErrorRequest(blob_data, 404);
}

TEST_F(BlobURLRequestJobTest, TestGetChangedFileSystemFileRequest) {
  SetUpFileSystem();
  scoped_refptr<BlobData> blob_data(new BlobData());
  base::Time old_time =
      temp_file_system_file_modification_time1_ -
      base::TimeDelta::FromSeconds(10);
  blob_data->AppendFileSystemFile(temp_file_system_file1_, 0, 3, old_time);
  TestErrorRequest(blob_data, 404);
}

TEST_F(BlobURLRequestJobTest, TestGetSlicedFileSystemFileRequest) {
  SetUpFileSystem();
  scoped_refptr<BlobData> blob_data(new BlobData());
  blob_data->AppendFileSystemFile(temp_file_system_file1_, 2, 4,
                                  temp_file_system_file_modification_time1_);
  std::string result(kTestFileSystemFileData1 + 2, 4);
  TestSuccessRequest(blob_data, result);
}

TEST_F(BlobURLRequestJobTest, TestGetComplicatedDataAndFileRequest) {
  SetUpFileSystem();
  std::string result;
  scoped_refptr<BlobData> blob_data = BuildComplicatedData(&result);
  TestSuccessRequest(blob_data, result);
}

TEST_F(BlobURLRequestJobTest, TestGetRangeRequest1) {
  SetUpFileSystem();
  std::string result;
  scoped_refptr<BlobData> blob_data = BuildComplicatedData(&result);
  net::HttpRequestHeaders extra_headers;
  extra_headers.SetHeader(net::HttpRequestHeaders::kRange, "bytes=5-10");
  expected_status_code_ = 206;
  expected_response_ = result.substr(5, 10 - 5 + 1);
  TestRequest("GET", extra_headers, blob_data);
}

TEST_F(BlobURLRequestJobTest, TestGetRangeRequest2) {
  SetUpFileSystem();
  std::string result;
  scoped_refptr<BlobData> blob_data = BuildComplicatedData(&result);
  net::HttpRequestHeaders extra_headers;
  extra_headers.SetHeader(net::HttpRequestHeaders::kRange, "bytes=-10");
  expected_status_code_ = 206;
  expected_response_ = result.substr(result.length() - 10);
  TestRequest("GET", extra_headers, blob_data);
}

TEST_F(BlobURLRequestJobTest, TestExtraHeaders) {
  scoped_refptr<BlobData> blob_data(new BlobData());
  blob_data->set_content_type(kTestContentType);
  blob_data->set_content_disposition(kTestContentDisposition);
  blob_data->AppendData(kTestData1);
  expected_status_code_ = 200;
  expected_response_ = kTestData1;
  TestRequest("GET", net::HttpRequestHeaders(), blob_data);

  std::string content_type;
  EXPECT_TRUE(request_->response_headers()->GetMimeType(&content_type));
  EXPECT_STREQ(content_type.c_str(), kTestContentType);
  void* iter = NULL;
  std::string content_disposition;
  EXPECT_TRUE(request_->response_headers()->EnumerateHeader(
      &iter, "Content-Disposition", &content_disposition));
  EXPECT_STREQ(content_disposition.c_str(), kTestContentDisposition);
}

}  // namespace webkit_blob
