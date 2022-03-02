// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_TESTING_APPLICATION_CONTEXT_H_
#define IOS_CHROME_TEST_TESTING_APPLICATION_CONTEXT_H_

#include <memory>
#include <string>

#include "base/threading/thread_checker.h"
#include "ios/chrome/browser/application_context.h"

namespace network {
class TestNetworkConnectionTracker;
class TestURLLoaderFactory;
}  // namespace network

class TestingApplicationContext : public ApplicationContext {
 public:
  TestingApplicationContext();

  TestingApplicationContext(const TestingApplicationContext&) = delete;
  TestingApplicationContext& operator=(const TestingApplicationContext&) =
      delete;

  ~TestingApplicationContext() override;

  // Convenience method to get the current application context as a
  // TestingApplicationContext.
  static TestingApplicationContext* GetGlobal();

  // Sets the local state.
  void SetLocalState(PrefService* local_state);

  // Sets the last shutdown "clean" state.
  void SetLastShutdownClean(bool clean);

  // Sets the ChromeBrowserStateManager.
  void SetChromeBrowserStateManager(ios::ChromeBrowserStateManager* manager);

  // ApplicationContext implementation.
  void OnAppEnterForeground() override;
  void OnAppEnterBackground() override;
  bool WasLastShutdownClean() override;

  PrefService* GetLocalState() override;
  net::URLRequestContextGetter* GetSystemURLRequestContext() override;
  scoped_refptr<network::SharedURLLoaderFactory> GetSharedURLLoaderFactory()
      override;
  network::mojom::NetworkContext* GetSystemNetworkContext() override;
  const std::string& GetApplicationLocale() override;
  ios::ChromeBrowserStateManager* GetChromeBrowserStateManager() override;
  metrics_services_manager::MetricsServicesManager* GetMetricsServicesManager()
      override;
  metrics::MetricsService* GetMetricsService() override;
  ukm::UkmRecorder* GetUkmRecorder() override;
  variations::VariationsService* GetVariationsService() override;
  net::NetLog* GetNetLog() override;
  net_log::NetExportFileWriter* GetNetExportFileWriter() override;
  network_time::NetworkTimeTracker* GetNetworkTimeTracker() override;
  IOSChromeIOThread* GetIOSChromeIOThread() override;
  gcm::GCMDriver* GetGCMDriver() override;
  component_updater::ComponentUpdateService* GetComponentUpdateService()
      override;
  SafeBrowsingService* GetSafeBrowsingService() override;
  network::NetworkConnectionTracker* GetNetworkConnectionTracker() override;
  BrowserPolicyConnectorIOS* GetBrowserPolicyConnector() override;
  breadcrumbs::BreadcrumbPersistentStorageManager*
  GetBreadcrumbPersistentStorageManager() override;

 private:
  base::ThreadChecker thread_checker_;
  std::string application_locale_;
  PrefService* local_state_;

  // Must be destroyed after |local_state_|. BrowserStatePolicyConnector isn't a
  // keyed service because the pref service, which isn't a keyed service, has a
  // hard dependency on the policy infrastructure. In order to outlive the pref
  // service, the policy connector must live outside the keyed services.
  std::unique_ptr<BrowserPolicyConnectorIOS> browser_policy_connector_;

  ios::ChromeBrowserStateManager* chrome_browser_state_manager_;
  std::unique_ptr<network_time::NetworkTimeTracker> network_time_tracker_;
  bool was_last_shutdown_clean_;
  std::unique_ptr<network::TestURLLoaderFactory> test_url_loader_factory_;
  scoped_refptr<SafeBrowsingService> fake_safe_browsing_service_;
  std::unique_ptr<network::TestNetworkConnectionTracker>
      test_network_connection_tracker_;
};

#endif  // IOS_CHROME_TEST_TESTING_APPLICATION_CONTEXT_H_
