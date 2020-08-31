// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <atomic>
#include <functional>
#include <map>
#include <string>

#include "cast/common/public/service_info.h"
#include "discovery/common/config.h"
#include "discovery/common/reporting_client.h"
#include "discovery/public/dns_sd_service_factory.h"
#include "discovery/public/dns_sd_service_publisher.h"
#include "discovery/public/dns_sd_service_watcher.h"
#include "gtest/gtest.h"
#include "platform/api/logging.h"
#include "platform/api/udp_socket.h"
#include "platform/base/interface_info.h"
#include "platform/impl/network_interface.h"
#include "platform/impl/platform_client_posix.h"
#include "platform/impl/task_runner.h"

namespace openscreen {
namespace cast {
namespace {

// Total wait time = 4 seconds.
constexpr std::chrono::milliseconds kWaitLoopSleepTime =
    std::chrono::milliseconds(500);
constexpr int kMaxWaitLoopIterations = 8;

// Total wait time = 2.5 seconds.
// NOTE: This must be less than the above wait time.
constexpr std::chrono::milliseconds kCheckLoopSleepTime =
    std::chrono::milliseconds(100);
constexpr int kMaxCheckLoopIterations = 25;

}  // namespace

// Publishes new service instances.
class Publisher : public discovery::DnsSdServicePublisher<ServiceInfo> {
 public:
  Publisher(discovery::DnsSdService* service)
      : DnsSdServicePublisher<ServiceInfo>(service,
                                           kCastV2ServiceId,
                                           ServiceInfoToDnsSdInstance) {
    OSP_LOG << "Initializing Publisher...\n";
  }

  ~Publisher() override = default;

  bool IsInstanceIdClaimed(const std::string& requested_id) {
    auto it =
        std::find(instance_ids_.begin(), instance_ids_.end(), requested_id);
    return it != instance_ids_.end();
  }

 private:
  // DnsSdPublisher::Client overrides.
  void OnInstanceClaimed(const std::string& requested_id) override {
    instance_ids_.push_back(requested_id);
  }

  std::vector<std::string> instance_ids_;
};

// Receives incoming services and outputs their results to stdout.
class Receiver : public discovery::DnsSdServiceWatcher<ServiceInfo> {
 public:
  Receiver(discovery::DnsSdService* service)
      : discovery::DnsSdServiceWatcher<ServiceInfo>(
            service,
            kCastV2ServiceId,
            DnsSdInstanceEndpointToServiceInfo,
            [this](
                std::vector<std::reference_wrapper<const ServiceInfo>> infos) {
              ProcessResults(std::move(infos));
            }) {
    OSP_LOG << "Initializing Receiver...";
  }

  bool IsServiceFound(const ServiceInfo& check_service) {
    return std::find_if(service_infos_.begin(), service_infos_.end(),
                        [&check_service](const ServiceInfo& info) {
                          return info.friendly_name ==
                                 check_service.friendly_name;
                        }) != service_infos_.end();
  }

  void EraseReceivedServices() { service_infos_.clear(); }

 private:
  void ProcessResults(
      std::vector<std::reference_wrapper<const ServiceInfo>> infos) {
    service_infos_.clear();
    for (const ServiceInfo& info : infos) {
      service_infos_.push_back(info);
    }
  }

  std::vector<ServiceInfo> service_infos_;
};

class FailOnErrorReporting : public discovery::ReportingClient {
  void OnFatalError(Error error) override {
    OSP_NOTREACHED() << "Fatal error received: '" << error << "'";
  }

  void OnRecoverableError(Error error) override {
    // Pending resolution of openscreen:105, logging recoverable errors is
    // disabled, as this will end up polluting the output with logs related to
    // mDNS messages received from non-loopback network interfaces over which
    // we have no control.
  }
};

discovery::Config GetConfigSettings() {
  discovery::Config config;

  // Get the loopback interface to run on.
  absl::optional<InterfaceInfo> loopback = GetLoopbackInterfaceForTesting();
  OSP_CHECK(loopback.has_value());
  discovery::Config::NetworkInfo::AddressFamilies address_families =
      discovery::Config::NetworkInfo::kUseIpV4 |
      discovery::Config::NetworkInfo::kUseIpV6;
  config.network_info.push_back({loopback.value(), address_families});

  return config;
}

class DiscoveryE2ETest : public testing::Test {
 public:
  DiscoveryE2ETest() {
    // Sleep to let any packets clear off the network before further tests.
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    PlatformClientPosix::Create(Clock::duration{50}, Clock::duration{50});
    task_runner_ = PlatformClientPosix::GetInstance()->GetTaskRunner();
  }

