/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/network/network_emulation.h"

#include <algorithm>
#include <limits>
#include <memory>

#include "api/units/data_size.h"
#include "rtc_base/bind.h"
#include "rtc_base/logging.h"

namespace webrtc {

namespace {
constexpr size_t kIPv4HeaderSize = 20;
constexpr size_t kIPv6HeaderSize = 40;
}  // namespace

EmulatedIpPacket::EmulatedIpPacket(const rtc::SocketAddress& from,
                                   const rtc::SocketAddress& to,
                                   rtc::CopyOnWriteBuffer data,
                                   Timestamp arrival_time)
    : from(from),
      to(to),
      data(data),
      ip_header_size((to.family() == AF_INET) ? kIPv4HeaderSize
                                              : kIPv6HeaderSize),
      arrival_time(arrival_time) {
  RTC_DCHECK(to.family() == AF_INET || to.family() == AF_INET6);
}

void LinkEmulation::OnPacketReceived(EmulatedIpPacket packet) {
  task_queue_->PostTask([this, packet = std::move(packet)]() mutable {
    RTC_DCHECK_RUN_ON(task_queue_);

    uint64_t packet_id = next_packet_id_++;
    bool sent = network_behavior_->EnqueuePacket(PacketInFlightInfo(
        packet.ip_packet_size(), packet.arrival_time.us(), packet_id));
    if (sent) {
      packets_.emplace_back(StoredPacket{packet_id, std::move(packet), false});
    }
    if (process_task_.Running())
      return;
    absl::optional<int64_t> next_time_us =
        network_behavior_->NextDeliveryTimeUs();
    if (!next_time_us)
      return;
    Timestamp current_time = clock_->CurrentTime();
    process_task_ = RepeatingTaskHandle::DelayedStart(
        task_queue_->Get(),
        std::max(TimeDelta::Zero(),
                 Timestamp::us(*next_time_us) - current_time),
        [this]() {
          RTC_DCHECK_RUN_ON(task_queue_);
          Timestamp current_time = clock_->CurrentTime();
          Process(current_time);
          absl::optional<int64_t> next_time_us =
              network_behavior_->NextDeliveryTimeUs();
          if (!next_time_us) {
            process_task_.Stop();
            return TimeDelta::Zero();  // This is ignored.
          }
          RTC_DCHECK_GE(*next_time_us, current_time.us());
          return Timestamp::us(*next_time_us) - current_time;
        });
  });
}

void LinkEmulation::Process(Timestamp at_time) {
  std::vector<PacketDeliveryInfo> delivery_infos =
      network_behavior_->DequeueDeliverablePackets(at_time.us());
  for (PacketDeliveryInfo& delivery_info : delivery_infos) {
    StoredPacket* packet = nullptr;
    for (auto& stored_packet : packets_) {
      if (stored_packet.id == delivery_info.packet_id) {
        packet = &stored_packet;
        break;
      }
    }
    RTC_CHECK(packet);
    RTC_DCHECK(!packet->removed);
    packet->removed = true;

    if (delivery_info.receive_time_us != PacketDeliveryInfo::kNotReceived) {
      packet->packet.arrival_time =
          Timestamp::us(delivery_info.receive_time_us);
      receiver_->OnPacketReceived(std::move(packet->packet));
    }
    while (!packets_.empty() && packets_.front().removed) {
      packets_.pop_front();
    }
  }
}

NetworkRouterNode::NetworkRouterNode(rtc::TaskQueue* task_queue)
    : task_queue_(task_queue) {}

void NetworkRouterNode::OnPacketReceived(EmulatedIpPacket packet) {
  RTC_DCHECK_RUN_ON(task_queue_);
  if (watcher_) {
    watcher_(packet);
  }
  auto receiver_it = routing_.find(packet.to.ipaddr());
  if (receiver_it == routing_.end()) {
    return;
  }
  RTC_CHECK(receiver_it != routing_.end());

  receiver_it->second->OnPacketReceived(std::move(packet));
}

void NetworkRouterNode::SetReceiver(
    rtc::IPAddress dest_ip,
    EmulatedNetworkReceiverInterface* receiver) {
  task_queue_->PostTask([=] {
    RTC_DCHECK_RUN_ON(task_queue_);
    EmulatedNetworkReceiverInterface* cur_receiver = routing_[dest_ip];
    RTC_CHECK(cur_receiver == nullptr || cur_receiver == receiver)
        << "Routing for dest_ip=" << dest_ip.ToString() << " already exists";
    routing_[dest_ip] = receiver;
  });
}

void NetworkRouterNode::RemoveReceiver(rtc::IPAddress dest_ip) {
  RTC_DCHECK_RUN_ON(task_queue_);
  routing_.erase(dest_ip);
}

void NetworkRouterNode::SetWatcher(
    std::function<void(const EmulatedIpPacket&)> watcher) {
  task_queue_->PostTask([=] {
    RTC_DCHECK_RUN_ON(task_queue_);
    watcher_ = watcher;
  });
}

EmulatedNetworkNode::EmulatedNetworkNode(
    Clock* clock,
    rtc::TaskQueue* task_queue,
    std::unique_ptr<NetworkBehaviorInterface> network_behavior)
    : router_(task_queue),
      link_(clock, task_queue, std::move(network_behavior), &router_) {}

void EmulatedNetworkNode::OnPacketReceived(EmulatedIpPacket packet) {
  link_.OnPacketReceived(std::move(packet));
}

void EmulatedNetworkNode::CreateRoute(
    rtc::IPAddress receiver_ip,
    std::vector<EmulatedNetworkNode*> nodes,
    EmulatedNetworkReceiverInterface* receiver) {
  RTC_CHECK(!nodes.empty());
  for (size_t i = 0; i + 1 < nodes.size(); ++i)
    nodes[i]->router()->SetReceiver(receiver_ip, nodes[i + 1]);
  nodes.back()->router()->SetReceiver(receiver_ip, receiver);
}

void EmulatedNetworkNode::ClearRoute(rtc::IPAddress receiver_ip,
                                     std::vector<EmulatedNetworkNode*> nodes) {
  for (EmulatedNetworkNode* node : nodes)
    node->router()->RemoveReceiver(receiver_ip);
}

EmulatedNetworkNode::~EmulatedNetworkNode() = default;

EmulatedEndpoint::EmulatedEndpoint(uint64_t id,
                                   const rtc::IPAddress& ip,
                                   bool is_enabled,
                                   rtc::TaskQueue* task_queue,
                                   Clock* clock)
    : id_(id),
      peer_local_addr_(ip),
      is_enabled_(is_enabled),
      clock_(clock),
      task_queue_(task_queue),
      router_(task_queue_),
      next_port_(kFirstEphemeralPort) {
  constexpr int kIPv4NetworkPrefixLength = 24;
  constexpr int kIPv6NetworkPrefixLength = 64;

  int prefix_length = 0;
  if (ip.family() == AF_INET) {
    prefix_length = kIPv4NetworkPrefixLength;
  } else if (ip.family() == AF_INET6) {
    prefix_length = kIPv6NetworkPrefixLength;
  }
  rtc::IPAddress prefix = TruncateIP(ip, prefix_length);
  network_ = std::make_unique<rtc::Network>(
      ip.ToString(), "Endpoint id=" + std::to_string(id_), prefix,
      prefix_length, rtc::AdapterType::ADAPTER_TYPE_UNKNOWN);
  network_->AddIP(ip);

  enabled_state_checker_.Detach();
}
EmulatedEndpoint::~EmulatedEndpoint() = default;

uint64_t EmulatedEndpoint::GetId() const {
  return id_;
}

void EmulatedEndpoint::SendPacket(const rtc::SocketAddress& from,
                                  const rtc::SocketAddress& to,
                                  rtc::CopyOnWriteBuffer packet_data) {
  RTC_CHECK(from.ipaddr() == peer_local_addr_);
  EmulatedIpPacket packet(from, to, std::move(packet_data),
                          clock_->CurrentTime());
  task_queue_->PostTask([this, packet = std::move(packet)]() mutable {
    RTC_DCHECK_RUN_ON(task_queue_);
    Timestamp current_time = clock_->CurrentTime();
    if (stats_.first_packet_sent_time.IsInfinite()) {
      stats_.first_packet_sent_time = current_time;
      stats_.first_sent_packet_size = DataSize::bytes(packet.ip_packet_size());
    }
    stats_.last_packet_sent_time = current_time;
    stats_.packets_sent++;
    stats_.bytes_sent += DataSize::bytes(packet.ip_packet_size());

    router_.OnPacketReceived(std::move(packet));
  });
}

absl::optional<uint16_t> EmulatedEndpoint::BindReceiver(
    uint16_t desired_port,
    EmulatedNetworkReceiverInterface* receiver) {
  rtc::CritScope crit(&receiver_lock_);
  uint16_t port = desired_port;
  if (port == 0) {
    // Because client can specify its own port, next_port_ can be already in
    // use, so we need to find next available port.
    int ports_pool_size =
        std::numeric_limits<uint16_t>::max() - kFirstEphemeralPort + 1;
    for (int i = 0; i < ports_pool_size; ++i) {
      uint16_t next_port = NextPort();
      if (port_to_receiver_.find(next_port) == port_to_receiver_.end()) {
        port = next_port;
        break;
      }
    }
  }
  RTC_CHECK(port != 0) << "Can't find free port for receiver in endpoint "
                       << id_;
  bool result = port_to_receiver_.insert({port, receiver}).second;
  if (!result) {
    RTC_LOG(INFO) << "Can't bind receiver to used port " << desired_port
                  << " in endpoint " << id_;
    return absl::nullopt;
  }
  RTC_LOG(INFO) << "New receiver is binded to endpoint " << id_ << " on port "
                << port;
  return port;
}

uint16_t EmulatedEndpoint::NextPort() {
  uint16_t out = next_port_;
  if (next_port_ == std::numeric_limits<uint16_t>::max()) {
    next_port_ = kFirstEphemeralPort;
  } else {
    next_port_++;
  }
  return out;
}

void EmulatedEndpoint::UnbindReceiver(uint16_t port) {
  rtc::CritScope crit(&receiver_lock_);
  port_to_receiver_.erase(port);
}

rtc::IPAddress EmulatedEndpoint::GetPeerLocalAddress() const {
  return peer_local_addr_;
}

void EmulatedEndpoint::OnPacketReceived(EmulatedIpPacket packet) {
  RTC_DCHECK_RUN_ON(task_queue_);
  RTC_CHECK(packet.to.ipaddr() == peer_local_addr_)
      << "Routing error: wrong destination endpoint. Packet.to.ipaddr()=: "
      << packet.to.ipaddr().ToString()
      << "; Receiver peer_local_addr_=" << peer_local_addr_.ToString();
  rtc::CritScope crit(&receiver_lock_);
  UpdateReceiveStats(packet);
  auto it = port_to_receiver_.find(packet.to.port());
  if (it == port_to_receiver_.end()) {
    // It can happen, that remote peer closed connection, but there still some
    // packets, that are going to it. It can happen during peer connection close
    // process: one peer closed connection, second still sending data.
    RTC_LOG(INFO) << "Drop packet: no receiver registered in " << id_
                  << " on port " << packet.to.port();
    stats_.packets_dropped++;
    stats_.bytes_dropped += DataSize::bytes(packet.ip_packet_size());
    return;
  }
  // Endpoint assumes frequent calls to bind and unbind methods, so it holds
  // lock during packet processing to ensure that receiver won't be deleted
  // before call to OnPacketReceived.
  it->second->OnPacketReceived(std::move(packet));
}

void EmulatedEndpoint::Enable() {
  RTC_DCHECK_RUN_ON(&enabled_state_checker_);
  RTC_CHECK(!is_enabled_);
  is_enabled_ = true;
}

void EmulatedEndpoint::Disable() {
  RTC_DCHECK_RUN_ON(&enabled_state_checker_);
  RTC_CHECK(is_enabled_);
  is_enabled_ = false;
}

bool EmulatedEndpoint::Enabled() const {
  RTC_DCHECK_RUN_ON(&enabled_state_checker_);
  return is_enabled_;
}

EmulatedNetworkStats EmulatedEndpoint::stats() {
  RTC_DCHECK_RUN_ON(task_queue_);
  return stats_;
}

void EmulatedEndpoint::UpdateReceiveStats(const EmulatedIpPacket& packet) {
  RTC_DCHECK_RUN_ON(task_queue_);
  Timestamp current_time = clock_->CurrentTime();
  if (stats_.first_packet_received_time.IsInfinite()) {
    stats_.first_packet_received_time = current_time;
    stats_.first_received_packet_size =
        DataSize::bytes(packet.ip_packet_size());
  }
  stats_.last_packet_received_time = current_time;
  stats_.packets_received++;
  stats_.bytes_received += DataSize::bytes(packet.ip_packet_size());
}

EndpointsContainer::EndpointsContainer(
    const std::vector<EmulatedEndpoint*>& endpoints)
    : endpoints_(endpoints) {}

EmulatedEndpoint* EndpointsContainer::LookupByLocalAddress(
    const rtc::IPAddress& local_ip) const {
  for (auto* endpoint : endpoints_) {
    rtc::IPAddress peer_local_address = endpoint->GetPeerLocalAddress();
    if (peer_local_address == local_ip) {
      return endpoint;
    }
  }
  RTC_CHECK(false) << "No network found for address" << local_ip.ToString();
}

bool EndpointsContainer::HasEndpoint(EmulatedEndpoint* endpoint) const {
  for (auto* e : endpoints_) {
    if (e->GetId() == endpoint->GetId()) {
      return true;
    }
  }
  return false;
}

std::vector<std::unique_ptr<rtc::Network>>
EndpointsContainer::GetEnabledNetworks() const {
  std::vector<std::unique_ptr<rtc::Network>> networks;
  for (auto* endpoint : endpoints_) {
    if (endpoint->Enabled()) {
      networks.emplace_back(
          std::make_unique<rtc::Network>(endpoint->network()));
    }
  }
  return networks;
}

EmulatedNetworkStats EndpointsContainer::GetStats() const {
  EmulatedNetworkStats stats;
  for (auto* endpoint : endpoints_) {
    EmulatedNetworkStats endpoint_stats = endpoint->stats();
    stats.packets_sent += endpoint_stats.packets_sent;
    stats.bytes_sent += endpoint_stats.bytes_sent;
    stats.packets_received += endpoint_stats.packets_received;
    stats.bytes_received += endpoint_stats.bytes_received;
    stats.packets_dropped += endpoint_stats.packets_dropped;
    stats.bytes_dropped += endpoint_stats.bytes_dropped;
    if (stats.first_packet_received_time >
        endpoint_stats.first_packet_received_time) {
      stats.first_packet_received_time =
          endpoint_stats.first_packet_received_time;
      stats.first_received_packet_size =
          endpoint_stats.first_received_packet_size;
    }
    if (stats.first_packet_sent_time > endpoint_stats.first_packet_sent_time) {
      stats.first_packet_sent_time = endpoint_stats.first_packet_sent_time;
      stats.first_sent_packet_size = endpoint_stats.first_sent_packet_size;
    }
    if (stats.last_packet_received_time.IsInfinite() ||
        stats.last_packet_received_time <
            endpoint_stats.last_packet_received_time) {
      stats.last_packet_received_time =
          endpoint_stats.last_packet_received_time;
    }
    if (stats.last_packet_sent_time.IsInfinite() ||
        stats.last_packet_sent_time < endpoint_stats.last_packet_sent_time) {
      stats.last_packet_sent_time = endpoint_stats.last_packet_sent_time;
    }
  }
  return stats;
}

}  // namespace webrtc
