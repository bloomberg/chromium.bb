/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <atomic>
#include <memory>

#include "absl/memory/memory.h"
#include "api/test/simulated_network.h"
#include "call/simulated_network.h"
#include "rtc_base/event.h"
#include "rtc_base/gunit.h"
#include "rtc_base/logging.h"
#include "system_wrappers/include/sleep.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/scenario/network/network_emulation.h"
#include "test/scenario/network/network_emulation_manager.h"

namespace webrtc {
namespace test {
namespace {

constexpr int kNetworkPacketWaitTimeoutMs = 100;
constexpr int kStatsWaitTimeoutMs = 1000;

class SocketReader : public sigslot::has_slots<> {
 public:
  explicit SocketReader(rtc::AsyncSocket* socket) : socket_(socket) {
    socket_->SignalReadEvent.connect(this, &SocketReader::OnReadEvent);
    size_ = 128 * 1024;
    buf_ = new char[size_];
  }
  ~SocketReader() override { delete[] buf_; }

  void OnReadEvent(rtc::AsyncSocket* socket) {
    RTC_DCHECK(socket_ == socket);
    int64_t timestamp;
    len_ = socket_->Recv(buf_, size_, &timestamp);
    {
      rtc::CritScope crit(&lock_);
      received_count_++;
    }
  }

  int ReceivedCount() {
    rtc::CritScope crit(&lock_);
    return received_count_;
  }

 private:
  rtc::AsyncSocket* socket_;
  char* buf_;
  size_t size_;
  int len_;

  rtc::CriticalSection lock_;
  int received_count_ RTC_GUARDED_BY(lock_) = 0;
};

class MockReceiver : public EmulatedNetworkReceiverInterface {
 public:
  MOCK_METHOD1(OnPacketReceived, void(EmulatedIpPacket packet));
};

class NetworkEmulationManagerThreeNodesRoutingTest : public ::testing::Test {
 public:
  NetworkEmulationManagerThreeNodesRoutingTest() {
    e1_ = emulation_.CreateEndpoint(EmulatedEndpointConfig());
    e2_ = emulation_.CreateEndpoint(EmulatedEndpointConfig());
    e3_ = emulation_.CreateEndpoint(EmulatedEndpointConfig());
  }

  void SetupRouting(
      std::function<void(EmulatedEndpoint*,
                         EmulatedEndpoint*,
                         EmulatedEndpoint*,
                         NetworkEmulationManager*)> create_routing_func) {
    create_routing_func(e1_, e2_, e3_, &emulation_);
  }

  void SendPacketsAndValidateDelivery() {
    EXPECT_CALL(r_e1_e2_, OnPacketReceived(::testing::_)).Times(1);
    EXPECT_CALL(r_e2_e1_, OnPacketReceived(::testing::_)).Times(1);
    EXPECT_CALL(r_e1_e3_, OnPacketReceived(::testing::_)).Times(1);
    EXPECT_CALL(r_e3_e1_, OnPacketReceived(::testing::_)).Times(1);

    uint16_t common_send_port = 80;
    uint16_t r_e1_e2_port = e2_->BindReceiver(0, &r_e1_e2_).value();
    uint16_t r_e2_e1_port = e1_->BindReceiver(0, &r_e2_e1_).value();
    uint16_t r_e1_e3_port = e3_->BindReceiver(0, &r_e1_e3_).value();
    uint16_t r_e3_e1_port = e1_->BindReceiver(0, &r_e3_e1_).value();

    // Next code is using API of EmulatedEndpoint, that is visible only for
    // internals of network emulation layer. Don't use this API in other tests.
    // Send packet from e1 to e2.
    e1_->SendPacket(
        rtc::SocketAddress(e1_->GetPeerLocalAddress(), common_send_port),
        rtc::SocketAddress(e2_->GetPeerLocalAddress(), r_e1_e2_port),
        rtc::CopyOnWriteBuffer(10));

    // Send packet from e2 to e1.
    e2_->SendPacket(
        rtc::SocketAddress(e2_->GetPeerLocalAddress(), common_send_port),
        rtc::SocketAddress(e1_->GetPeerLocalAddress(), r_e2_e1_port),
        rtc::CopyOnWriteBuffer(10));

    // Send packet from e1 to e3.
    e1_->SendPacket(
        rtc::SocketAddress(e1_->GetPeerLocalAddress(), common_send_port),
        rtc::SocketAddress(e3_->GetPeerLocalAddress(), r_e1_e3_port),
        rtc::CopyOnWriteBuffer(10));

    // Send packet from e3 to e1.
    e3_->SendPacket(
        rtc::SocketAddress(e3_->GetPeerLocalAddress(), common_send_port),
        rtc::SocketAddress(e1_->GetPeerLocalAddress(), r_e3_e1_port),
        rtc::CopyOnWriteBuffer(10));

    // Sleep at the end to wait for async packets delivery.
    SleepMs(kNetworkPacketWaitTimeoutMs);
  }