  ~DiscoveryE2ETest() {
    OSP_LOG << "TEST COMPLETE!";
    dnssd_service_.reset();
    PlatformClientPosix::ShutDown();
  }

 protected:
  ServiceInfo GetInfo(int id) {
    ServiceInfo hosted_service;
    hosted_service.port = 1234;
    hosted_service.unique_id = "id" + std::to_string(id);
    hosted_service.model_name = "openscreen-Model" + std::to_string(id);
    hosted_service.friendly_name = "Demo" + std::to_string(id);
    return hosted_service;
  }

  void SetUpService(const discovery::Config& config) {
    OSP_DCHECK(!dnssd_service_.get());
    std::atomic_bool done{false};
    task_runner_->PostTask([this, &config, &done]() {
      dnssd_service_ = discovery::CreateDnsSdService(
          task_runner_, &reporting_client_, config);
      receiver_ = std::make_unique<Receiver>(dnssd_service_.get());
      publisher_ = std::make_unique<Publisher>(dnssd_service_.get());
      done = true;
    });
    for (int i = 0; i < kMaxWaitLoopIterations; ++i) {
      if (done) {
        break;
      }
      std::this_thread::sleep_for(kWaitLoopSleepTime);
    }
    OSP_CHECK(done);
  }

  void StartDiscovery() {
    OSP_DCHECK(dnssd_service_.get());
    task_runner_->PostTask([this]() { receiver_->StartDiscovery(); });
  }

  template <typename... RecordTypes>
  void UpdateRecords(RecordTypes... records) {
    OSP_DCHECK(dnssd_service_.get());
    OSP_DCHECK(publisher_.get());

    std::vector<ServiceInfo> record_set{std::move(records)...};
    for (ServiceInfo& record : record_set) {
      task_runner_->PostTask([this, r = std::move(record)]() {
        auto error = publisher_->UpdateRegistration(r);
        OSP_CHECK(error.ok()) << "\tFailed to update service instance '"
                              << r.friendly_name << "': " << error << "!";
      });
    }
  }

  template <typename... RecordTypes>
  void PublishRecords(RecordTypes... records) {
    OSP_DCHECK(dnssd_service_.get());
    OSP_DCHECK(publisher_.get());

    std::vector<ServiceInfo> record_set{std::move(records)...};
    for (ServiceInfo& record : record_set) {
      task_runner_->PostTask([this, r = std::move(record)]() {
        auto error = publisher_->Register(r);
        OSP_CHECK(error.ok()) << "\tFailed to publish service instance '"
                              << r.friendly_name << "': " << error << "!";
      });
    }
  }

  template <typename... AtomicBoolPtrs>
  void WaitUntilSeen(bool should_be_seen, AtomicBoolPtrs... bools) {
    OSP_DCHECK(dnssd_service_.get());
    std::vector<std::atomic_bool*> atomic_bools{bools...};

    int waiting_on = atomic_bools.size();
    for (int i = 0; i < kMaxWaitLoopIterations; i++) {
      waiting_on = atomic_bools.size();
      for (std::atomic_bool* atomic : atomic_bools) {
        if (*atomic) {
          OSP_CHECK(should_be_seen) << "Found service instance!";
          waiting_on--;
        }
      }

      if (waiting_on) {
        OSP_LOG << "\tWaiting on " << waiting_on << "...";
        std::this_thread::sleep_for(kWaitLoopSleepTime);
        continue;
      }
      return;
    }
    OSP_CHECK(!should_be_seen)
        << "Could not find " << waiting_on << " service instances!";
  }

  void CheckForClaimedIds(ServiceInfo service_info,
                          std::atomic_bool* has_been_seen) {
    OSP_DCHECK(dnssd_service_.get());
    task_runner_->PostTask(
        [this, info = std::move(service_info), has_been_seen]() mutable {
          CheckForClaimedIds(std::move(info), has_been_seen, 0);
        });
  }

