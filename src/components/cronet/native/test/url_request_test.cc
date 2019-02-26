// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <atomic>
#include <memory>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "components/cronet/native/test/test_upload_data_provider.h"
#include "components/cronet/native/test/test_url_request_callback.h"
#include "components/cronet/native/test/test_util.h"
#include "components/cronet/test/test_server.h"
#include "cronet_c.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using cronet::test::TestUploadDataProvider;
using cronet::test::TestUrlRequestCallback;

namespace {

// A Cronet_UrlRequestStatusListener impl that waits for OnStatus callback.
class StatusListener {
 public:
  // |callback| is verified to not yet have reached a final state when
  // OnStatus() is called back.
  explicit StatusListener(TestUrlRequestCallback* callback)
      : status_listener_(Cronet_UrlRequestStatusListener_CreateWith(
            StatusListener::OnStatus)),
        callback_(callback),
        expect_request_not_done_(false) {
    Cronet_UrlRequestStatusListener_SetClientContext(status_listener_, this);
  }

  ~StatusListener() {
    Cronet_UrlRequestStatusListener_Destroy(status_listener_);
  }

  // Wait for and return request status.
  Cronet_UrlRequestStatusListener_Status GetStatus(
      Cronet_UrlRequestPtr request) {
    Cronet_UrlRequest_GetStatus(request, status_listener_);
    // NOTE(pauljensen): There's no guarantee this line will get executed
    // before OnStatus() reads |expect_request_not_done_|.  It's very unlikely
    // it will get read before this write, but if it does it just means
    // OnStatus() won't check that the final callback has not been issued yet.
    expect_request_not_done_ = !Cronet_UrlRequest_IsDone(request);
    awaiting_status_.Wait();
    return status_;
  }

 private:
  // Cronet_UrlRequestStatusListener OnStatus impl.
  static void OnStatus(Cronet_UrlRequestStatusListenerPtr self,
                       Cronet_UrlRequestStatusListener_Status status) {
    StatusListener* listener = static_cast<StatusListener*>(
        Cronet_UrlRequestStatusListener_GetClientContext(self));

    // Enforce we call OnStatus() before OnSucceeded/OnFailed/OnCanceled().
    if (listener->expect_request_not_done_)
      EXPECT_FALSE(listener->callback_->IsDone());

    listener->status_ = status;
    listener->awaiting_status_.Signal();
  }

  Cronet_UrlRequestStatusListenerPtr const status_listener_;
  TestUrlRequestCallback* const callback_;

  Cronet_UrlRequestStatusListener_Status status_ =
      Cronet_UrlRequestStatusListener_Status_INVALID;
  base::WaitableEvent awaiting_status_;

  // Indicates if GetStatus() was called before request finished, indicating
  // that OnStatus() should be called before request finishes. The writing of
  // this variable races the reading of it, but it's initialized to a safe
  // value.
  std::atomic_bool expect_request_not_done_;

  DISALLOW_COPY_AND_ASSIGN(StatusListener);
};

// Query and return status of |request|. |callback| is verified to not yet have
// reached a final state by the time OnStatus is called.
Cronet_UrlRequestStatusListener_Status GetRequestStatus(
    Cronet_UrlRequestPtr request,
    TestUrlRequestCallback* callback) {
  return StatusListener(callback).GetStatus(request);
}

// Parameterized off whether to use a direct executor.
class UrlRequestTest : public ::testing::TestWithParam<bool> {
 protected:
  UrlRequestTest() {}
  ~UrlRequestTest() override {}

  void SetUp() override { cronet::TestServer::Start(); }

  void TearDown() override { cronet::TestServer::Shutdown(); }

  std::unique_ptr<TestUrlRequestCallback> StartAndWaitForComplete(
      const std::string& url,
      std::unique_ptr<TestUrlRequestCallback> test_callback,
      const std::string& http_method,
      TestUploadDataProvider* test_upload_data_provider) {
    Cronet_EnginePtr engine = cronet::test::CreateTestEngine(0);
    Cronet_UrlRequestPtr request = Cronet_UrlRequest_Create();
    Cronet_UrlRequestParamsPtr request_params =
        Cronet_UrlRequestParams_Create();
    Cronet_UrlRequestParams_http_method_set(request_params,
                                            http_method.c_str());
    Cronet_UploadDataProviderPtr upload_data_provider = nullptr;

    // Add upload data provider and set content type required for upload.
    if (test_upload_data_provider != nullptr) {
      upload_data_provider =
          test_upload_data_provider->CreateUploadDataProvider();
      Cronet_UrlRequestParams_upload_data_provider_set(request_params,
                                                       upload_data_provider);
      Cronet_HttpHeaderPtr header = Cronet_HttpHeader_Create();
      Cronet_HttpHeader_name_set(header, "Content-Type");
      Cronet_HttpHeader_value_set(header, "Useless/string");
      Cronet_UrlRequestParams_request_headers_add(request_params, header);
      Cronet_HttpHeader_Destroy(header);
    }

    // Executor provided by the application is owned by |test_callback|.
    Cronet_ExecutorPtr executor = test_callback->GetExecutor();
    // Callback provided by the application.
    Cronet_UrlRequestCallbackPtr callback =
        test_callback->CreateUrlRequestCallback();

    Cronet_UrlRequest_InitWithParams(request, engine, url.c_str(),
                                     request_params, callback, executor);

    Cronet_UrlRequest_Start(request);
    test_callback->WaitForDone();
    // Wait for all posted tasks to be executed to ensure there is no unhandled
    // exception.
    test_callback->ShutdownExecutor();
    EXPECT_TRUE(test_callback->IsDone());
    EXPECT_TRUE(Cronet_UrlRequest_IsDone(request));
    if (upload_data_provider != nullptr)
      Cronet_UploadDataProvider_Destroy(upload_data_provider);
    Cronet_UrlRequestParams_Destroy(request_params);
    Cronet_UrlRequest_Destroy(request);
    Cronet_UrlRequestCallback_Destroy(callback);
    Cronet_Engine_Destroy(engine);
    return test_callback;
  }

  std::unique_ptr<TestUrlRequestCallback> StartAndWaitForComplete(
      const std::string& url,
      std::unique_ptr<TestUrlRequestCallback> test_callback) {
    return StartAndWaitForComplete(url, std::move(test_callback),
                                   /* http_method =  */ std::string(),
                                   /* upload_data_provider =  */ nullptr);
  }

  std::unique_ptr<TestUrlRequestCallback> StartAndWaitForComplete(
      const std::string& url) {
    return StartAndWaitForComplete(
        url, std::make_unique<TestUrlRequestCallback>(GetParam()));
  }

