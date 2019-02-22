// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/cloud/test_request_interceptor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/dm_auth.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/core/common/policy_switches.h"
#include "components/policy/core/common/policy_test_utils.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_request_context_getter.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/policy/user_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/policy/user_policy_manager_factory_chromeos.h"
#else
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/policy/cloud/user_cloud_policy_manager_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "components/signin/core/browser/signin_manager.h"
#endif

using content::BrowserThread;
using testing::AnyNumber;
using testing::InvokeWithoutArgs;
using testing::Mock;
using testing::_;

namespace em = enterprise_management;

namespace policy {

// Tests the cloud policy stack using a URLRequestJobFactory::ProtocolHandler
// to intercept requests and produce canned responses.
class CloudPolicyManagerTest : public InProcessBrowserTest {
 protected:
  CloudPolicyManagerTest() {}
  ~CloudPolicyManagerTest() override {}

  void SetUpInProcessBrowserTestFixture() override {
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    command_line->AppendSwitchASCII(switches::kDeviceManagementUrl,
                                    "http://localhost");

    // Set retry delay to prevent timeouts.
    policy::DeviceManagementService::SetRetryDelayForTesting(0);
  }

  void SetUpOnMainThread() override {
    ASSERT_TRUE(PolicyServiceIsEmpty(g_browser_process->policy_service()))
        << "Pre-existing policies in this machine will make this test fail.";

    test_url_loader_factory_ =
        std::make_unique<network::TestURLLoaderFactory>();
    test_shared_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            test_url_loader_factory_.get());

    BrowserPolicyConnector* connector =
        g_browser_process->browser_policy_connector();
    connector->ScheduleServiceInitialization(0);

#if defined(OS_CHROMEOS)
    policy_manager()->core()->client()->SetURLLoaderFactoryForTesting(
        test_shared_loader_factory_);
#else
    // Mock a signed-in user. This is used by the UserCloudPolicyStore to pass
    // the username to the UserCloudPolicyValidator.
    SigninManager* signin_manager =
        SigninManagerFactory::GetForProfile(browser()->profile());
    ASSERT_TRUE(signin_manager);
    signin_manager->SetAuthenticatedAccountInfo("12345", "user@example.com");

    ASSERT_TRUE(policy_manager());
    policy_manager()->Connect(g_browser_process->local_state(),
                              UserCloudPolicyManager::CreateCloudPolicyClient(
                                  connector->device_management_service(),
                                  test_shared_loader_factory_));
#endif
  }

  void TearDownOnMainThread() override {
    // Need to detach since |test_url_loader_factory_| will go away after this
    // destructor, but other code might be referencing
    // |test_shared_loader_factory_|.
    test_shared_loader_factory_->Detach();
    // Verify that all the expected requests were handled.
    EXPECT_EQ(0, test_url_loader_factory_->NumPending());
  }

#if defined(OS_CHROMEOS)
  UserCloudPolicyManagerChromeOS* policy_manager() {
    return UserPolicyManagerFactoryChromeOS::GetCloudPolicyManagerForProfile(
        browser()->profile());
  }
#else
  UserCloudPolicyManager* policy_manager() {
    return UserCloudPolicyManagerFactory::GetForBrowserContext(
        browser()->profile());
  }
#endif  // defined(OS_CHROMEOS)

