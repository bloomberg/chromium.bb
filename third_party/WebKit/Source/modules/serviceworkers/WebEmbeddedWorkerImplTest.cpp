// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/web/WebEmbeddedWorker.h"

#include <memory>
#include "platform/WaitableEvent.h"
#include "platform/testing/URLTestHelpers.h"
#include "platform/testing/UnitTestHelpers.h"
#include "platform/wtf/PtrUtil.h"
#include "public/platform/Platform.h"
#include "public/platform/WebURLLoaderMockFactory.h"
#include "public/platform/WebURLResponse.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerNetworkProvider.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerProvider.h"
#include "public/web/WebEmbeddedWorkerStartData.h"
#include "public/web/WebSettings.h"
#include "public/web/modules/serviceworker/WebServiceWorkerContextClient.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace {

class MockServiceWorkerContextClient : public WebServiceWorkerContextClient {
 public:
  MockServiceWorkerContextClient() {}
  ~MockServiceWorkerContextClient() override {}
  MOCK_METHOD0(WorkerReadyForInspection, void());
  MOCK_METHOD0(WorkerContextFailedToStart, void());
  MOCK_METHOD0(WorkerScriptLoaded, void());

  // Work-around for mocking a method that return unique_ptr.
  MOCK_METHOD0(CreateServiceWorkerNetworkProviderProxy,
               WebServiceWorkerNetworkProvider*());
  MOCK_METHOD0(CreateServiceWorkerProviderProxy, WebServiceWorkerProvider*());
  std::unique_ptr<WebServiceWorkerNetworkProvider>
  CreateServiceWorkerNetworkProvider() override {
    return std::unique_ptr<WebServiceWorkerNetworkProvider>(
        CreateServiceWorkerNetworkProviderProxy());
  }
  std::unique_ptr<WebServiceWorkerProvider> CreateServiceWorkerProvider()
      override {
    return std::unique_ptr<WebServiceWorkerProvider>(
        CreateServiceWorkerProviderProxy());
  }

  bool HasAssociatedRegistration() override {
    return has_associated_registration_;
  }
  void SetHasAssociatedRegistration(bool has_associated_registration) {
    has_associated_registration_ = has_associated_registration;
  }
  void GetClient(const WebString&,
                 std::unique_ptr<WebServiceWorkerClientCallbacks>) override {
    NOTREACHED();
  }
  void GetClients(const WebServiceWorkerClientQueryOptions&,
                  std::unique_ptr<WebServiceWorkerClientsCallbacks>) override {
    NOTREACHED();
  }
  void OpenNewTab(const WebURL&,
                  std::unique_ptr<WebServiceWorkerClientCallbacks>) override {
    NOTREACHED();
  }
  void OpenNewPopup(const WebURL&,
                    std::unique_ptr<WebServiceWorkerClientCallbacks>) override {
    NOTREACHED();
  }
  void PostMessageToClient(const WebString& uuid,
                           const WebString&,
                           WebMessagePortChannelArray) override {
    NOTREACHED();
  }
  void SkipWaiting(
      std::unique_ptr<WebServiceWorkerSkipWaitingCallbacks>) override {
    NOTREACHED();
  }
  void Claim(std::unique_ptr<WebServiceWorkerClientsClaimCallbacks>) override {
    NOTREACHED();
  }
  void Focus(const WebString& uuid,
             std::unique_ptr<WebServiceWorkerClientCallbacks>) override {
    NOTREACHED();
  }
  void Navigate(const WebString& uuid,
                const WebURL&,
                std::unique_ptr<WebServiceWorkerClientCallbacks>) override {
    NOTREACHED();
  }
  void RegisterForeignFetchScopes(
      int install_event_id,
      const WebVector<WebURL>& sub_scopes,
      const WebVector<WebSecurityOrigin>& origins) override {
    NOTREACHED();
  }

  void WorkerContextDestroyed() override { termination_event_.Signal(); }