  void CheckResponseInfo(
      const TestUrlRequestCallback::UrlResponseInfo& response_info,
      const std::string& expected_url,
      int expected_http_status_code,
      const std::string& expected_http_status_text) {
    EXPECT_EQ(expected_url, response_info.url);
    EXPECT_EQ(expected_url, response_info.url_chain.back());
    EXPECT_EQ(expected_http_status_code, response_info.http_status_code);
    EXPECT_EQ(expected_http_status_text, response_info.http_status_text);
    EXPECT_FALSE(response_info.was_cached);
  }

  void CheckResponseInfoHeader(
      const TestUrlRequestCallback::UrlResponseInfo& response_info,
      const std::string& header_name,
      const std::string& header_value) {
    for (const auto& header : response_info.all_headers) {
      if (header.first == header_name) {
        EXPECT_EQ(header.second, header_value);
        return;
      }
    }
    NOTREACHED();
  }

  void ExpectResponseInfoEquals(
      const TestUrlRequestCallback::UrlResponseInfo& expected,
      const TestUrlRequestCallback::UrlResponseInfo& actual) {
    EXPECT_EQ(expected.url, actual.url);
    EXPECT_EQ(expected.url_chain, actual.url_chain);
    EXPECT_EQ(expected.http_status_code, actual.http_status_code);
    EXPECT_EQ(expected.http_status_text, actual.http_status_text);
    EXPECT_EQ(expected.all_headers, actual.all_headers);
    EXPECT_EQ(expected.was_cached, actual.was_cached);
    EXPECT_EQ(expected.negotiated_protocol, actual.negotiated_protocol);
    EXPECT_EQ(expected.proxy_server, actual.proxy_server);
    EXPECT_EQ(expected.received_byte_count, actual.received_byte_count);
  }

  void TestCancel(TestUrlRequestCallback::FailureType failure_type,
                  TestUrlRequestCallback::ResponseStep failure_step,
                  bool expect_response_info,
                  bool expect_error);

 protected:
  // Provide a message loop for use by TestExecutor instances.
  base::MessageLoop message_loop_;

