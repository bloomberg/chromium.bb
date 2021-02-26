// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/system_proxy_manager.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "chrome/browser/chromeos/settings/device_settings_test_helper.h"
#include "chrome/browser/chromeos/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/chromeos/settings/stub_cros_settings_provider.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/shill/shill_clients.h"
#include "chromeos/dbus/system_proxy/system_proxy_client.h"
#include "chromeos/dbus/system_proxy/system_proxy_service.pb.h"
#include "chromeos/network/network_handler.h"
#include "components/arc/arc_prefs.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/http/http_auth.h"
#include "net/http/http_auth_cache.h"
#include "net/http/http_network_session.h"
#include "net/http/http_transaction_factory.h"
#include "net/url_request/url_request_context.h"
#include "services/network/network_context.h"
#include "services/network/network_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Invoke;
using testing::WithArg;

namespace {
constexpr char kBrowserUsername[] = "browser_username";
constexpr char kBrowserPassword[] = "browser_password";
constexpr char kKerberosActivePrincipalName[] = "kerberos_princ_name";
constexpr char kProxyAuthUrl[] = "http://example.com:3128";
constexpr char kProxyAuthEmptyPath[] = "http://example.com:3128/";
constexpr char kRealm[] = "My proxy";
constexpr char kScheme[] = "dIgEsT";
constexpr char kProxyAuthChallenge[] = "challenge";

std::unique_ptr<network::NetworkContext>
CreateNetworkContextForDefaultStoragePartition(
    network::NetworkService* network_service,
    content::BrowserContext* browser_context) {
  mojo::PendingRemote<network::mojom::NetworkContext> network_context_remote;
  auto params = network::mojom::NetworkContextParams::New();
  params->cert_verifier_params = content::GetCertVerifierParams(
      network::mojom::CertVerifierCreationParams::New());
  auto network_context = std::make_unique<network::NetworkContext>(
      network_service, network_context_remote.InitWithNewPipeAndPassReceiver(),
      std::move(params));
  content::BrowserContext::GetDefaultStoragePartition(browser_context)
      ->SetNetworkContextForTesting(std::move(network_context_remote));
  return network_context;
}

network::NetworkService* GetNetworkService() {
  content::GetNetworkService();
  // Wait for the Network Service to initialize on the IO thread.
  content::RunAllPendingInMessageLoop(content::BrowserThread::IO);
  return network::NetworkService::GetNetworkServiceForTesting();
}

}  // namespace

namespace policy {
// TODO(acostinas, https://crbug.com/1102351) Replace RunUntilIdle() in tests
// with RunLoop::Run() with explicit RunLoop::QuitClosure().
class SystemProxyManagerTest : public testing::Test {
 public:
  SystemProxyManagerTest() : local_state_(TestingBrowserProcess::GetGlobal()) {}
  ~SystemProxyManagerTest() override = default;

  // testing::Test
  void SetUp() override {
    testing::Test::SetUp();
    chromeos::shill_clients::InitializeFakes();
    chromeos::NetworkHandler::Initialize();

    profile_ = std::make_unique<TestingProfile>();
    chromeos::SystemProxyClient::InitializeFake();
    system_proxy_manager_ = std::make_unique<SystemProxyManager>(
        chromeos::CrosSettings::Get(), local_state_.Get());
    // Listen for pref changes for the primary profile.
    system_proxy_manager_->StartObservingPrimaryProfilePrefs(profile_.get());
  }

  void TearDown() override {
    system_proxy_manager_->StopObservingPrimaryProfilePrefs();
    system_proxy_manager_.reset();
    chromeos::SystemProxyClient::Shutdown();
    chromeos::NetworkHandler::Shutdown();
    chromeos::shill_clients::Shutdown();
  }

 protected:
  void SetPolicy(bool system_proxy_enabled,
                 const std::string& system_services_username,
                 const std::string& system_services_password) {
    base::DictionaryValue dict;
    dict.SetKey("system_proxy_enabled", base::Value(system_proxy_enabled));
    dict.SetKey("system_services_username",
                base::Value(system_services_username));
    dict.SetKey("system_services_password",
                base::Value(system_services_password));
    scoped_testing_cros_settings_.device_settings()->Set(
        chromeos::kSystemProxySettings, dict);
    task_environment_.RunUntilIdle();
  }

  chromeos::SystemProxyClient::TestInterface* client_test_interface() {
    return chromeos::SystemProxyClient::Get()->GetTestInterface();
  }

