// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/web/WebEmbeddedWorker.h"

#include <memory>
#include "platform/WaitableEvent.h"
#include "platform/WebTaskRunner.h"
#include "platform/testing/URLTestHelpers.h"
#include "platform/testing/UnitTestHelpers.h"
#include "platform/wtf/PtrUtil.h"
#include "public/platform/Platform.h"
#include "public/platform/WebContentSettingsClient.h"
#include "public/platform/WebURLLoaderMockFactory.h"
#include "public/platform/WebURLResponse.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerInstalledScriptsManager.h"
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
  MockServiceWorkerContextClient() = default;
  ~MockServiceWorkerContextClient() override = default;

  MOCK_METHOD0(WorkerReadyForInspection, void());
  MOCK_METHOD0(WorkerContextFailedToStart, void());
  MOCK_METHOD0(WorkerScriptLoaded, void());

  void DidEvaluateWorkerScript(bool /* success */) override {
    script_evaluated_event_.Signal();
  }

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

  void WaitUntilScriptEvaluated() { script_evaluated_event_.Wait(); }
  void WaitUntilThreadTermination() { termination_event_.Wait(); }

 private:
  bool has_associated_registration_ = true;
  WaitableEvent script_evaluated_event_;
  WaitableEvent termination_event_;
};

class MockServiceWorkerInstalledScriptsManager
    : public WebServiceWorkerInstalledScriptsManager {
 public:
  MOCK_CONST_METHOD1(IsScriptInstalled, bool(const WebURL& script_url));
  MOCK_METHOD1(GetRawScriptData,
               std::unique_ptr<RawScriptData>(const WebURL& script_url));
};

class WebEmbeddedWorkerImplTest : public ::testing::Test {
 protected:
  void SetUp() override {
    auto client = WTF::MakeUnique<MockServiceWorkerContextClient>();
    auto installed_scripts_manager =
        WTF::MakeUnique<MockServiceWorkerInstalledScriptsManager>();
    mock_client_ = client.get();
    mock_installed_scripts_manager_ = installed_scripts_manager.get();
    worker_ = WebEmbeddedWorker::Create(
        std::move(client), std::move(installed_scripts_manager), nullptr);

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
  MockServiceWorkerInstalledScriptsManager* mock_installed_scripts_manager_;
  std::unique_ptr<WebEmbeddedWorker> worker_;
};

}  // namespace

TEST_F(WebEmbeddedWorkerImplTest, TerminateSoonAfterStart) {
  EXPECT_CALL(*mock_client_, WorkerReadyForInspection()).Times(1);
  worker_->StartWorkerContext(start_data_);
  ::testing::Mock::VerifyAndClearExpectations(mock_client_);
  ::testing::Mock::VerifyAndClearExpectations(mock_installed_scripts_manager_);

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
  ::testing::Mock::VerifyAndClearExpectations(mock_installed_scripts_manager_);

  EXPECT_CALL(*mock_client_, WorkerContextFailedToStart()).Times(1);
  worker_->TerminateWorkerContext();
  ::testing::Mock::VerifyAndClearExpectations(mock_client_);
}

TEST_F(WebEmbeddedWorkerImplTest, TerminateWhileLoadingScript) {
  EXPECT_CALL(*mock_client_, WorkerReadyForInspection()).Times(1);
  worker_->StartWorkerContext(start_data_);
  ::testing::Mock::VerifyAndClearExpectations(mock_client_);
  ::testing::Mock::VerifyAndClearExpectations(mock_installed_scripts_manager_);

  // Load the shadow page.
  EXPECT_CALL(*mock_client_, CreateServiceWorkerNetworkProviderProxy())
      .WillOnce(::testing::Return(nullptr));
  EXPECT_CALL(*mock_installed_scripts_manager_,
              IsScriptInstalled(start_data_.script_url))
      .Times(1)
      .WillOnce(::testing::Return(false));
  Platform::Current()->GetURLLoaderMockFactory()->ServeAsynchronousRequests();
  ::testing::Mock::VerifyAndClearExpectations(mock_client_);
  ::testing::Mock::VerifyAndClearExpectations(mock_installed_scripts_manager_);

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
  EXPECT_CALL(*mock_installed_scripts_manager_,
              IsScriptInstalled(start_data_.script_url))
      .Times(1)
      .WillOnce(::testing::Return(false));
  Platform::Current()->GetURLLoaderMockFactory()->ServeAsynchronousRequests();
  ::testing::Mock::VerifyAndClearExpectations(mock_client_);
  ::testing::Mock::VerifyAndClearExpectations(mock_installed_scripts_manager_);

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
  EXPECT_CALL(*mock_installed_scripts_manager_,
              IsScriptInstalled(start_data_.script_url))
      .Times(1)
      .WillOnce(::testing::Return(false));
  Platform::Current()->GetURLLoaderMockFactory()->ServeAsynchronousRequests();
  ::testing::Mock::VerifyAndClearExpectations(mock_client_);
  ::testing::Mock::VerifyAndClearExpectations(mock_installed_scripts_manager_);

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
  EXPECT_CALL(*mock_installed_scripts_manager_,
              IsScriptInstalled(start_data_.script_url))
      .Times(1)
      .WillOnce(::testing::Return(false));
  Platform::Current()->GetURLLoaderMockFactory()->ServeAsynchronousRequests();
  ::testing::Mock::VerifyAndClearExpectations(mock_client_);
  ::testing::Mock::VerifyAndClearExpectations(mock_installed_scripts_manager_);

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
  EXPECT_CALL(*mock_installed_scripts_manager_,
              IsScriptInstalled(start_data_.script_url))
      .Times(1)
      .WillOnce(::testing::Return(false));
  Platform::Current()->GetURLLoaderMockFactory()->ServeAsynchronousRequests();
  ::testing::Mock::VerifyAndClearExpectations(mock_client_);
  ::testing::Mock::VerifyAndClearExpectations(mock_installed_scripts_manager_);

  // Load the script.
  EXPECT_CALL(*mock_client_, WorkerScriptLoaded()).Times(1);
  EXPECT_CALL(*mock_client_, CreateServiceWorkerProviderProxy())
      .WillOnce(::testing::Return(nullptr));
  // This is called on the worker thread.
  EXPECT_CALL(*mock_installed_scripts_manager_,
              IsScriptInstalled(start_data_.script_url))
      .Times(1)
      .WillOnce(::testing::Return(false));
  Platform::Current()->GetURLLoaderMockFactory()->ServeAsynchronousRequests();
  mock_client_->WaitUntilScriptEvaluated();
  ::testing::Mock::VerifyAndClearExpectations(mock_client_);
  ::testing::Mock::VerifyAndClearExpectations(mock_installed_scripts_manager_);

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
  EXPECT_CALL(*mock_installed_scripts_manager_,
              IsScriptInstalled(start_data_.script_url))
      .Times(1)
      .WillOnce(::testing::Return(false));
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
  // This is called on the worker thread.
  EXPECT_CALL(*mock_installed_scripts_manager_,
              IsScriptInstalled(start_data_.script_url))
      .Times(1)
      .WillOnce(::testing::Return(false));
  worker_->ResumeAfterDownload();
  mock_client_->WaitUntilScriptEvaluated();
  ::testing::Mock::VerifyAndClearExpectations(mock_client_);
  ::testing::Mock::VerifyAndClearExpectations(mock_installed_scripts_manager_);

  // Terminate the running worker thread.
  EXPECT_CALL(*mock_client_, WorkerContextFailedToStart()).Times(0);
  worker_->TerminateWorkerContext();
  mock_client_->WaitUntilThreadTermination();
}

}  // namespace blink