 private:
  DISALLOW_COPY_AND_ASSIGN(UrlRequestTest);
};

const bool kDirectExecutorEnabled[]{true, false};
INSTANTIATE_TEST_CASE_P(,
                        UrlRequestTest,
                        testing::ValuesIn(kDirectExecutorEnabled));

TEST_P(UrlRequestTest, InitChecks) {
  Cronet_EngineParamsPtr engine_params = Cronet_EngineParams_Create();
  Cronet_EnginePtr engine = Cronet_Engine_Create();
  // Disable runtime CHECK of the result, so it could be verified.
  Cronet_EngineParams_enable_check_result_set(engine_params, false);
  EXPECT_EQ(Cronet_RESULT_SUCCESS,
            Cronet_Engine_StartWithParams(engine, engine_params));
  Cronet_EngineParams_Destroy(engine_params);

  Cronet_UrlRequestPtr request = Cronet_UrlRequest_Create();
  Cronet_UrlRequestParamsPtr request_params = Cronet_UrlRequestParams_Create();
  const std::string url = cronet::TestServer::GetEchoMethodURL();

  TestUrlRequestCallback test_callback(GetParam());
  // Executor provided by the application is owned by |test_callback|.
  Cronet_ExecutorPtr executor = test_callback.GetExecutor();
  // Callback provided by the application.
  Cronet_UrlRequestCallbackPtr callback =
      test_callback.CreateUrlRequestCallback();
  EXPECT_EQ(Cronet_RESULT_NULL_POINTER_URL,
            Cronet_UrlRequest_InitWithParams(
                request, engine, /* url = */ nullptr,
                /* request_params = */ nullptr, /* callback = */ nullptr,
                /* executor = */ nullptr));
  Cronet_UrlRequest_Destroy(request);

  request = Cronet_UrlRequest_Create();
  EXPECT_EQ(Cronet_RESULT_NULL_POINTER_PARAMS,
            Cronet_UrlRequest_InitWithParams(request, engine, url.c_str(),
                                             /* request_params = */ nullptr,
                                             /* callback = */ nullptr,
                                             /* executor = */ nullptr));
  Cronet_UrlRequest_Destroy(request);

  request = Cronet_UrlRequest_Create();
  EXPECT_EQ(Cronet_RESULT_NULL_POINTER_CALLBACK,
            Cronet_UrlRequest_InitWithParams(
                request, engine, url.c_str(), request_params,
                /* callback = */ nullptr, /* executor = */ nullptr));
  Cronet_UrlRequest_Destroy(request);

  request = Cronet_UrlRequest_Create();
  EXPECT_EQ(Cronet_RESULT_NULL_POINTER_EXECUTOR,
            Cronet_UrlRequest_InitWithParams(request, engine, url.c_str(),
                                             request_params, callback,
                                             /* executor = */ nullptr));
  Cronet_UrlRequest_Destroy(request);

  request = Cronet_UrlRequest_Create();
  EXPECT_EQ(Cronet_RESULT_NULL_POINTER_EXECUTOR,
            Cronet_UrlRequest_InitWithParams(request, engine, url.c_str(),
                                             request_params, callback,
                                             /* executor = */ nullptr));
  Cronet_UrlRequest_Destroy(request);

  request = Cronet_UrlRequest_Create();
  Cronet_UrlRequestParams_http_method_set(request_params, "bad:method");
  EXPECT_EQ(
      Cronet_RESULT_ILLEGAL_ARGUMENT_INVALID_HTTP_METHOD,
      Cronet_UrlRequest_InitWithParams(request, engine, url.c_str(),
                                       request_params, callback, executor));
  Cronet_UrlRequest_Destroy(request);

  request = Cronet_UrlRequest_Create();
  Cronet_UrlRequestParams_http_method_set(request_params, "HEAD");
  // Check header validation
  Cronet_HttpHeaderPtr http_header = Cronet_HttpHeader_Create();
  Cronet_UrlRequestParams_request_headers_add(request_params, http_header);
  EXPECT_EQ(
      Cronet_RESULT_NULL_POINTER_HEADER_NAME,
      Cronet_UrlRequest_InitWithParams(request, engine, url.c_str(),
                                       request_params, callback, executor));
  Cronet_UrlRequestParams_request_headers_clear(request_params);
  Cronet_UrlRequest_Destroy(request);

  request = Cronet_UrlRequest_Create();
  Cronet_HttpHeader_name_set(http_header, "bad:name");
  Cronet_UrlRequestParams_request_headers_add(request_params, http_header);
  EXPECT_EQ(
      Cronet_RESULT_NULL_POINTER_HEADER_VALUE,
      Cronet_UrlRequest_InitWithParams(request, engine, url.c_str(),
                                       request_params, callback, executor));
  Cronet_UrlRequestParams_request_headers_clear(request_params);
  Cronet_UrlRequest_Destroy(request);

  request = Cronet_UrlRequest_Create();
  Cronet_HttpHeader_value_set(http_header, "header value");
  Cronet_UrlRequestParams_request_headers_add(request_params, http_header);
  EXPECT_EQ(
      Cronet_RESULT_ILLEGAL_ARGUMENT_INVALID_HTTP_HEADER,
      Cronet_UrlRequest_InitWithParams(request, engine, url.c_str(),
                                       request_params, callback, executor));
  Cronet_UrlRequestParams_request_headers_clear(request_params);
  Cronet_UrlRequest_Destroy(request);

  request = Cronet_UrlRequest_Create();
  Cronet_HttpHeader_name_set(http_header, "header-name");
  Cronet_UrlRequestParams_request_headers_add(request_params, http_header);
  EXPECT_EQ(Cronet_RESULT_SUCCESS, Cronet_UrlRequest_InitWithParams(
                                       request, engine, url.c_str(),
                                       request_params, callback, executor));
  EXPECT_EQ(
      Cronet_RESULT_ILLEGAL_STATE_REQUEST_ALREADY_INITIALIZED,
      Cronet_UrlRequest_InitWithParams(request, engine, url.c_str(),
                                       request_params, callback, executor));
  Cronet_HttpHeader_Destroy(http_header);
  Cronet_UrlRequest_Destroy(request);
  Cronet_UrlRequestParams_Destroy(request_params);
  Cronet_UrlRequestCallback_Destroy(callback);
  Cronet_Engine_Destroy(engine);
}

TEST_P(UrlRequestTest, SimpleGet) {
  const std::string url = cronet::TestServer::GetEchoMethodURL();
  auto callback = StartAndWaitForComplete(url);
  EXPECT_EQ(200, callback->response_info_->http_status_code);
  // Default method is 'GET'.
  EXPECT_EQ("GET", callback->response_as_string_);
  EXPECT_EQ(0, callback->redirect_count_);
  EXPECT_EQ(callback->response_step_, callback->ON_SUCCEEDED);
  CheckResponseInfo(*callback->response_info_, url, 200, "OK");
  TestUrlRequestCallback::UrlResponseInfo expected_response_info(
      std::vector<std::string>({url}), "OK", 200, 86,
      std::vector<std::string>({"Connection", "close", "Content-Length", "3",
                                "Content-Type", "text/plain"}));
  ExpectResponseInfoEquals(expected_response_info, *callback->response_info_);
}

TEST_P(UrlRequestTest, UploadEmptyBodySync) {
  const std::string url = cronet::TestServer::GetEchoRequestBodyURL();
  TestUploadDataProvider data_provider(TestUploadDataProvider::SYNC,
                                       /* executor = */ nullptr);
  auto callback = std::make_unique<TestUrlRequestCallback>(GetParam());
  callback = StartAndWaitForComplete(url, std::move(callback), std::string(),
                                     &data_provider);
  data_provider.AssertClosed();
  EXPECT_EQ(0, data_provider.GetUploadedLength());
  EXPECT_EQ(0, data_provider.num_read_calls());
  EXPECT_EQ(0, data_provider.num_rewind_calls());
  EXPECT_EQ(200, callback->response_info_->http_status_code);
  EXPECT_EQ("", callback->response_as_string_);
}

TEST_P(UrlRequestTest, UploadSync) {
  const std::string url = cronet::TestServer::GetEchoRequestBodyURL();
  TestUploadDataProvider data_provider(TestUploadDataProvider::SYNC,
                                       /* executor = */ nullptr);
  data_provider.AddRead("Test");
  auto callback = std::make_unique<TestUrlRequestCallback>(GetParam());
  callback = StartAndWaitForComplete(url, std::move(callback), std::string(),
                                     &data_provider);
  data_provider.AssertClosed();
  EXPECT_EQ(4, data_provider.GetUploadedLength());
  EXPECT_EQ(1, data_provider.num_read_calls());
  EXPECT_EQ(0, data_provider.num_rewind_calls());
  EXPECT_EQ(200, callback->response_info_->http_status_code);
  EXPECT_EQ("Test", callback->response_as_string_);
}

TEST_P(UrlRequestTest, SSLCertificateError) {
  net::EmbeddedTestServer ssl_server(net::EmbeddedTestServer::TYPE_HTTPS);
  ssl_server.SetSSLConfig(net::EmbeddedTestServer::CERT_EXPIRED);
  ASSERT_TRUE(ssl_server.Start());

  const std::string url = ssl_server.GetURL("/").spec();
  TestUploadDataProvider data_provider(TestUploadDataProvider::SYNC,
                                       /* executor = */ nullptr);
  data_provider.AddRead("Test");
  auto callback = std::make_unique<TestUrlRequestCallback>(GetParam());
  callback = StartAndWaitForComplete(url, std::move(callback), std::string(),
                                     &data_provider);
  data_provider.AssertClosed();
  EXPECT_EQ(4, data_provider.GetUploadedLength());
  EXPECT_EQ(0, data_provider.num_read_calls());
  EXPECT_EQ(0, data_provider.num_rewind_calls());
  EXPECT_EQ(nullptr, callback->response_info_);
  EXPECT_EQ("", callback->response_as_string_);
  EXPECT_EQ("net::ERR_CERT_INVALID", callback->last_error_message_);
}

TEST_P(UrlRequestTest, UploadMultiplePiecesSync) {
  const std::string url = cronet::TestServer::GetEchoRequestBodyURL();
  auto callback = std::make_unique<TestUrlRequestCallback>(GetParam());
  TestUploadDataProvider data_provider(TestUploadDataProvider::SYNC,
                                       callback->GetExecutor());
  data_provider.AddRead("Y");
  data_provider.AddRead("et ");
  data_provider.AddRead("another ");
  data_provider.AddRead("test");
  callback = StartAndWaitForComplete(url, std::move(callback), std::string(),
                                     &data_provider);
  data_provider.AssertClosed();
  EXPECT_EQ(16, data_provider.GetUploadedLength());
  EXPECT_EQ(4, data_provider.num_read_calls());
  EXPECT_EQ(0, data_provider.num_rewind_calls());
  EXPECT_EQ(200, callback->response_info_->http_status_code);
  EXPECT_EQ("Yet another test", callback->response_as_string_);
}

TEST_P(UrlRequestTest, UploadMultiplePiecesAsync) {
  const std::string url = cronet::TestServer::GetEchoRequestBodyURL();
  auto callback = std::make_unique<TestUrlRequestCallback>(GetParam());
  TestUploadDataProvider data_provider(TestUploadDataProvider::ASYNC,
                                       callback->GetExecutor());
  data_provider.AddRead("Y");
  data_provider.AddRead("et ");
  data_provider.AddRead("another ");
  data_provider.AddRead("test");
  callback = StartAndWaitForComplete(url, std::move(callback), std::string(),
                                     &data_provider);
  data_provider.AssertClosed();
  EXPECT_EQ(16, data_provider.GetUploadedLength());
  EXPECT_EQ(4, data_provider.num_read_calls());
  EXPECT_EQ(0, data_provider.num_rewind_calls());
  EXPECT_EQ(200, callback->response_info_->http_status_code);
  EXPECT_EQ("Yet another test", callback->response_as_string_);
}

TEST_P(UrlRequestTest, UploadChangesDefaultMethod) {
  const std::string url = cronet::TestServer::GetEchoMethodURL();
  TestUploadDataProvider upload_data_provider(TestUploadDataProvider::SYNC,
                                              /* executor = */ nullptr);
  upload_data_provider.AddRead("Test");
  auto callback = std::make_unique<TestUrlRequestCallback>(GetParam());

  callback = StartAndWaitForComplete(url, std::move(callback), std::string(),
                                     &upload_data_provider);
  EXPECT_EQ(200, callback->response_info_->http_status_code);
  // Setting upload provider should change method to 'POST'.
  EXPECT_EQ("POST", callback->response_as_string_);
}

TEST_P(UrlRequestTest, UploadWithSetMethod) {
  const std::string url = cronet::TestServer::GetEchoMethodURL();
  TestUploadDataProvider upload_data_provider(TestUploadDataProvider::SYNC,
                                              /* executor = */ nullptr);
  upload_data_provider.AddRead("Test");
  auto callback = std::make_unique<TestUrlRequestCallback>(GetParam());

  callback = StartAndWaitForComplete(url, std::move(callback),
                                     std::string("PUT"), &upload_data_provider);
  EXPECT_EQ(200, callback->response_info_->http_status_code);
  // Setting upload provider should change method to 'POST'.
  EXPECT_EQ("PUT", callback->response_as_string_);
}

TEST_F(UrlRequestTest, UploadWithDirectExecutor) {
  const std::string url = cronet::TestServer::GetEchoRequestBodyURL();
  auto callback = std::make_unique<TestUrlRequestCallback>(true);
  TestUploadDataProvider data_provider(TestUploadDataProvider::SYNC,
                                       callback->GetExecutor());
  data_provider.AddRead("Test");
  callback = StartAndWaitForComplete(url, std::move(callback), std::string(),
                                     &data_provider);
  data_provider.AssertClosed();
  EXPECT_EQ(4, data_provider.GetUploadedLength());
  EXPECT_EQ(1, data_provider.num_read_calls());
  EXPECT_EQ(0, data_provider.num_rewind_calls());
  EXPECT_EQ(200, callback->response_info_->http_status_code);
  EXPECT_EQ("Test", callback->response_as_string_);
}

TEST_P(UrlRequestTest, UploadRedirectSync) {
  const std::string url = cronet::TestServer::GetRedirectToEchoBodyURL();
  TestUploadDataProvider data_provider(TestUploadDataProvider::SYNC,
                                       /* executor = */ nullptr);
  data_provider.AddRead("Test");
  auto callback = std::make_unique<TestUrlRequestCallback>(GetParam());
  callback = StartAndWaitForComplete(url, std::move(callback), std::string(),
                                     &data_provider);
  data_provider.AssertClosed();
  EXPECT_EQ(4, data_provider.GetUploadedLength());
  EXPECT_EQ(2, data_provider.num_read_calls());
  EXPECT_EQ(1, data_provider.num_rewind_calls());
  EXPECT_EQ(200, callback->response_info_->http_status_code);
  EXPECT_EQ("Test", callback->response_as_string_);
}

TEST_P(UrlRequestTest, UploadRedirectAsync) {
  auto callback = std::make_unique<TestUrlRequestCallback>(GetParam());
  const std::string url = cronet::TestServer::GetRedirectToEchoBodyURL();
  TestUploadDataProvider data_provider(TestUploadDataProvider::ASYNC,
                                       callback->GetExecutor());
  data_provider.AddRead("Test");
  callback = StartAndWaitForComplete(url, std::move(callback), std::string(),
                                     &data_provider);
  data_provider.AssertClosed();
  EXPECT_EQ(4, data_provider.GetUploadedLength());
  EXPECT_EQ(2, data_provider.num_read_calls());
  EXPECT_EQ(1, data_provider.num_rewind_calls());
  EXPECT_EQ(200, callback->response_info_->http_status_code);
  EXPECT_EQ("Test", callback->response_as_string_);
}

TEST_P(UrlRequestTest, UploadWithBadLength) {
  auto callback = std::make_unique<TestUrlRequestCallback>(GetParam());
  const std::string url = cronet::TestServer::GetEchoRequestBodyURL();
  TestUploadDataProvider data_provider(TestUploadDataProvider::SYNC,
                                       callback->GetExecutor());
  data_provider.set_bad_length(1ll);
  data_provider.AddRead("12");
  callback = StartAndWaitForComplete(url, std::move(callback), std::string(),
                                     &data_provider);
  data_provider.AssertClosed();
  EXPECT_EQ(2, data_provider.GetUploadedLength());
  EXPECT_EQ(1, data_provider.num_read_calls());
  EXPECT_EQ(0, data_provider.num_rewind_calls());
  EXPECT_EQ(nullptr, callback->response_info_);
  EXPECT_NE(nullptr, callback->last_error_);
  EXPECT_EQ(Cronet_Error_ERROR_CODE_ERROR_CALLBACK, callback->last_error_code_);
  EXPECT_EQ(0ul, callback->last_error_message_.find(
                     "Failure from UploadDataProvider"));
  EXPECT_NE(std::string::npos,
            callback->last_error_message_.find(
                "Read upload data length 2 exceeds expected length 1"));
}

TEST_P(UrlRequestTest, UploadWithBadLengthBufferAligned) {
  auto callback = std::make_unique<TestUrlRequestCallback>(GetParam());
  const std::string url = cronet::TestServer::GetEchoRequestBodyURL();
  TestUploadDataProvider data_provider(TestUploadDataProvider::SYNC,
                                       callback->GetExecutor());
  data_provider.set_bad_length(8191ll);
  // Add 8192 bytes to read.
  for (int i = 0; i < 512; ++i)
    data_provider.AddRead("0123456789abcdef");

  callback = StartAndWaitForComplete(url, std::move(callback), std::string(),
                                     &data_provider);
  data_provider.AssertClosed();
  EXPECT_EQ(8192, data_provider.GetUploadedLength());
  EXPECT_EQ(512, data_provider.num_read_calls());
  EXPECT_EQ(0, data_provider.num_rewind_calls());
  EXPECT_EQ(nullptr, callback->response_info_);
  EXPECT_NE(nullptr, callback->last_error_);
  EXPECT_EQ(Cronet_Error_ERROR_CODE_ERROR_CALLBACK, callback->last_error_code_);
  EXPECT_EQ(0ul, callback->last_error_message_.find(
                     "Failure from UploadDataProvider"));
  EXPECT_NE(std::string::npos,
            callback->last_error_message_.find(
                "Read upload data length 8192 exceeds expected length 8191"));
}

TEST_P(UrlRequestTest, UploadReadFailSync) {
  auto callback = std::make_unique<TestUrlRequestCallback>(GetParam());
  const std::string url = cronet::TestServer::GetEchoRequestBodyURL();
  TestUploadDataProvider data_provider(TestUploadDataProvider::SYNC,
                                       callback->GetExecutor());
  data_provider.SetReadFailure(0, TestUploadDataProvider::CALLBACK_SYNC);
  data_provider.AddRead("Test");
  callback = StartAndWaitForComplete(url, std::move(callback), std::string(),
                                     &data_provider);
  data_provider.AssertClosed();
  EXPECT_EQ(4, data_provider.GetUploadedLength());
  EXPECT_EQ(1, data_provider.num_read_calls());
  EXPECT_EQ(0, data_provider.num_rewind_calls());
  EXPECT_EQ(nullptr, callback->response_info_);
  EXPECT_NE(nullptr, callback->last_error_);
  EXPECT_EQ(Cronet_Error_ERROR_CODE_ERROR_CALLBACK, callback->last_error_code_);
  EXPECT_EQ(0ul, callback->last_error_message_.find(
                     "Failure from UploadDataProvider"));
  EXPECT_NE(std::string::npos,
            callback->last_error_message_.find("Sync read failure"));
}

TEST_P(UrlRequestTest, UploadReadFailAsync) {
  auto callback = std::make_unique<TestUrlRequestCallback>(GetParam());
  const std::string url = cronet::TestServer::GetEchoRequestBodyURL();
  TestUploadDataProvider data_provider(TestUploadDataProvider::SYNC,
                                       callback->GetExecutor());
  data_provider.SetReadFailure(0, TestUploadDataProvider::CALLBACK_ASYNC);
  data_provider.AddRead("Test");
  callback = StartAndWaitForComplete(url, std::move(callback), std::string(),
                                     &data_provider);
  data_provider.AssertClosed();
  EXPECT_EQ(4, data_provider.GetUploadedLength());
  EXPECT_EQ(1, data_provider.num_read_calls());
  EXPECT_EQ(0, data_provider.num_rewind_calls());
  EXPECT_EQ(nullptr, callback->response_info_);
  EXPECT_NE(nullptr, callback->last_error_);
  EXPECT_EQ(Cronet_Error_ERROR_CODE_ERROR_CALLBACK, callback->last_error_code_);
  EXPECT_EQ(0ul, callback->last_error_message_.find(
                     "Failure from UploadDataProvider"));
  EXPECT_NE(std::string::npos,
            callback->last_error_message_.find("Async read failure"));
}

TEST_P(UrlRequestTest, UploadRewindFailSync) {
  auto callback = std::make_unique<TestUrlRequestCallback>(GetParam());
  const std::string url = cronet::TestServer::GetRedirectToEchoBodyURL();
  TestUploadDataProvider data_provider(TestUploadDataProvider::SYNC,
                                       callback->GetExecutor());
  data_provider.SetRewindFailure(TestUploadDataProvider::CALLBACK_SYNC);
  data_provider.AddRead("Test");
  callback = StartAndWaitForComplete(url, std::move(callback), std::string(),
                                     &data_provider);
  data_provider.AssertClosed();
  EXPECT_EQ(4, data_provider.GetUploadedLength());
  EXPECT_EQ(1, data_provider.num_read_calls());
  EXPECT_EQ(1, data_provider.num_rewind_calls());
  EXPECT_NE(nullptr, callback->last_error_);
  EXPECT_EQ(Cronet_Error_ERROR_CODE_ERROR_CALLBACK, callback->last_error_code_);
  EXPECT_EQ(0ul, callback->last_error_message_.find(
                     "Failure from UploadDataProvider"));
  EXPECT_NE(std::string::npos,
            callback->last_error_message_.find("Sync rewind failure"));
}

TEST_P(UrlRequestTest, UploadRewindFailAsync) {
  auto callback = std::make_unique<TestUrlRequestCallback>(GetParam());
  const std::string url = cronet::TestServer::GetRedirectToEchoBodyURL();
  TestUploadDataProvider data_provider(TestUploadDataProvider::SYNC,
                                       callback->GetExecutor());
  data_provider.SetRewindFailure(TestUploadDataProvider::CALLBACK_ASYNC);
  data_provider.AddRead("Test");
  callback = StartAndWaitForComplete(url, std::move(callback), std::string(),
                                     &data_provider);
  data_provider.AssertClosed();
  EXPECT_EQ(4, data_provider.GetUploadedLength());
  EXPECT_EQ(1, data_provider.num_read_calls());
  EXPECT_EQ(1, data_provider.num_rewind_calls());
  EXPECT_NE(nullptr, callback->last_error_);
  EXPECT_EQ(Cronet_Error_ERROR_CODE_ERROR_CALLBACK, callback->last_error_code_);
  EXPECT_EQ(0ul, callback->last_error_message_.find(
                     "Failure from UploadDataProvider"));
  EXPECT_NE(std::string::npos,
            callback->last_error_message_.find("Async rewind failure"));
}

TEST_P(UrlRequestTest, UploadChunked) {
  auto callback = std::make_unique<TestUrlRequestCallback>(GetParam());
  const std::string url = cronet::TestServer::GetEchoRequestBodyURL();
  TestUploadDataProvider data_provider(TestUploadDataProvider::SYNC,
                                       callback->GetExecutor());
  data_provider.AddRead("Test Hello");
  data_provider.set_chunked(true);
  EXPECT_EQ(-1, data_provider.GetLength());
  callback = StartAndWaitForComplete(url, std::move(callback), std::string(),
                                     &data_provider);
  data_provider.AssertClosed();
  EXPECT_EQ(-1, data_provider.GetUploadedLength());
  EXPECT_EQ(1, data_provider.num_read_calls());
  EXPECT_EQ(200, callback->response_info_->http_status_code);
  EXPECT_EQ("Test Hello", callback->response_as_string_);
}

TEST_P(UrlRequestTest, UploadChunkedLastReadZeroLengthBody) {
  auto callback = std::make_unique<TestUrlRequestCallback>(GetParam());
  const std::string url = cronet::TestServer::GetEchoRequestBodyURL();
  TestUploadDataProvider data_provider(TestUploadDataProvider::SYNC,
                                       callback->GetExecutor());
  // Add 3 reads. The last read has a 0-length body.
  data_provider.AddRead("hello there");
  data_provider.AddRead("!");
  data_provider.AddRead("");
  data_provider.set_chunked(true);
  EXPECT_EQ(-1, data_provider.GetLength());
  callback = StartAndWaitForComplete(url, std::move(callback), std::string(),
                                     &data_provider);
  data_provider.AssertClosed();
  EXPECT_EQ(-1, data_provider.GetUploadedLength());
  // 2 read call for the first two data chunks, and 1 for final chunk.
  EXPECT_EQ(3, data_provider.num_read_calls());
  EXPECT_EQ(200, callback->response_info_->http_status_code);
  EXPECT_EQ("hello there!", callback->response_as_string_);
}

// Test where an upload fails without ever initializing the
// UploadDataStream, because it can't connect to the server.
TEST_P(UrlRequestTest, UploadFailsWithoutInitializingStream) {
  // The port for PTP will always refuse a TCP connection
  const std::string url = "http://127.0.0.1:319";
  TestUploadDataProvider data_provider(TestUploadDataProvider::SYNC,
                                       /* executor = */ nullptr);
  data_provider.AddRead("Test");
  auto callback = std::make_unique<TestUrlRequestCallback>(GetParam());
  callback = StartAndWaitForComplete(url, std::move(callback), std::string(),
                                     &data_provider);
  data_provider.AssertClosed();
  EXPECT_EQ(0, data_provider.num_read_calls());
  EXPECT_EQ(0, data_provider.num_rewind_calls());
  EXPECT_EQ(nullptr, callback->response_info_);
  EXPECT_EQ("", callback->response_as_string_);
  EXPECT_TRUE(callback->on_error_called_);
}

TEST_P(UrlRequestTest, SimpleRequest) {
  Cronet_EnginePtr engine = cronet::test::CreateTestEngine(0);
  Cronet_UrlRequestPtr request = Cronet_UrlRequest_Create();
  Cronet_UrlRequestParamsPtr request_params = Cronet_UrlRequestParams_Create();
  std::string url = cronet::TestServer::GetSimpleURL();

  TestUrlRequestCallback test_callback(GetParam());
  // Executor provided by the application is owned by |test_callback|.
  Cronet_ExecutorPtr executor = test_callback.GetExecutor();
  // Callback provided by the application.
  Cronet_UrlRequestCallbackPtr callback =
      test_callback.CreateUrlRequestCallback();

  Cronet_UrlRequest_InitWithParams(request, engine, url.c_str(), request_params,
                                   callback, executor);

  Cronet_UrlRequest_Start(request);

  test_callback.WaitForDone();
  EXPECT_TRUE(test_callback.IsDone());
  ASSERT_EQ("The quick brown fox jumps over the lazy dog.",
            test_callback.response_as_string_);

  Cronet_UrlRequestParams_Destroy(request_params);
  Cronet_UrlRequest_Destroy(request);
  Cronet_UrlRequestCallback_Destroy(callback);
  Cronet_Engine_Destroy(engine);
}

TEST_P(UrlRequestTest, MultiRedirect) {
  const std::string url = cronet::TestServer::GetMultiRedirectURL();
  auto callback = StartAndWaitForComplete(url);
  EXPECT_EQ(2, callback->redirect_count_);
  EXPECT_EQ(200, callback->response_info_->http_status_code);
  EXPECT_EQ(2ul, callback->redirect_response_info_list_.size());
  EXPECT_EQ(2ul, callback->redirect_url_list_.size());

  // Check first redirect (multiredirect.html -> redirect.html).
  TestUrlRequestCallback::UrlResponseInfo first_expected_response_info(
      std::vector<std::string>({url}), "Found", 302, 76,
      std::vector<std::string>(
          {"Location", GURL(cronet::TestServer::GetRedirectURL()).path(),
           "redirect-header0", "header-value"}));
  ExpectResponseInfoEquals(first_expected_response_info,
                           *callback->redirect_response_info_list_.front());
  EXPECT_EQ(cronet::TestServer::GetRedirectURL(),
            callback->redirect_url_list_.front());

  // Check second redirect (redirect.html -> success.txt).
  TestUrlRequestCallback::UrlResponseInfo second_expected_response_info(
      std::vector<std::string>({cronet::TestServer::GetMultiRedirectURL(),
                                cronet::TestServer::GetRedirectURL()}),
      "Found", 302, 149,
      std::vector<std::string>(
          {"Location", GURL(cronet::TestServer::GetSuccessURL()).path(),
           "redirect-header", "header-value"}));
  ExpectResponseInfoEquals(second_expected_response_info,
                           *callback->redirect_response_info_list_.back());
  EXPECT_EQ(cronet::TestServer::GetSuccessURL(),
            callback->redirect_url_list_.back());

  // Check final response (success.txt).
  TestUrlRequestCallback::UrlResponseInfo final_expected_response_info(
      std::vector<std::string>({cronet::TestServer::GetMultiRedirectURL(),
                                cronet::TestServer::GetRedirectURL(),
                                cronet::TestServer::GetSuccessURL()}),
      "OK", 200, 334,
      std::vector<std::string>(
          {"Content-Type", "text/plain", "Access-Control-Allow-Origin", "*",
           "header-name", "header-value", "multi-header-name", "header-value1",
           "multi-header-name", "header-value2"}));
  ExpectResponseInfoEquals(final_expected_response_info,
                           *callback->response_info_);
  EXPECT_NE(0, callback->response_data_length_);
  EXPECT_EQ(callback->ON_SUCCEEDED, callback->response_step_);
}

TEST_P(UrlRequestTest, CancelRequest) {
  Cronet_EnginePtr engine = cronet::test::CreateTestEngine(0);
  Cronet_UrlRequestPtr request = Cronet_UrlRequest_Create();
  Cronet_UrlRequestParamsPtr request_params = Cronet_UrlRequestParams_Create();
  std::string url = cronet::TestServer::GetSimpleURL();

  TestUrlRequestCallback test_callback(GetParam());
  test_callback.set_failure(test_callback.CANCEL_SYNC,
                            test_callback.ON_RESPONSE_STARTED);
  // Executor provided by the application is owned by |test_callback|.
  Cronet_ExecutorPtr executor = test_callback.GetExecutor();
  // Callback provided by the application.
  Cronet_UrlRequestCallbackPtr callback =
      test_callback.CreateUrlRequestCallback();

  Cronet_UrlRequest_InitWithParams(request, engine, url.c_str(), request_params,
                                   callback, executor);

  Cronet_UrlRequest_Start(request);

  test_callback.WaitForDone();
  EXPECT_TRUE(test_callback.IsDone());
  EXPECT_TRUE(test_callback.on_canceled_called_);
  ASSERT_FALSE(test_callback.on_error_called_);
  EXPECT_TRUE(test_callback.response_as_string_.empty());

  Cronet_UrlRequestParams_Destroy(request_params);
  Cronet_UrlRequest_Destroy(request);
  Cronet_UrlRequestCallback_Destroy(callback);
  Cronet_Engine_Destroy(engine);
}

TEST_P(UrlRequestTest, FailedRequestHostNotFound) {
  Cronet_EnginePtr engine = cronet::test::CreateTestEngine(0);
  Cronet_UrlRequestPtr request = Cronet_UrlRequest_Create();
  Cronet_UrlRequestParamsPtr request_params = Cronet_UrlRequestParams_Create();
  std::string url = "https://notfound.example.com";

  TestUrlRequestCallback test_callback(GetParam());
  // Executor provided by the application is owned by |test_callback|.
  Cronet_ExecutorPtr executor = test_callback.GetExecutor();
  // Callback provided by the application.
  Cronet_UrlRequestCallbackPtr callback =
      test_callback.CreateUrlRequestCallback();

  Cronet_UrlRequest_InitWithParams(request, engine, url.c_str(), request_params,
                                   callback, executor);

  Cronet_UrlRequest_Start(request);

  test_callback.WaitForDone();
  EXPECT_TRUE(test_callback.IsDone());
  EXPECT_TRUE(test_callback.on_error_called_);
  EXPECT_FALSE(test_callback.on_canceled_called_);

  EXPECT_TRUE(test_callback.response_as_string_.empty());
  EXPECT_EQ(nullptr, test_callback.response_info_);
  EXPECT_NE(nullptr, test_callback.last_error_);

  EXPECT_EQ(Cronet_Error_ERROR_CODE_ERROR_HOSTNAME_NOT_RESOLVED,
            Cronet_Error_error_code_get(test_callback.last_error_));
  EXPECT_FALSE(
      Cronet_Error_immediately_retryable_get(test_callback.last_error_));
  EXPECT_STREQ("net::ERR_NAME_NOT_RESOLVED",
               Cronet_Error_message_get(test_callback.last_error_));
  EXPECT_EQ(-105,
            Cronet_Error_internal_error_code_get(test_callback.last_error_));
  EXPECT_EQ(
      0, Cronet_Error_quic_detailed_error_code_get(test_callback.last_error_));

  Cronet_UrlRequestParams_Destroy(request_params);
  Cronet_UrlRequest_Destroy(request);
  Cronet_UrlRequestCallback_Destroy(callback);
  Cronet_Engine_Destroy(engine);
}

void UrlRequestTest::TestCancel(
    TestUrlRequestCallback::FailureType failure_type,
    TestUrlRequestCallback::ResponseStep failure_step,
    bool expect_response_info,
    bool expect_error) {
  auto callback = std::make_unique<TestUrlRequestCallback>(GetParam());
  callback->set_failure(failure_type, failure_step);
  const std::string url = cronet::TestServer::GetRedirectURL();
  callback = StartAndWaitForComplete(url, std::move(callback));
  EXPECT_EQ(1, callback->redirect_count_);
  EXPECT_EQ(1ul, callback->redirect_response_info_list_.size());

  if (failure_type == TestUrlRequestCallback::CANCEL_SYNC ||
      failure_type == TestUrlRequestCallback::CANCEL_ASYNC) {
    EXPECT_EQ(TestUrlRequestCallback::ON_CANCELED, callback->response_step_);
  }

  EXPECT_EQ(expect_response_info, callback->response_info_ != nullptr);
  EXPECT_EQ(expect_error, callback->last_error_ != nullptr);
  EXPECT_EQ(expect_error, callback->on_error_called_);

  // When |failure_type| is CANCEL_ASYNC_WITHOUT_PAUSE and |failure_step|
  // is ON_READ_COMPLETED, there might be an onSucceeded() task
  // already posted. If that's the case, onCanceled() will not be invoked. See
  // crbug.com/657415.
  if (!(failure_type == TestUrlRequestCallback::CANCEL_ASYNC_WITHOUT_PAUSE &&
        failure_step == TestUrlRequestCallback::ON_READ_COMPLETED)) {
    EXPECT_TRUE(callback->on_canceled_called_);
  }
}

TEST_P(UrlRequestTest, TestCancel) {
  TestCancel(TestUrlRequestCallback::CANCEL_SYNC,
             TestUrlRequestCallback::ON_RECEIVED_REDIRECT, false, false);
  TestCancel(TestUrlRequestCallback::CANCEL_ASYNC,
             TestUrlRequestCallback::ON_RECEIVED_REDIRECT, false, false);
  TestCancel(TestUrlRequestCallback::CANCEL_ASYNC_WITHOUT_PAUSE,
             TestUrlRequestCallback::ON_RECEIVED_REDIRECT, false, false);

  TestCancel(TestUrlRequestCallback::CANCEL_SYNC,
             TestUrlRequestCallback::ON_RESPONSE_STARTED, true, false);
  TestCancel(TestUrlRequestCallback::CANCEL_ASYNC,
             TestUrlRequestCallback::ON_RESPONSE_STARTED, true, false);
  // https://crbug.com/812334 - If request is canceled asynchronously, the
  // 'OnReadCompleted' callback may arrive AFTER 'OnCanceled'.
  TestCancel(TestUrlRequestCallback::CANCEL_ASYNC_WITHOUT_PAUSE,
             TestUrlRequestCallback::ON_RESPONSE_STARTED, true, false);

  TestCancel(TestUrlRequestCallback::CANCEL_SYNC,
             TestUrlRequestCallback::ON_READ_COMPLETED, true, false);
  TestCancel(TestUrlRequestCallback::CANCEL_ASYNC,
             TestUrlRequestCallback::ON_READ_COMPLETED, true, false);
  TestCancel(TestUrlRequestCallback::CANCEL_ASYNC_WITHOUT_PAUSE,
             TestUrlRequestCallback::ON_READ_COMPLETED, true, false);
}

TEST_P(UrlRequestTest, PerfTest) {
  const int kTestIterations = 10;
  const int kDownloadSize = 19307439;  // used for internal server only

  Cronet_EnginePtr engine = Cronet_Engine_Create();
  Cronet_EngineParamsPtr engine_params = Cronet_EngineParams_Create();
  Cronet_Engine_StartWithParams(engine, engine_params);

  std::string url = cronet::TestServer::PrepareBigDataURL(kDownloadSize);

  base::Time start = base::Time::Now();

  for (int i = 0; i < kTestIterations; ++i) {
    Cronet_UrlRequestPtr request = Cronet_UrlRequest_Create();
    Cronet_UrlRequestParamsPtr request_params =
        Cronet_UrlRequestParams_Create();
    TestUrlRequestCallback test_callback(GetParam());
    test_callback.set_accumulate_response_data(false);
    // Executor provided by the application is owned by |test_callback|.
    Cronet_ExecutorPtr executor = test_callback.GetExecutor();
    // Callback provided by the application.
    Cronet_UrlRequestCallbackPtr callback =
        test_callback.CreateUrlRequestCallback();

    Cronet_UrlRequest_InitWithParams(request, engine, url.c_str(),
                                     request_params, callback, executor);

    Cronet_UrlRequest_Start(request);
    test_callback.WaitForDone();

    EXPECT_TRUE(test_callback.IsDone());
    ASSERT_EQ(kDownloadSize, test_callback.response_data_length_);

    Cronet_UrlRequestParams_Destroy(request_params);
    Cronet_UrlRequest_Destroy(request);
    Cronet_UrlRequestCallback_Destroy(callback);
  }
  base::Time end = base::Time::Now();
  base::TimeDelta delta = end - start;

  LOG(INFO) << "Total time " << delta.InMillisecondsF() << " ms";
  LOG(INFO) << "Single Iteration time "
            << delta.InMillisecondsF() / kTestIterations << " ms";

  double bytes_per_ms =
      kDownloadSize * kTestIterations / delta.InMillisecondsF();
  double megabytes_per_ms = bytes_per_ms / 1000000;
  double megabits_per_second = megabytes_per_ms * 8 * 1000;
  LOG(INFO) << "Average Throughput: " << megabits_per_second << " mbps";

  Cronet_EngineParams_Destroy(engine_params);
  Cronet_Engine_Destroy(engine);
  cronet::TestServer::ReleaseBigDataURL();
}

TEST_P(UrlRequestTest, GetStatus) {
  Cronet_EnginePtr engine = cronet::test::CreateTestEngine(0);
  Cronet_UrlRequestPtr request = Cronet_UrlRequest_Create();
  Cronet_UrlRequestParamsPtr request_params = Cronet_UrlRequestParams_Create();
  std::string url = cronet::TestServer::GetSimpleURL();

  TestUrlRequestCallback test_callback(GetParam());
  test_callback.set_auto_advance(false);
  // Executor provided by the application is owned by |test_callback|.
  Cronet_ExecutorPtr executor = test_callback.GetExecutor();
  // Callback provided by the application.
  Cronet_UrlRequestCallbackPtr callback =
      test_callback.CreateUrlRequestCallback();

  Cronet_UrlRequest_InitWithParams(request, engine, url.c_str(), request_params,
                                   callback, executor);
  EXPECT_EQ(Cronet_UrlRequestStatusListener_Status_INVALID,
            GetRequestStatus(request, &test_callback));

  Cronet_UrlRequest_Start(request);
  EXPECT_LE(Cronet_UrlRequestStatusListener_Status_IDLE,
            GetRequestStatus(request, &test_callback));
  EXPECT_GE(Cronet_UrlRequestStatusListener_Status_READING_RESPONSE,
            GetRequestStatus(request, &test_callback));

  test_callback.WaitForNextStep();
  EXPECT_EQ(Cronet_UrlRequestStatusListener_Status_WAITING_FOR_DELEGATE,
            GetRequestStatus(request, &test_callback));

  Cronet_BufferPtr buffer = Cronet_Buffer_Create();
  Cronet_Buffer_InitWithAlloc(buffer, 100);
  Cronet_UrlRequest_Read(request, buffer);
  EXPECT_LE(Cronet_UrlRequestStatusListener_Status_IDLE,
            GetRequestStatus(request, &test_callback));
  EXPECT_GE(Cronet_UrlRequestStatusListener_Status_READING_RESPONSE,
            GetRequestStatus(request, &test_callback));

  test_callback.WaitForNextStep();
  EXPECT_LE(Cronet_UrlRequestStatusListener_Status_IDLE,
            GetRequestStatus(request, &test_callback));
  EXPECT_GE(Cronet_UrlRequestStatusListener_Status_READING_RESPONSE,
            GetRequestStatus(request, &test_callback));

  do {
    buffer = Cronet_Buffer_Create();
    Cronet_Buffer_InitWithAlloc(buffer, 100);
    Cronet_UrlRequest_Read(request, buffer);
    // Verify that late calls to GetStatus() don't invoke OnStatus() after
    // final callbacks.
    GetRequestStatus(request, &test_callback);
    test_callback.WaitForNextStep();
  } while (!test_callback.IsDone());

  EXPECT_EQ(Cronet_UrlRequestStatusListener_Status_INVALID,
            GetRequestStatus(request, &test_callback));
  ASSERT_EQ("The quick brown fox jumps over the lazy dog.",
            test_callback.response_as_string_);

  Cronet_UrlRequestParams_Destroy(request_params);
  Cronet_UrlRequest_Destroy(request);
  Cronet_UrlRequestCallback_Destroy(callback);
  Cronet_Engine_Destroy(engine);
}

}  // namespace