  void WaitUntilThreadTermination() { termination_event_.Wait(); }

 private:
  bool has_associated_registration_ = true;
  WaitableEvent termination_event_;
};

class WebEmbeddedWorkerImplTest : public ::testing::Test {
 protected:
  void SetUp() override {
    mock_client_ = new MockServiceWorkerContextClient();
    worker_ = WTF::WrapUnique(WebEmbeddedWorker::Create(mock_client_, nullptr));

    WebURL script_url = URLTestHelpers::ToKURL("https://www.example.com/sw.js");
    WebURLResponse response;
    response.SetMIMEType("text/javascript");
    response.SetHTTPStatusCode(200);
    Platform::Current()->GetURLLoaderMockFactory()->RegisterURL(script_url,
                                                                response, "");

    start_data_.script_url = script_url;
    start_data_.user_agent = WebString("dummy user agent");
    start_data_.pause_after_download_mode =
        WebEmbeddedWorkerStartData::kDontPauseAfterDownload;
    start_data_.wait_for_debugger_mode =
        WebEmbeddedWorkerStartData::kDontWaitForDebugger;
    start_data_.v8_cache_options = WebSettings::kV8CacheOptionsDefault;
  }

  void TearDown() override {
    Platform::Current()
        ->GetURLLoaderMockFactory()
        ->UnregisterAllURLsAndClearMemoryCache();
  }

  WebEmbeddedWorkerStartData start_data_;
  MockServiceWorkerContextClient* mock_client_;
  std::unique_ptr<WebEmbeddedWorker> worker_;
};

}  // namespace

TEST_F(WebEmbeddedWorkerImplTest, TerminateSoonAfterStart) {
  EXPECT_CALL(*mock_client_, WorkerReadyForInspection()).Times(1);
  worker_->StartWorkerContext(start_data_);
  ::testing::Mock::VerifyAndClearExpectations(mock_client_);

  EXPECT_CALL(*mock_client_, WorkerContextFailedToStart()).Times(1);
  worker_->TerminateWorkerContext();
  ::testing::Mock::VerifyAndClearExpectations(mock_client_);
}

TEST_F(WebEmbeddedWorkerImplTest, TerminateWhileWaitingForDebugger) {
  EXPECT_CALL(*mock_client_, WorkerReadyForInspection()).Times(1);
  start_data_.wait_for_debugger_mode =
      WebEmbeddedWorkerStartData::kWaitForDebugger;
  worker_->StartWorkerContext(start_data_);
  ::testing::Mock::VerifyAndClearExpectations(mock_client_);

  EXPECT_CALL(*mock_client_, WorkerContextFailedToStart()).Times(1);
  worker_->TerminateWorkerContext();
  ::testing::Mock::VerifyAndClearExpectations(mock_client_);
}

TEST_F(WebEmbeddedWorkerImplTest, TerminateWhileLoadingScript) {
  EXPECT_CALL(*mock_client_, WorkerReadyForInspection()).Times(1);
  worker_->StartWorkerContext(start_data_);
  ::testing::Mock::VerifyAndClearExpectations(mock_client_);

  // Load the shadow page.
  EXPECT_CALL(*mock_client_, CreateServiceWorkerNetworkProviderProxy())
      .WillOnce(::testing::Return(nullptr));
  Platform::Current()->GetURLLoaderMockFactory()->ServeAsynchronousRequests();
  ::testing::Mock::VerifyAndClearExpectations(mock_client_);

  // Terminate before loading the script.
  EXPECT_CALL(*mock_client_, WorkerContextFailedToStart()).Times(1);
  worker_->TerminateWorkerContext();
  ::testing::Mock::VerifyAndClearExpectations(mock_client_);
}