  // Register the client of the policy_manager() using a bogus auth token, and
  // returns once the registration gets a result back.
  void Register() {
    ASSERT_TRUE(policy_manager());
    ASSERT_TRUE(policy_manager()->core()->client());

    base::RunLoop run_loop;
    MockCloudPolicyClientObserver observer;
    EXPECT_CALL(observer, OnRegistrationStateChanged(_))
        .Times(AnyNumber())
        .WillRepeatedly(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    EXPECT_CALL(observer, OnClientError(_))
        .Times(AnyNumber())
        .WillRepeatedly(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    policy_manager()->core()->client()->AddObserver(&observer);

    // Give a bogus OAuth token to the |policy_manager|. This should make its
    // CloudPolicyClient fetch the DMToken.
    em::DeviceRegisterRequest::Type registration_type =
#if defined(OS_CHROMEOS)
        em::DeviceRegisterRequest::USER;
#else
        em::DeviceRegisterRequest::BROWSER;
#endif
    policy_manager()->core()->client()->Register(
        registration_type, em::DeviceRegisterRequest::FLAVOR_USER_REGISTRATION,
        em::DeviceRegisterRequest::LIFETIME_INDEFINITE,
        em::LicenseType::UNDEFINED, DMAuth::FromOAuthToken("bogus"),
        std::string(), std::string(), std::string());
    run_loop.Run();
    Mock::VerifyAndClearExpectations(&observer);
    policy_manager()->core()->client()->RemoveObserver(&observer);
  }

  std::unique_ptr<network::TestURLLoaderFactory> test_url_loader_factory_;
  scoped_refptr<network::WeakWrapperSharedURLLoaderFactory>
      test_shared_loader_factory_;
};

IN_PROC_BROWSER_TEST_F(CloudPolicyManagerTest, Register) {
  // Accept one register request. The initial request should not include the
  // reregister flag.
  em::DeviceRegisterRequest::Type expected_type =
#if defined(OS_CHROMEOS)
      em::DeviceRegisterRequest::USER;
#else
      em::DeviceRegisterRequest::BROWSER;
#endif
  const bool expect_reregister = false;
  test_url_loader_factory_->SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        TestRequestInterceptor::RespondToRegisterWithSuccess(
            expected_type, expect_reregister, request,
            test_url_loader_factory_.get());
      }));

  EXPECT_FALSE(policy_manager()->core()->client()->is_registered());
  ASSERT_NO_FATAL_FAILURE(Register());
  EXPECT_TRUE(policy_manager()->core()->client()->is_registered());
}

IN_PROC_BROWSER_TEST_F(CloudPolicyManagerTest, RegisterFails) {
  test_url_loader_factory_->SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        test_url_loader_factory_->AddResponse(request.url.spec(), std::string(),
                                              net::HTTP_BAD_REQUEST);
      }));

  EXPECT_FALSE(policy_manager()->core()->client()->is_registered());
  ASSERT_NO_FATAL_FAILURE(Register());
  EXPECT_FALSE(policy_manager()->core()->client()->is_registered());
}

IN_PROC_BROWSER_TEST_F(CloudPolicyManagerTest, RegisterFailsWithRetries) {
  // Fail 4 times with ERR_NETWORK_CHANGED; the first 3 will trigger a retry,
  // the last one will forward the error to the client and unblock the
  // register process.
  int count = 0;
  test_url_loader_factory_->SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        network::URLLoaderCompletionStatus status(net::ERR_NETWORK_CHANGED);
        test_url_loader_factory_->AddResponse(request.url,
                                              network::ResourceResponseHead(),
                                              std::string(), status);
        ++count;
      }));

  EXPECT_FALSE(policy_manager()->core()->client()->is_registered());
  ASSERT_NO_FATAL_FAILURE(Register());
  EXPECT_FALSE(policy_manager()->core()->client()->is_registered());
  EXPECT_EQ(4, count);
}

IN_PROC_BROWSER_TEST_F(CloudPolicyManagerTest, RegisterWithRetry) {
  em::DeviceRegisterRequest::Type expected_type =
#if defined(OS_CHROMEOS)
      em::DeviceRegisterRequest::USER;
#else
      em::DeviceRegisterRequest::BROWSER;
#endif
  const bool expect_reregister = true;

  // Accept one register request after failing once. The retry request should
  // set the reregister flag.
  bool gave_error = false;
  test_url_loader_factory_->SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        if (!gave_error) {
          gave_error = true;
          network::URLLoaderCompletionStatus status(net::ERR_NETWORK_CHANGED);
          test_url_loader_factory_->AddResponse(request.url,
                                                network::ResourceResponseHead(),
                                                std::string(), status);
          return;
        }

        TestRequestInterceptor::RespondToRegisterWithSuccess(
            expected_type, expect_reregister, request,
            test_url_loader_factory_.get());
      }));

  EXPECT_FALSE(policy_manager()->core()->client()->is_registered());
  ASSERT_NO_FATAL_FAILURE(Register());
  EXPECT_TRUE(policy_manager()->core()->client()->is_registered());
}

}  // namespace policy
