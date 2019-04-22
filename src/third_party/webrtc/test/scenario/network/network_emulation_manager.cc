/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/scenario/network/network_emulation_manager.h"
#include "test/time_controller/real_time_controller.h"

#include <algorithm>
#include <memory>

#include "absl/memory/memory.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "rtc_base/fake_network.h"

namespace webrtc {
namespace test {
namespace {

constexpr int64_t kPacketProcessingIntervalMs = 1;
// uint32_t representation of 192.168.0.0 address
constexpr uint32_t kMinIPv4Address = 0xC0A80000;
// uint32_t representation of 192.168.255.255 address
constexpr uint32_t kMaxIPv4Address = 0xC0A8FFFF;

}  // namespace

NetworkEmulationManagerImpl::NetworkEmulationManagerImpl()
    : NetworkEmulationManagerImpl(GlobalRealTimeController()) {}

NetworkEmulationManagerImpl::NetworkEmulationManagerImpl(
    TimeController* time_controller)
    : clock_(time_controller->GetClock()),
      next_node_id_(1),
      next_ip4_address_(kMinIPv4Address),
      task_queue_(time_controller->GetTaskQueueFactory()->CreateTaskQueue(
          "NetworkEmulation",
          TaskQueueFactory::Priority::NORMAL)) {
  process_task_handle_ = RepeatingTaskHandle::Start(task_queue_.Get(), [this] {
    ProcessNetworkPackets();
    return TimeDelta::ms(kPacketProcessingIntervalMs);
  });
}

// TODO(srte): Ensure that any pending task that must be run for consistency
// (such as stats collection tasks) are not cancelled when the task queue is
// destroyed.
NetworkEmulationManagerImpl::~NetworkEmulationManagerImpl() = default;

EmulatedNetworkNode* NetworkEmulationManagerImpl::CreateEmulatedNode(
    std::unique_ptr<NetworkBehaviorInterface> network_behavior) {
  auto node = absl::make_unique<EmulatedNetworkNode>(
      clock_, &task_queue_, std::move(network_behavior));
  EmulatedNetworkNode* out = node.get();

  struct Closure {
    void operator()() { manager->network_nodes_.push_back(std::move(node)); }
    NetworkEmulationManagerImpl* manager;
    std::unique_ptr<EmulatedNetworkNode> node;
  };
  task_queue_.PostTask(Closure{this, std::move(node)});
  return out;
}

EmulatedEndpoint* NetworkEmulationManagerImpl::CreateEndpoint(
    EmulatedEndpointConfig config) {
  absl::optional<rtc::IPAddress> ip = config.ip;
  if (!ip) {
    switch (config.generated_ip_family) {
      case EmulatedEndpointConfig::IpAddressFamily::kIpv4:
        ip = GetNextIPv4Address();
        RTC_CHECK(ip) << "All auto generated IPv4 addresses exhausted";
        break;
      case EmulatedEndpointConfig::IpAddressFamily::kIpv6:
        ip = GetNextIPv4Address();
        RTC_CHECK(ip) << "All auto generated IPv6 addresses exhausted";
        ip = ip->AsIPv6Address();
        break;
    }
  }

  bool res = used_ip_addresses_.insert(*ip).second;
  RTC_CHECK(res) << "IP=" << ip->ToString() << " already in use";
  auto node = absl::make_unique<EmulatedEndpoint>(
      next_node_id_++, *ip, config.start_as_enabled, &task_queue_, clock_);
  EmulatedEndpoint* out = node.get();
  endpoints_.push_back(std::move(node));
  return out;
}

void NetworkEmulationManagerImpl::EnableEndpoint(EmulatedEndpoint* endpoint) {
  EmulatedNetworkManager* network_manager =
      endpoint_to_network_manager_[endpoint];
  RTC_CHECK(network_manager);
  network_manager->EnableEndpoint(endpoint);
}

void NetworkEmulationManagerImpl::DisableEndpoint(EmulatedEndpoint* endpoint) {
  EmulatedNetworkManager* network_manager =
      endpoint_to_network_manager_[endpoint];
  RTC_CHECK(network_manager);
  network_manager->DisableEndpoint(endpoint);
}

EmulatedRoute* NetworkEmulationManagerImpl::CreateRoute(
    EmulatedEndpoint* from,
    const std::vector<EmulatedNetworkNode*>& via_nodes,
    EmulatedEndpoint* to) {
  // Because endpoint has no send node by default at least one should be
  // provided here.
  RTC_CHECK(!via_nodes.empty());

  from->router()->SetReceiver(to->GetPeerLocalAddress(), via_nodes[0]);
  EmulatedNetworkNode* cur_node = via_nodes[0];
  for (size_t i = 1; i < via_nodes.size(); ++i) {
    cur_node->router()->SetReceiver(to->GetPeerLocalAddress(), via_nodes[i]);
    cur_node = via_nodes[i];
  }
  cur_node->router()->SetReceiver(to->GetPeerLocalAddress(), to);

  std::unique_ptr<EmulatedRoute> route =
      absl::make_unique<EmulatedRoute>(from, std::move(via_nodes), to);
  EmulatedRoute* out = route.get();
  routes_.push_back(std::move(route));
  return out;
}

void NetworkEmulationManagerImpl::ClearRoute(EmulatedRoute* route) {
  RTC_CHECK(route->active) << "Route already cleared";
  task_queue_.SendTask([route]() {
    // Remove receiver from intermediate nodes.
    for (auto* node : route->via_nodes) {
      node->router()->RemoveReceiver(route->to->GetPeerLocalAddress());
    }
    // Remove destination endpoint from source endpoint's router.
    route->from->router()->RemoveReceiver(route->to->GetPeerLocalAddress());

    route->active = false;
  });
}

TrafficRoute* NetworkEmulationManagerImpl::CreateTrafficRoute(
    const std::vector<EmulatedNetworkNode*>& via_nodes) {
  RTC_CHECK(!via_nodes.empty());
  EmulatedEndpoint* endpoint = CreateEndpoint(EmulatedEndpointConfig());

  // Setup a route via specified nodes.
  EmulatedNetworkNode* cur_node = via_nodes[0];
  for (size_t i = 1; i < via_nodes.size(); ++i) {
    cur_node->router()->SetReceiver(endpoint->GetPeerLocalAddress(),
                                    via_nodes[i]);
    cur_node = via_nodes[i];
  }
  cur_node->router()->SetReceiver(endpoint->GetPeerLocalAddress(), endpoint);

  std::unique_ptr<TrafficRoute> traffic_route =
      absl::make_unique<TrafficRoute>(clock_, via_nodes[0], endpoint);
  TrafficRoute* out = traffic_route.get();
  traffic_routes_.push_back(std::move(traffic_route));
  return out;
}

RandomWalkCrossTraffic*
NetworkEmulationManagerImpl::CreateRandomWalkCrossTraffic(
    TrafficRoute* traffic_route,
    RandomWalkConfig config) {
  auto traffic = absl::make_unique<RandomWalkCrossTraffic>(std::move(config),
                                                           traffic_route);
  RandomWalkCrossTraffic* out = traffic.get();
  struct Closure {
    void operator()() {
      manager->random_cross_traffics_.push_back(std::move(traffic));
    }
    NetworkEmulationManagerImpl* manager;
    std::unique_ptr<RandomWalkCrossTraffic> traffic;
  };
  task_queue_.PostTask(Closure{this, std::move(traffic)});
  return out;
}

PulsedPeaksCrossTraffic*
NetworkEmulationManagerImpl::CreatePulsedPeaksCrossTraffic(
    TrafficRoute* traffic_route,
    PulsedPeaksConfig config) {
  auto traffic = absl::make_unique<PulsedPeaksCrossTraffic>(std::move(config),
                                                            traffic_route);
  PulsedPeaksCrossTraffic* out = traffic.get();
  struct Closure {
    void operator()() {
      manager->pulsed_cross_traffics_.push_back(std::move(traffic));
    }
    NetworkEmulationManagerImpl* manager;
    std::unique_ptr<PulsedPeaksCrossTraffic> traffic;
  };
  task_queue_.PostTask(Closure{this, std::move(traffic)});
  return out;
}

EmulatedNetworkManagerInterface*
NetworkEmulationManagerImpl::CreateEmulatedNetworkManagerInterface(
    const std::vector<EmulatedEndpoint*>& endpoints) {
  auto endpoints_container = absl::make_unique<EndpointsContainer>(endpoints);
  auto network_manager = absl::make_unique<EmulatedNetworkManager>(
      clock_, &task_queue_, endpoints_container.get());
  for (auto* endpoint : endpoints) {
    // Associate endpoint with network manager.
    bool insertion_result =
        endpoint_to_network_manager_.insert({endpoint, network_manager.get()})
            .second;
    RTC_CHECK(insertion_result)
        << "Endpoint ip=" << endpoint->GetPeerLocalAddress().ToString()
        << " is already used for another network";
  }

  EmulatedNetworkManagerInterface* out = network_manager.get();

  endpoints_containers_.push_back(std::move(endpoints_container));
  network_managers_.push_back(std::move(network_manager));
  return out;
}

absl::optional<rtc::IPAddress>
NetworkEmulationManagerImpl::GetNextIPv4Address() {
  uint32_t addresses_count = kMaxIPv4Address - kMinIPv4Address;
  for (uint32_t i = 0; i < addresses_count; i++) {
    rtc::IPAddress ip(next_ip4_address_);
    if (next_ip4_address_ == kMaxIPv4Address) {
      next_ip4_address_ = kMinIPv4Address;
    } else {
      next_ip4_address_++;
    }
    if (used_ip_addresses_.find(ip) == used_ip_addresses_.end()) {
      return ip;
    }
  }
  return absl::nullopt;
}

void NetworkEmulationManagerImpl::ProcessNetworkPackets() {
  Timestamp current_time = Now();
  for (auto& traffic : random_cross_traffics_) {
    traffic->Process(current_time);
  }
  for (auto& traffic : pulsed_cross_traffics_) {
    traffic->Process(current_time);
  }
}

Timestamp NetworkEmulationManagerImpl::Now() const {
  return Timestamp::us(clock_->TimeInMicroseconds());
}

}  // namespace test
}  // namespace webrtc