 private:
  // Receivers: r_<source endpoint>_<destination endpoint>
  // They must be destroyed after emulation, so they should be declared before.
  MockReceiver r_e1_e2_;
  MockReceiver r_e2_e1_;
  MockReceiver r_e1_e3_;
  MockReceiver r_e3_e1_;

  NetworkEmulationManagerImpl emulation_;
  EmulatedEndpoint* e1_;
  EmulatedEndpoint* e2_;
  EmulatedEndpoint* e3_;
};

EmulatedNetworkNode* CreateEmulatedNodeWithDefaultBuiltInConfig(
    NetworkEmulationManager* emulation) {
  return emulation->CreateEmulatedNode(
      absl::make_unique<SimulatedNetwork>(BuiltInNetworkBehaviorConfig()));
}

}  // namespace

using ::testing::_;

TEST(NetworkEmulationManagerTest, GeneratedIpv4AddressDoesNotCollide) {
  NetworkEmulationManagerImpl network_manager;
  std::set<rtc::IPAddress> ips;
  EmulatedEndpointConfig config;
  config.generated_ip_family = EmulatedEndpointConfig::IpAddressFamily::kIpv4;
  for (int i = 0; i < 1000; i++) {
    EmulatedEndpoint* endpoint = network_manager.CreateEndpoint(config);
    ASSERT_EQ(endpoint->GetPeerLocalAddress().family(), AF_INET);
    bool result = ips.insert(endpoint->GetPeerLocalAddress()).second;
    ASSERT_TRUE(result);
  }
}

TEST(NetworkEmulationManagerTest, GeneratedIpv6AddressDoesNotCollide) {
  NetworkEmulationManagerImpl network_manager;
  std::set<rtc::IPAddress> ips;
  EmulatedEndpointConfig config;
  config.generated_ip_family = EmulatedEndpointConfig::IpAddressFamily::kIpv6;
  for (int i = 0; i < 1000; i++) {
    EmulatedEndpoint* endpoint = network_manager.CreateEndpoint(config);
    ASSERT_EQ(endpoint->GetPeerLocalAddress().family(), AF_INET6);
    bool result = ips.insert(endpoint->GetPeerLocalAddress()).second;
    ASSERT_TRUE(result);
  }
}

TEST(NetworkEmulationManagerTest, Run) {
  NetworkEmulationManagerImpl network_manager;

  EmulatedNetworkNode* alice_node = network_manager.CreateEmulatedNode(
      absl::make_unique<SimulatedNetwork>(BuiltInNetworkBehaviorConfig()));
  EmulatedNetworkNode* bob_node = network_manager.CreateEmulatedNode(
      absl::make_unique<SimulatedNetwork>(BuiltInNetworkBehaviorConfig()));
  EmulatedEndpoint* alice_endpoint =
      network_manager.CreateEndpoint(EmulatedEndpointConfig());
  EmulatedEndpoint* bob_endpoint =
      network_manager.CreateEndpoint(EmulatedEndpointConfig());
  network_manager.CreateRoute(alice_endpoint, {alice_node}, bob_endpoint);
  network_manager.CreateRoute(bob_endpoint, {bob_node}, alice_endpoint);

  EmulatedNetworkManagerInterface* nt1 =
      network_manager.CreateEmulatedNetworkManagerInterface({alice_endpoint});
  EmulatedNetworkManagerInterface* nt2 =
      network_manager.CreateEmulatedNetworkManagerInterface({bob_endpoint});

  rtc::CopyOnWriteBuffer data("Hello");
  for (uint64_t j = 0; j < 2; j++) {
    auto* s1 = nt1->network_thread()->socketserver()->CreateAsyncSocket(
        AF_INET, SOCK_DGRAM);
    auto* s2 = nt2->network_thread()->socketserver()->CreateAsyncSocket(
        AF_INET, SOCK_DGRAM);

    SocketReader r1(s1);
    SocketReader r2(s2);

    rtc::SocketAddress a1(alice_endpoint->GetPeerLocalAddress(), 0);
    rtc::SocketAddress a2(bob_endpoint->GetPeerLocalAddress(), 0);

    s1->Bind(a1);
    s2->Bind(a2);

    s1->Connect(s2->GetLocalAddress());
    s2->Connect(s1->GetLocalAddress());

    for (uint64_t i = 0; i < 1000; i++) {
      s1->Send(data.data(), data.size());
      s2->Send(data.data(), data.size());
    }

    rtc::Event wait;
    wait.Wait(1000);
    EXPECT_EQ(r1.ReceivedCount(), 1000);
    EXPECT_EQ(r2.ReceivedCount(), 1000);

    delete s1;
    delete s2;
  }

  int64_t single_packet_size = data.size();
  std::atomic<int> received_stats_count{0};
  nt1->GetStats([&](EmulatedNetworkStats st) {
    EXPECT_EQ(st.packets_sent, 2000l);
    EXPECT_EQ(st.bytes_sent.bytes(), single_packet_size * 2000l);
    EXPECT_EQ(st.packets_received, 2000l);
    EXPECT_EQ(st.bytes_received.bytes(), single_packet_size * 2000l);
    EXPECT_EQ(st.packets_dropped, 0l);
    EXPECT_EQ(st.bytes_dropped.bytes(), 0l);
    received_stats_count++;
  });
  nt2->GetStats([&](EmulatedNetworkStats st) {
    EXPECT_EQ(st.packets_sent, 2000l);
    EXPECT_EQ(st.bytes_sent.bytes(), single_packet_size * 2000l);
    EXPECT_EQ(st.packets_received, 2000l);
    EXPECT_EQ(st.bytes_received.bytes(), single_packet_size * 2000l);
    EXPECT_EQ(st.packets_dropped, 0l);
    EXPECT_EQ(st.bytes_dropped.bytes(), 0l);
    received_stats_count++;
  });
  ASSERT_EQ_WAIT(received_stats_count.load(), 2, kStatsWaitTimeoutMs);
}

TEST(NetworkEmulationManagerTest, ThoughputStats) {
  NetworkEmulationManagerImpl network_manager;

  EmulatedNetworkNode* alice_node = network_manager.CreateEmulatedNode(
      absl::make_unique<SimulatedNetwork>(BuiltInNetworkBehaviorConfig()));
  EmulatedNetworkNode* bob_node = network_manager.CreateEmulatedNode(
      absl::make_unique<SimulatedNetwork>(BuiltInNetworkBehaviorConfig()));
  EmulatedEndpoint* alice_endpoint =
      network_manager.CreateEndpoint(EmulatedEndpointConfig());
  EmulatedEndpoint* bob_endpoint =
      network_manager.CreateEndpoint(EmulatedEndpointConfig());
  network_manager.CreateRoute(alice_endpoint, {alice_node}, bob_endpoint);
  network_manager.CreateRoute(bob_endpoint, {bob_node}, alice_endpoint);

  EmulatedNetworkManagerInterface* nt1 =
      network_manager.CreateEmulatedNetworkManagerInterface({alice_endpoint});
  EmulatedNetworkManagerInterface* nt2 =
      network_manager.CreateEmulatedNetworkManagerInterface({bob_endpoint});

  int64_t single_packet_size = 100;
  rtc::CopyOnWriteBuffer data(single_packet_size);
  auto* s1 = nt1->network_thread()->socketserver()->CreateAsyncSocket(
      AF_INET, SOCK_DGRAM);
  auto* s2 = nt2->network_thread()->socketserver()->CreateAsyncSocket(
      AF_INET, SOCK_DGRAM);

  SocketReader r1(s1);
  SocketReader r2(s2);

  rtc::SocketAddress a1(alice_endpoint->GetPeerLocalAddress(), 0);
  rtc::SocketAddress a2(bob_endpoint->GetPeerLocalAddress(), 0);

  s1->Bind(a1);
  s2->Bind(a2);

  s1->Connect(s2->GetLocalAddress());
  s2->Connect(s1->GetLocalAddress());

  // Send 10 packets for 1
  rtc::Event wait;
  for (uint64_t i = 0; i < 11; i++) {
    s1->Send(data.data(), data.size());
    s2->Send(data.data(), data.size());
    wait.Wait(100);
  }
  EXPECT_EQ(r1.ReceivedCount(), 11);
  EXPECT_EQ(r2.ReceivedCount(), 11);

  delete s1;
  delete s2;

  std::atomic<int> received_stats_count{0};
  nt1->GetStats([&](EmulatedNetworkStats st) {
    EXPECT_EQ(st.packets_sent, 11l);
    EXPECT_EQ(st.bytes_sent.bytes(), single_packet_size * 11l);
    EXPECT_NEAR(st.AverageSendRate().bps(), DataRate::bytes_per_sec(1000).bps(),
                1000);
    received_stats_count++;
  });
  ASSERT_EQ_WAIT(received_stats_count.load(), 1, kStatsWaitTimeoutMs);
}

// Testing that packets are delivered via all routes using a routing scheme as
// follows:
//  * e1 -> n1 -> e2
//  * e2 -> n2 -> e1
//  * e1 -> n3 -> e3
//  * e3 -> n4 -> e1
TEST_F(NetworkEmulationManagerThreeNodesRoutingTest,
       PacketsAreDeliveredInBothWaysWhenConnectedToTwoPeers) {
  SetupRouting([](EmulatedEndpoint* e1, EmulatedEndpoint* e2,
                  EmulatedEndpoint* e3, NetworkEmulationManager* emulation) {
    auto* node1 = CreateEmulatedNodeWithDefaultBuiltInConfig(emulation);
    auto* node2 = CreateEmulatedNodeWithDefaultBuiltInConfig(emulation);
    auto* node3 = CreateEmulatedNodeWithDefaultBuiltInConfig(emulation);
    auto* node4 = CreateEmulatedNodeWithDefaultBuiltInConfig(emulation);

    emulation->CreateRoute(e1, {node1}, e2);
    emulation->CreateRoute(e2, {node2}, e1);

    emulation->CreateRoute(e1, {node3}, e3);
    emulation->CreateRoute(e3, {node4}, e1);
  });
  SendPacketsAndValidateDelivery();
}

// Testing that packets are delivered via all routes using a routing scheme as
// follows:
//  * e1 -> n1 -> e2
//  * e2 -> n2 -> e1
//  * e1 -> n1 -> e3
//  * e3 -> n4 -> e1
TEST_F(NetworkEmulationManagerThreeNodesRoutingTest,
       PacketsAreDeliveredInBothWaysWhenConnectedToTwoPeersOverSameSendLink) {
  SetupRouting([](EmulatedEndpoint* e1, EmulatedEndpoint* e2,
                  EmulatedEndpoint* e3, NetworkEmulationManager* emulation) {
    auto* node1 = CreateEmulatedNodeWithDefaultBuiltInConfig(emulation);
    auto* node2 = CreateEmulatedNodeWithDefaultBuiltInConfig(emulation);
    auto* node3 = CreateEmulatedNodeWithDefaultBuiltInConfig(emulation);

    emulation->CreateRoute(e1, {node1}, e2);
    emulation->CreateRoute(e2, {node2}, e1);

    emulation->CreateRoute(e1, {node1}, e3);
    emulation->CreateRoute(e3, {node3}, e1);
  });
  SendPacketsAndValidateDelivery();
}

}  // namespace test
}  // namespace webrtc
