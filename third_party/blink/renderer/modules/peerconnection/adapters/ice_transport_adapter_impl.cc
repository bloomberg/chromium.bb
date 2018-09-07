// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/adapters/ice_transport_adapter_impl.h"

namespace blink {

IceTransportAdapterImpl::IceTransportAdapterImpl(
    Delegate* delegate,
    std::unique_ptr<cricket::PortAllocator> port_allocator,
    rtc::Thread* thread)
    : delegate_(delegate), port_allocator_(std::move(port_allocator)) {
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
      "", 0, port_allocator_.get());
  p2p_transport_channel_->SignalGatheringState.connect(
      this, &IceTransportAdapterImpl::OnGatheringStateChanged);
  p2p_transport_channel_->SignalCandidateGathered.connect(
      this, &IceTransportAdapterImpl::OnCandidateGathered);
  p2p_transport_channel_->SignalStateChanged.connect(
      this, &IceTransportAdapterImpl::OnStateChanged);
  // The ICE tiebreaker is used to determine which side is controlling/
  // controlled when both sides start in the same role. The number is randomly
  // generated so that each peer can calculate a.tiebreaker <= b.tiebreaker
  // consistently.
  p2p_transport_channel_->SetIceTiebreaker(rtc::CreateRandomId64());
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
  auto remote_candidates = p2p_transport_channel_->remote_candidates();
  for (const auto& remote_candidate : remote_candidates) {
    p2p_transport_channel_->RemoveRemoteCandidate(remote_candidate);
  }
  p2p_transport_channel_->SetRemoteIceParameters(new_remote_parameters);
}

void IceTransportAdapterImpl::AddRemoteCandidate(
    const cricket::Candidate& candidate) {
  p2p_transport_channel_->AddRemoteCandidate(candidate);
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

}  // namespace blink