  content::BrowserTaskEnvironment task_environment_;
  ScopedTestingLocalState local_state_;
  chromeos::ScopedTestingCrosSettings scoped_testing_cros_settings_;
  std::unique_ptr<SystemProxyManager> system_proxy_manager_;
  std::unique_ptr<TestingProfile> profile_;
  chromeos::ScopedDeviceSettingsTestHelper device_settings_test_helper_;
  chromeos::ScopedStubInstallAttributes test_install_attributes_;
};

// Verifies requests to shut down are sent to System-proxy according to the
// |kSystemProxySettings| policy.
TEST_F(SystemProxyManagerTest, ShutDownDaemon) {
  EXPECT_EQ(0, client_test_interface()->GetShutDownCallCount());

  SetPolicy(false /* system_proxy_enabled */, "" /* system_services_username */,
            "" /* system_services_password */);
  // Don't send empty credentials.
  EXPECT_EQ(1, client_test_interface()->GetShutDownCallCount());
}

// Tests that |SystemProxyManager| sends the correct Kerberos details and
// updates to System-proxy.
TEST_F(SystemProxyManagerTest, KerberosConfig) {
  int expected_set_auth_details_call_count = 0;
  SetPolicy(true /* system_proxy_enabled */, "" /* system_services_username */,
            "" /* system_services_password */);
  EXPECT_EQ(++expected_set_auth_details_call_count,
            client_test_interface()->GetSetAuthenticationDetailsCallCount());

  local_state_.Get()->SetBoolean(prefs::kKerberosEnabled, true);
  EXPECT_EQ(++expected_set_auth_details_call_count,
            client_test_interface()->GetSetAuthenticationDetailsCallCount());

  system_proxy::SetAuthenticationDetailsRequest request =
      client_test_interface()->GetLastAuthenticationDetailsRequest();
  EXPECT_FALSE(request.has_credentials());
  EXPECT_TRUE(request.kerberos_enabled());
  EXPECT_EQ(request.traffic_type(), system_proxy::TrafficOrigin::SYSTEM);

  // Set an active principal name.
  profile_->GetPrefs()->SetString(prefs::kKerberosActivePrincipalName,
                                  kKerberosActivePrincipalName);
  EXPECT_EQ(++expected_set_auth_details_call_count,
            client_test_interface()->GetSetAuthenticationDetailsCallCount());

  profile_->GetPrefs()->SetBoolean(arc::prefs::kArcEnabled, true);
  EXPECT_EQ(++expected_set_auth_details_call_count,
            client_test_interface()->GetSetAuthenticationDetailsCallCount());

  request = client_test_interface()->GetLastAuthenticationDetailsRequest();
  EXPECT_EQ(kKerberosActivePrincipalName, request.active_principal_name());
  EXPECT_EQ(request.traffic_type(), system_proxy::TrafficOrigin::ALL);

  // Remove the active principal name.
  profile_->GetPrefs()->SetString(prefs::kKerberosActivePrincipalName, "");
  request = client_test_interface()->GetLastAuthenticationDetailsRequest();
  EXPECT_EQ("", request.active_principal_name());
  EXPECT_TRUE(request.kerberos_enabled());

  // Disable kerberos.
  local_state_.Get()->SetBoolean(prefs::kKerberosEnabled, false);
  request = client_test_interface()->GetLastAuthenticationDetailsRequest();
  EXPECT_FALSE(request.kerberos_enabled());
}

// Tests that when no user is signed in, credential requests are resolved to a
// D-Bus call which sends back to System-proxy empty credentials for the
// specified protection space.
TEST_F(SystemProxyManagerTest, UserCredentialsRequiredNoUser) {
  SetPolicy(true /* system_proxy_enabled */, "" /* system_services_username */,
            "" /* system_services_password */);
  system_proxy_manager_->StopObservingPrimaryProfilePrefs();
  system_proxy::ProtectionSpace protection_space;
  protection_space.set_origin(kProxyAuthUrl);
  protection_space.set_scheme(kScheme);
  protection_space.set_realm(kRealm);

  system_proxy::AuthenticationRequiredDetails details;
  details.set_bad_cached_credentials(false);
  *details.mutable_proxy_protection_space() = protection_space;

  client_test_interface()->SendAuthenticationRequiredSignal(details);
  task_environment_.RunUntilIdle();

  EXPECT_EQ(2, client_test_interface()->GetSetAuthenticationDetailsCallCount());

  system_proxy::SetAuthenticationDetailsRequest request =
      client_test_interface()->GetLastAuthenticationDetailsRequest();

  ASSERT_TRUE(request.has_protection_space());
  ASSERT_EQ(protection_space.SerializeAsString(),
            request.protection_space().SerializeAsString());

  ASSERT_TRUE(request.has_credentials());
  EXPECT_EQ("", request.credentials().username());
  EXPECT_EQ("", request.credentials().password());
  system_proxy_manager_->StartObservingPrimaryProfilePrefs(profile_.get());
}

// Tests that credential requests are resolved to a  D-Bus call which sends back
// to System-proxy credentials acquired from the NetworkService.
TEST_F(SystemProxyManagerTest, UserCredentialsRequestedFromNetworkService) {
  SetPolicy(true /* system_proxy_enabled */, "" /* system_services_username */,
            "" /* system_services_password */);

  // Setup the NetworkContext with credentials.
  std::unique_ptr<network::NetworkContext> network_context =
      CreateNetworkContextForDefaultStoragePartition(GetNetworkService(),
                                                     profile_.get());
  network_context->url_request_context()
      ->http_transaction_factory()
      ->GetSession()
      ->http_auth_cache()
      ->Add(GURL(kProxyAuthEmptyPath), net::HttpAuth::AUTH_PROXY, kRealm,
            net::HttpAuth::AUTH_SCHEME_DIGEST, net::NetworkIsolationKey(),
            kProxyAuthChallenge,
            net::AuthCredentials(base::ASCIIToUTF16(kBrowserUsername),
                                 base::ASCIIToUTF16(kBrowserPassword)),
            std::string() /* path */);

  system_proxy::ProtectionSpace protection_space;
  protection_space.set_origin(kProxyAuthUrl);
  protection_space.set_scheme(kScheme);
  protection_space.set_realm(kRealm);

  system_proxy::AuthenticationRequiredDetails details;
  *details.mutable_proxy_protection_space() = protection_space;

  EXPECT_EQ(1, client_test_interface()->GetSetAuthenticationDetailsCallCount());

  client_test_interface()->SendAuthenticationRequiredSignal(details);
  task_environment_.RunUntilIdle();

  EXPECT_EQ(2, client_test_interface()->GetSetAuthenticationDetailsCallCount());

  system_proxy::SetAuthenticationDetailsRequest request =
      client_test_interface()->GetLastAuthenticationDetailsRequest();

  ASSERT_TRUE(request.has_protection_space());
  EXPECT_EQ(protection_space.SerializeAsString(),
            request.protection_space().SerializeAsString());

  ASSERT_TRUE(request.has_credentials());
  EXPECT_EQ(kBrowserUsername, request.credentials().username());
  EXPECT_EQ(kBrowserPassword, request.credentials().password());
  EXPECT_EQ(request.traffic_type(), system_proxy::TrafficOrigin::SYSTEM);

  // Enable ARC and verify that the credentials are sent both for user and
  // system traffic.
  profile_->GetPrefs()->SetBoolean(arc::prefs::kArcEnabled, true);
  task_environment_.RunUntilIdle();
  client_test_interface()->SendAuthenticationRequiredSignal(details);
  task_environment_.RunUntilIdle();
  request = client_test_interface()->GetLastAuthenticationDetailsRequest();
  EXPECT_EQ(request.traffic_type(), system_proxy::TrafficOrigin::ALL);
}

// Tests that |SystemProxyManager| sends requests to start and shut down the
// worker which tunnels ARC++ traffic according to policy.
TEST_F(SystemProxyManagerTest, EnableArcWorker) {
  int expected_set_auth_details_call_count = 0;
  SetPolicy(true /* system_proxy_enabled */, "" /* system_services_username */,
            "" /* system_services_password */);

  EXPECT_EQ(++expected_set_auth_details_call_count,
            client_test_interface()->GetSetAuthenticationDetailsCallCount());

  profile_->GetPrefs()->SetBoolean(arc::prefs::kArcEnabled, true);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(++expected_set_auth_details_call_count,
            client_test_interface()->GetSetAuthenticationDetailsCallCount());

  profile_->GetPrefs()->SetBoolean(arc::prefs::kArcEnabled, false);
  EXPECT_EQ(1, client_test_interface()->GetShutDownCallCount());
}

// Tests that the user preference used by ARC++ to point to the local proxy is
// kept in sync.
TEST_F(SystemProxyManagerTest, ArcWorkerAddressPrefSynced) {
  const char kLocalProxyAddress[] = "local address";
  SetPolicy(true /* system_proxy_enabled */, "" /* system_services_username */,
            "" /* system_services_password */);

  system_proxy::WorkerActiveSignalDetails details;
  details.set_traffic_origin(system_proxy::TrafficOrigin::USER);
  details.set_local_proxy_url(kLocalProxyAddress);
  client_test_interface()->SendWorkerActiveSignal(details);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(kLocalProxyAddress,
            profile_->GetPrefs()->GetString(
                ::prefs::kSystemProxyUserTrafficHostAndPort));

  // The preference shouldn't be updated if the signal is send for system
  // traffic.
  details.set_traffic_origin(system_proxy::TrafficOrigin::SYSTEM);
  details.set_local_proxy_url("other address");
  client_test_interface()->SendWorkerActiveSignal(details);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(kLocalProxyAddress,
            profile_->GetPrefs()->GetString(
                ::prefs::kSystemProxyUserTrafficHostAndPort));

  SetPolicy(false /* system_proxy_enabled */, "" /* system_services_username */,
            "" /* system_services_password */);

  EXPECT_TRUE(profile_->GetPrefs()
                  ->GetString(::prefs::kSystemProxyUserTrafficHostAndPort)
                  .empty());
}

}  // namespace policy