  void CheckForPublishedService(ServiceInfo service_info,
                                std::atomic_bool* has_been_seen) {
    OSP_DCHECK(dnssd_service_.get());
    task_runner_->PostTask(
        [this, info = std::move(service_info), has_been_seen]() mutable {
          CheckForPublishedService(std::move(info), has_been_seen, 0, true);
        });
  }

  void CheckNotPublishedService(ServiceInfo service_info,
                                std::atomic_bool* has_been_seen) {
    OSP_DCHECK(dnssd_service_.get());
    task_runner_->PostTask(
        [this, info = std::move(service_info), has_been_seen]() mutable {
          CheckForPublishedService(std::move(info), has_been_seen, 0, false);
        });
  }
  TaskRunner* task_runner_;
  FailOnErrorReporting reporting_client_;
  SerialDeletePtr<discovery::DnsSdService> dnssd_service_;
  std::unique_ptr<Receiver> receiver_;
  std::unique_ptr<Publisher> publisher_;

 private:
  void CheckForClaimedIds(ServiceInfo service_info,
                          std::atomic_bool* has_been_seen,
                          int attempts) {
    if (publisher_->IsInstanceIdClaimed(service_info.GetInstanceId())) {
      // TODO(crbug.com/openscreen/110): Log the published service instance.
      *has_been_seen = true;
      return;
    }

    if (attempts++ > kMaxCheckLoopIterations) {
      OSP_NOTREACHED() << "Service " << service_info.friendly_name
                       << " publication failed.";
    }
    task_runner_->PostTaskWithDelay(
        [this, info = std::move(service_info), has_been_seen,
         attempts]() mutable {
          CheckForClaimedIds(std::move(info), has_been_seen, attempts);
        },
        kCheckLoopSleepTime);
  }