TEST_F(WebEmbeddedWorkerImplTest, TerminateWhilePausedAfterDownload) {
  EXPECT_CALL(*mock_client_, WorkerReadyForInspection()).Times(1);
  start_data_.pause_after_download_mode =
      WebEmbeddedWorkerStartData::kPauseAfterDownload;
  worker_->StartWorkerContext(start_data_);
  ::testing::Mock::VerifyAndClearExpectations(mock_client_);

  // Load the shadow page.
  EXPECT_CALL(*mock_client_, CreateServiceWorkerNetworkProviderProxy())
      .WillOnce(::testing::Return(nullptr));
  Platform::Current()->GetURLLoaderMockFactory()->ServeAsynchronousRequests();
  ::testing::Mock::VerifyAndClearExpectations(mock_client_);

  // Load the script.
  EXPECT_CALL(*mock_client_, WorkerScriptLoaded()).Times(1);
  EXPECT_CALL(*mock_client_, CreateServiceWorkerProviderProxy()).Times(0);
  Platform::Current()->GetURLLoaderMockFactory()->ServeAsynchronousRequests();
  ::testing::Mock::VerifyAndClearExpectations(mock_client_);

  // Terminate before resuming after download.
  EXPECT_CALL(*mock_client_, CreateServiceWorkerProviderProxy()).Times(0);
  EXPECT_CALL(*mock_client_, WorkerContextFailedToStart()).Times(1);
  worker_->TerminateWorkerContext();
  ::testing::Mock::VerifyAndClearExpectations(mock_client_);
}

TEST_F(WebEmbeddedWorkerImplTest, ScriptNotFound) {
  WebURL script_url =
      URLTestHelpers::ToKURL("https://www.example.com/sw-404.js");
  WebURLResponse response;
  response.SetMIMEType("text/javascript");
  response.SetHTTPStatusCode(404);
  WebURLError error;
  error.reason = 1010;
  error.domain = "WebEmbeddedWorkerImplTest";
  Platform::Current()->GetURLLoaderMockFactory()->RegisterErrorURL(
      script_url, response, error);
  start_data_.script_url = script_url;

  EXPECT_CALL(*mock_client_, WorkerReadyForInspection()).Times(1);
  worker_->StartWorkerContext(start_data_);
  ::testing::Mock::VerifyAndClearExpectations(mock_client_);

  // Load the shadow page.
  EXPECT_CALL(*mock_client_, CreateServiceWorkerNetworkProviderProxy())
      .WillOnce(::testing::Return(nullptr));
  Platform::Current()->GetURLLoaderMockFactory()->ServeAsynchronousRequests();
  ::testing::Mock::VerifyAndClearExpectations(mock_client_);

  // Load the script.
  EXPECT_CALL(*mock_client_, WorkerScriptLoaded()).Times(0);
  EXPECT_CALL(*mock_client_, CreateServiceWorkerProviderProxy()).Times(0);
  EXPECT_CALL(*mock_client_, WorkerContextFailedToStart()).Times(1);
  Platform::Current()->GetURLLoaderMockFactory()->ServeAsynchronousRequests();
  ::testing::Mock::VerifyAndClearExpectations(mock_client_);
}

TEST_F(WebEmbeddedWorkerImplTest, NoRegistration) {
  EXPECT_CALL(*mock_client_, WorkerReadyForInspection()).Times(1);
  start_data_.pause_after_download_mode =
      WebEmbeddedWorkerStartData::kPauseAfterDownload;
  worker_->StartWorkerContext(start_data_);
  ::testing::Mock::VerifyAndClearExpectations(mock_client_);

  // Load the shadow page.
  EXPECT_CALL(*mock_client_, CreateServiceWorkerNetworkProviderProxy())
      .WillOnce(::testing::Return(nullptr));
  Platform::Current()->GetURLLoaderMockFactory()->ServeAsynchronousRequests();
  ::testing::Mock::VerifyAndClearExpectations(mock_client_);

  // Load the script.
  mock_client_->SetHasAssociatedRegistration(false);
  EXPECT_CALL(*mock_client_, WorkerScriptLoaded()).Times(0);
  EXPECT_CALL(*mock_client_, CreateServiceWorkerProviderProxy()).Times(0);
  EXPECT_CALL(*mock_client_, WorkerContextFailedToStart()).Times(1);
  Platform::Current()->GetURLLoaderMockFactory()->ServeAsynchronousRequests();
  ::testing::Mock::VerifyAndClearExpectations(mock_client_);
}

