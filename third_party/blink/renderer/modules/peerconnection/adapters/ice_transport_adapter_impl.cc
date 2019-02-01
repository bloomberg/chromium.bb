// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/adapters/ice_transport_adapter_impl.h"

#include "third_party/blink/renderer/modules/peerconnection/adapters/quic_packet_transport_adapter.h"

namespace blink {

IceTransportAdapterImpl::IceTransportAdapterImpl(
    Delegate* delegate,
    std::unique_ptr<cricket::PortAllocator> port_allocator,
    std::unique_ptr<webrtc::AsyncResolverFactory> async_resolver_factory,
    rtc::Thread* thread)
    : delegate_(delegate),
      port_allocator_(std::move(port_allocator)),
      async_resolver_factory_(std::move(async_resolver_factory)) {
  // TODO(bugs.webrtc.org/9419): Remove once WebRTC can be built as a component.
  if (!rtc::ThreadManager::Instance()->CurrentThread()) {
    rtc::ThreadManager::Instance()->SetCurrentThread(thread);
  }

  // These settings are copied from PeerConnection:
  // https://codesearch.chromium.org/chromium/src/third_party/webrtc/pc/peerconnection.cc?l=4708&rcl=820ebd0f661696043959b5105b2814e0edd8b694
  port_allocator_->set_step_delay(cricket::kMinimumStepDelay);
  port_allocator_->set_flags(port_allocator_->flags() |
                             cricket::PORTALLOCATOR_ENABLE_SHARED_SOCKET |
                             cricket::PORTALLOCATOR_ENABLE_IPV6 |
                             cricket::PORTALLOCATOR_ENABLE_IPV6_ON_WIFI);
  port_allocator_->Initialize();

  p2p_transport_channel_ = std::make_unique<cricket::P2PTransportChannel>(
      "", 0, port_allocator_.get(), async_resolver_factory.get());
  p2p_transport_channel_->SignalGatheringState.connect(
      this, &IceTransportAdapterImpl::OnGatheringStateChanged);
  p2p_transport_channel_->SignalCandidateGathered.connect(
      this, &IceTransportAdapterImpl::OnCandidateGathered);
  p2p_transport_channel_->SignalStateChanged.connect(
      this, &IceTransportAdapterImpl::OnStateChanged);
  p2p_transport_channel_->SignalNetworkRouteChanged.connect(
      this, &IceTransportAdapterImpl::OnNetworkRouteChanged);
  p2p_transport_channel_->SignalRoleConflict.connect(
      this, &IceTransportAdapterImpl::OnRoleConflict);
  // We need to set the ICE role even before Start is called since the Port
  // assumes that the role has been set before receiving incoming connectivity
  // checks. These checks can race with the information signaled for Start.
  p2p_transport_channel_->SetIceRole(cricket::ICEROLE_CONTROLLING);
  // The ICE tiebreaker is used to determine which side is controlling/
  // controlled when both sides start in the same role. The number is randomly
  // generated so that each peer can calculate a.tiebreaker <= b.tiebreaker
  // consistently.
  p2p_transport_channel_->SetIceTiebreaker(rtc::CreateRandomId64());
  quic_packet_transport_adapter_ = std::make_unique<QuicPacketTransportAdapter>(
      p2p_transport_channel_.get());
}

IceTransportAdapterImpl::~IceTransportAdapterImpl() = default;

static uint32_t GetCandidateFilterForPolicy(IceTransportPolicy policy) {
  switch (policy) {
    case IceTransportPolicy::kRelay:
      return cricket::CF_RELAY;
    case IceTransportPolicy::kAll:
      return cricket::CF_ALL;
  }
  NOTREACHED();
  return 0;
}

void IceTransportAdapterImpl::StartGathering(
    const cricket::IceParameters& local_parameters,
    const cricket::ServerAddresses& stun_servers,
    const std::vector<cricket::RelayServerConfig>& turn_servers,
    IceTransportPolicy policy) {
  port_allocator_->set_candidate_filter(GetCandidateFilterForPolicy(policy));
  port_allocator_->SetConfiguration(stun_servers, turn_servers,
                                    port_allocator_->candidate_pool_size(),
                                    port_allocator_->prune_turn_ports());

  p2p_transport_channel_->SetIceParameters(local_parameters);
  p2p_transport_channel_->MaybeStartGathering();
  DCHECK_EQ(p2p_transport_channel_->gathering_state(),
            cricket::kIceGatheringGathering);
}

void IceTransportAdapterImpl::Start(
    const cricket::IceParameters& remote_parameters,
    cricket::IceRole role,
    const std::vector<cricket::Candidate>& initial_remote_candidates) {
  p2p_transport_channel_->SetRemoteIceParameters(remote_parameters);
  p2p_transport_channel_->SetIceRole(role);
  for (const auto& candidate : initial_remote_candidates) {
    p2p_transport_channel_->AddRemoteCandidate(candidate);
  }
}

void IceTransportAdapterImpl::HandleRemoteRestart(
    const cricket::IceParameters& new_remote_parameters) {
  p2p_transport_channel_->RemoveAllRemoteCandidates();
  p2p_transport_channel_->SetRemoteIceParameters(new_remote_parameters);
}

void IceTransportAdapterImpl::AddRemoteCandidate(
    const cricket::Candidate& candidate) {
  p2p_transport_channel_->AddRemoteCandidate(candidate);
}

P2PQuicPacketTransport* IceTransportAdapterImpl::packet_transport() const {
  return quic_packet_transport_adapter_.get();
}

void IceTransportAdapterImpl::OnGatheringStateChanged(
    cricket::IceTransportInternal* transport) {
  DCHECK_EQ(transport, p2p_transport_channel_.get());
  delegate_->OnGatheringStateChanged(p2p_transport_channel_->gathering_state());
}

void IceTransportAdapterImpl::OnCandidateGathered(
    cricket::IceTransportInternal* transport,
    const cricket::Candidate& candidate) {
  DCHECK_EQ(transport, p2p_transport_channel_.get());
  delegate_->OnCandidateGathered(candidate);
}

void IceTransportAdapterImpl::OnStateChanged(
    cricket::IceTransportInternal* transport) {
  DCHECK_EQ(transport, p2p_transport_channel_.get());
  delegate_->OnStateChanged(p2p_transport_channel_->GetState());
}

void IceTransportAdapterImpl::OnNetworkRouteChanged(
    absl::optional<rtc::NetworkRoute> new_network_route) {
  const cricket::CandidatePairInterface* selected_connection =
      p2p_transport_channel_->selected_connection();
  if (!selected_connection) {
    // The selected connection will only be null if the ICE connection has
    // totally failed, at which point we'll get a StateChanged signal. The
    // client will implicitly clear the selected candidate pair when it receives
    // the failed state change, so we don't need to give an explicit callback
    // here.
    return;
  }
  delegate_->OnSelectedCandidatePairChanged(
      std::make_pair(selected_connection->local_candidate(),
                     selected_connection->remote_candidate()));
}

static const char* IceRoleToString(cricket::IceRole role) {
  switch (role) {
    case cricket::ICEROLE_CONTROLLING:
      return "controlling";
    case cricket::ICEROLE_CONTROLLED:
      return "controlled";
    default:
      return "unknown";
  }
}

static cricket::IceRole IceRoleReversed(cricket::IceRole role) {
  switch (role) {
    case cricket::ICEROLE_CONTROLLING:
      return cricket::ICEROLE_CONTROLLED;
    case cricket::ICEROLE_CONTROLLED:
      return cricket::ICEROLE_CONTROLLING;
    default:
      return cricket::ICEROLE_UNKNOWN;
  }
}

void IceTransportAdapterImpl::OnRoleConflict(
    cricket::IceTransportInternal* transport) {
  DCHECK_EQ(transport, p2p_transport_channel_.get());
  // This logic is copied from JsepTransportController.
  cricket::IceRole reversed_role =
      IceRoleReversed(p2p_transport_channel_->GetIceRole());
  LOG(INFO) << "Got role conflict; switching to "
            << IceRoleToString(reversed_role) << " role.";
  p2p_transport_channel_->SetIceRole(reversed_role);
}

}  // namespace blink