  void CheckForPublishedService(ServiceInfo service_info,
                                std::atomic_bool* has_been_seen,
                                int attempts,
                                bool expect_to_be_present) {
    if (!receiver_->IsServiceFound(service_info)) {
      if (attempts++ > kMaxCheckLoopIterations) {
        OSP_CHECK(!expect_to_be_present)
            << "Service " << service_info.friendly_name << " discovery failed.";
        return;
      }
      task_runner_->PostTaskWithDelay(
          [this, info = std::move(service_info), has_been_seen, attempts,
           expect_to_be_present]() mutable {
            CheckForPublishedService(std::move(info), has_been_seen, attempts,
                                     expect_to_be_present);
          },
          kCheckLoopSleepTime);
    } else if (expect_to_be_present) {
      // TODO(crbug.com/openscreen/110): Log the discovered service instance.
      *has_been_seen = true;
    } else {
      OSP_NOTREACHED() << "Found instance '" << service_info.friendly_name
                       << "'!";
    }
  }
};

// The below runs an E2E tests. These test requires no user interaction and is
// intended to perform a set series of actions to validate that discovery is
// functioning as intended.
//
// Known issues:
// - The ipv6 socket in discovery/mdns/service_impl.cc fails to bind to an ipv6
//   address on the loopback interface. Investigating this issue is pending
//   resolution of bug
//   https://bugs.chromium.org/p/openscreen/issues/detail?id=105.
//
// In this test, the following operations are performed:
// 1) Start up the Cast platform for a posix system.
// 2) Publish 3 CastV2 service instances to the loopback interface using mDNS,
//    with record announcement disabled.
// 3) Wait for the probing phase to successfully complete.
// 4) Query for records published over the loopback interface, and validate that
//    all 3 previously published services are discovered.
TEST_F(DiscoveryE2ETest, ValidateQueryFlow) {
  // Set up demo infra.
  auto discovery_config = GetConfigSettings();
  discovery_config.new_record_announcement_count = 0;
  SetUpService(discovery_config);

  auto instance1 = GetInfo(1);
  auto instance2 = GetInfo(2);
  auto instance3 = GetInfo(3);

  // Start discovery and publication.
  StartDiscovery();
  PublishRecords(instance1, instance2, instance3);

  // Wait until all probe phases complete and all instance ids are claimed. At
  // this point, all records should be published.
  OSP_LOG << "Service publication in progress...";
  std::atomic_bool found1{false};
  std::atomic_bool found2{false};
  std::atomic_bool found3{false};
  CheckForClaimedIds(instance1, &found1);
  CheckForClaimedIds(instance2, &found2);
  CheckForClaimedIds(instance3, &found3);
  WaitUntilSeen(true, &found1, &found2, &found3);
  OSP_LOG << "\tAll services successfully published!\n";

  // Make sure all services are found through discovery.
  OSP_LOG << "Service discovery in progress...";
  found1 = false;
  found2 = false;
  found3 = false;
  CheckForPublishedService(instance1, &found1);
  CheckForPublishedService(instance2, &found2);
  CheckForPublishedService(instance3, &found3);
  WaitUntilSeen(true, &found1, &found2, &found3);
}

// In this test, the following operations are performed:
// 1) Start up the Cast platform for a posix system.
// 2) Start service discovery and new queries, with no query messages being
//    sent.
// 3) Publish 3 CastV2 service instances to the loopback interface using mDNS,
//    with record announcement enabled.
// 4) Ensure the correct records were published over the loopback interface.
// 5) De-register all services.
// 6) Ensure that goodbye records are received for all service instances.
TEST_F(DiscoveryE2ETest, ValidateAnnouncementFlow) {
  // Set up demo infra.
  auto discovery_config = GetConfigSettings();
  discovery_config.new_query_announcement_count = 0;
  SetUpService(discovery_config);

  auto instance1 = GetInfo(1);
  auto instance2 = GetInfo(2);
  auto instance3 = GetInfo(3);

  // Start discovery and publication.
  StartDiscovery();
  PublishRecords(instance1, instance2, instance3);

  // Wait until all probe phases complete and all instance ids are claimed. At
  // this point, all records should be published.
  OSP_LOG << "Service publication in progress...";
  std::atomic_bool found1{false};
  std::atomic_bool found2{false};
  std::atomic_bool found3{false};
  CheckForClaimedIds(instance1, &found1);
  CheckForClaimedIds(instance2, &found2);
  CheckForClaimedIds(instance3, &found3);
  WaitUntilSeen(true, &found1, &found2, &found3);
  OSP_LOG << "\tAll services successfully published and announced!\n";

  // Make sure all services are found through discovery.
  OSP_LOG << "Service discovery in progress...";
  found1 = false;
  found2 = false;
  found3 = false;
  CheckForPublishedService(instance1, &found1);
  CheckForPublishedService(instance2, &found2);
  CheckForPublishedService(instance3, &found3);
  WaitUntilSeen(true, &found1, &found2, &found3);
  OSP_LOG << "\tAll services successfully discovered!\n";

  // Deregister all service instances.
  OSP_LOG << "Deregister all services...";
  task_runner_->PostTask([this]() {
    ErrorOr<int> result = publisher_->DeregisterAll();
    ASSERT_FALSE(result.is_error());
    ASSERT_EQ(result.value(), 3);
  });
  std::this_thread::sleep_for(std::chrono::seconds(3));
  found1 = false;
  found2 = false;
  found3 = false;
  CheckNotPublishedService(instance1, &found1);
  CheckNotPublishedService(instance2, &found2);
  CheckNotPublishedService(instance3, &found3);
  WaitUntilSeen(false, &found1, &found2, &found3);
}

// In this test, the following operations are performed:
// 1) Start up the Cast platform for a posix system.
// 2) Publish one service and ensure it is NOT received.
// 3) Start service discovery and new queries.
// 4) Ensure above published service IS received.
// 5) Stop the started query.
// 6) Update a service, and ensure that no callback is received.
// 7) Restart the query and ensure that only the expected callbacks are
// received.
TEST_F(DiscoveryE2ETest, ValidateRecordsOnlyReceivedWhenQueryRunning) {
  // Set up demo infra.
  auto discovery_config = GetConfigSettings();
  discovery_config.new_record_announcement_count = 1;
  SetUpService(discovery_config);

  auto instance = GetInfo(1);

  // Start discovery and publication.
  PublishRecords(instance);

  // Wait until all probe phases complete and all instance ids are claimed. At
  // this point, all records should be published.
  OSP_LOG << "Service publication in progress...";
  std::atomic_bool found{false};
  CheckForClaimedIds(instance, &found);
  WaitUntilSeen(true, &found);

  // And ensure stopped discovery does not find the records.
  OSP_LOG << "Validating no service discovery occurs when discovery stopped...";
  found = false;
  CheckNotPublishedService(instance, &found);
  WaitUntilSeen(false, &found);

  // Make sure all services are found through discovery.
  StartDiscovery();
  OSP_LOG << "Service discovery in progress...";
  found = false;
  CheckForPublishedService(instance, &found);
  WaitUntilSeen(true, &found);

  // Update discovery and ensure that the updated service is seen.
  OSP_LOG << "Updating service and waiting for discovery...";
  auto updated_instance = instance;
  updated_instance.friendly_name = "OtherName";
  found = false;
  UpdateRecords(updated_instance);
  CheckForPublishedService(updated_instance, &found);
  WaitUntilSeen(true, &found);

  // And ensure the old service has been removed.
  found = false;
  CheckNotPublishedService(instance, &found);
  WaitUntilSeen(false, &found);

  // Stop discovery.
  OSP_LOG << "Stopping discovery...";
  task_runner_->PostTask([this]() { receiver_->StopDiscovery(); });

  // Update discovery and ensure that the updated service is NOT seen.
  OSP_LOG << "Updating service and validating the change isn't received...";
  found = false;
  instance.friendly_name = "ThirdName";
  UpdateRecords(instance);
  CheckNotPublishedService(instance, &found);
  WaitUntilSeen(false, &found);

  // Restart discovery and ensure that only the updated record is returned.
  StartDiscovery();
  OSP_LOG << "Service discovery in progress...";
  found = false;
  CheckNotPublishedService(updated_instance, &found);
  WaitUntilSeen(false, &found);

  found = false;
  CheckForPublishedService(instance, &found);
  WaitUntilSeen(true, &found);
}

// In this test, the following operations are performed:
// 1) Start up the Cast platform for a posix system.
// 2) Start service discovery and new queries.
// 3) Publish one service and ensure it is received.
// 4) Hard reset discovery
// 5) Ensure the same service is discovered
// 6) Soft reset the service, and ensure that a callback is received.
TEST_F(DiscoveryE2ETest, ValidateRefreshFlow) {
  // Set up demo infra.
  // NOTE: This configuration assumes that packets cannot be lost over the
  // loopback interface.
  auto discovery_config = GetConfigSettings();
  discovery_config.new_record_announcement_count = 0;
  discovery_config.new_query_announcement_count = 2;
  constexpr std::chrono::seconds kMaxQueryDuration{3};
  SetUpService(discovery_config);

  auto instance = GetInfo(1);

  // Start discovery and publication.
  StartDiscovery();
  PublishRecords(instance);

  // Wait until all probe phases complete and all instance ids are claimed. At
  // this point, all records should be published.
  OSP_LOG << "Service publication in progress...";
  std::atomic_bool found{false};
  CheckForClaimedIds(instance, &found);
  WaitUntilSeen(true, &found);

  // Make sure all services are found through discovery.
  OSP_LOG << "Service discovery in progress...";
  found = false;
  CheckForPublishedService(instance, &found);
  WaitUntilSeen(true, &found);

  // Force refresh discovery, then ensure that the published service is
  // re-discovered.
  OSP_LOG << "Force refresh discovery...";
  task_runner_->PostTask([this]() { receiver_->EraseReceivedServices(); });
  std::this_thread::sleep_for(kMaxQueryDuration);
  found = false;
  CheckNotPublishedService(instance, &found);
  WaitUntilSeen(false, &found);
  task_runner_->PostTask([this]() { receiver_->ForceRefresh(); });

  OSP_LOG << "Ensure that the published service is re-discovered...";
  found = false;
  CheckForPublishedService(instance, &found);
  WaitUntilSeen(true, &found);

  // Soft refresh discovery, then ensure that the published service is NOT
  // re-discovered.
  OSP_LOG << "Call DiscoverNow on discovery...";
  task_runner_->PostTask([this]() { receiver_->EraseReceivedServices(); });
  std::this_thread::sleep_for(kMaxQueryDuration);
  found = false;
  CheckNotPublishedService(instance, &found);
  WaitUntilSeen(false, &found);
  task_runner_->PostTask([this]() { receiver_->DiscoverNow(); });

  OSP_LOG << "Ensure that the published service is re-discovered...";
  found = false;
  CheckForPublishedService(instance, &found);
  WaitUntilSeen(true, &found);
}

}  // namespace cast
}  // namespace openscreen