// The running worker is detected as a memory leak. crbug.com/586897
#if defined(ADDRESS_SANITIZER)
#define MAYBE_DontPauseAfterDownload DISABLED_DontPauseAfterDownload
#else
#define MAYBE_DontPauseAfterDownload DontPauseAfterDownload
#endif

TEST_F(WebEmbeddedWorkerImplTest, MAYBE_DontPauseAfterDownload) {
  EXPECT_CALL(*mock_client_, WorkerReadyForInspection()).Times(1);
  worker_->StartWorkerContext(start_data_);
  ::testing::Mock::VerifyAndClearExpectations(mock_client_);

  // Load the shadow page.
  EXPECT_CALL(*mock_client_, CreateServiceWorkerNetworkProviderProxy())
      .WillOnce(::testing::Return(nullptr));
  Platform::Current()->GetURLLoaderMockFactory()->ServeAsynchronousRequests();
  ::testing::Mock::VerifyAndClearExpectations(mock_client_);

  // Load the script.
  EXPECT_CALL(*mock_client_, WorkerScriptLoaded()).Times(1);
  EXPECT_CALL(*mock_client_, CreateServiceWorkerProviderProxy())
      .WillOnce(::testing::Return(nullptr));
  Platform::Current()->GetURLLoaderMockFactory()->ServeAsynchronousRequests();
  ::testing::Mock::VerifyAndClearExpectations(mock_client_);

  // Terminate the running worker thread.
  EXPECT_CALL(*mock_client_, WorkerContextFailedToStart()).Times(0);
  worker_->TerminateWorkerContext();
  mock_client_->WaitUntilThreadTermination();
}

// The running worker is detected as a memory leak. crbug.com/586897
#if defined(ADDRESS_SANITIZER)
#define MAYBE_PauseAfterDownload DISABLED_PauseAfterDownload
#else
#define MAYBE_PauseAfterDownload PauseAfterDownload
#endif

TEST_F(WebEmbeddedWorkerImplTest, MAYBE_PauseAfterDownload) {
  EXPECT_CALL(*mock_client_, WorkerReadyForInspection()).Times(1);
  start_data_.pause_after_download_mode =
      WebEmbeddedWorkerStartData::kPauseAfterDownload;
  worker_->StartWorkerContext(start_data_);
  ::testing::Mock::VerifyAndClearExpectations(mock_client_);

  // Load the shadow page.
  EXPECT_CALL(*mock_client_, CreateServiceWorkerNetworkProviderProxy())
      .WillOnce(::testing::Return(nullptr));
  Platform::Current()->GetURLLoaderMockFactory()->ServeAsynchronousRequests();
  ::testing::Mock::VerifyAndClearExpectations(mock_client_);

  // Load the script.
  EXPECT_CALL(*mock_client_, WorkerScriptLoaded()).Times(1);
  EXPECT_CALL(*mock_client_, CreateServiceWorkerProviderProxy()).Times(0);
  Platform::Current()->GetURLLoaderMockFactory()->ServeAsynchronousRequests();
  ::testing::Mock::VerifyAndClearExpectations(mock_client_);

  // Resume after download.
  EXPECT_CALL(*mock_client_, CreateServiceWorkerProviderProxy())
      .WillOnce(::testing::Return(nullptr));
  worker_->ResumeAfterDownload();
  ::testing::Mock::VerifyAndClearExpectations(mock_client_);

  // Terminate the running worker thread.
  EXPECT_CALL(*mock_client_, WorkerContextFailedToStart()).Times(0);
  worker_->TerminateWorkerContext();
  mock_client_->WaitUntilThreadTermination();
}

}  // namespace blink
