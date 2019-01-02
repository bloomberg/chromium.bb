// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/adapters/ice_transport_host.h"

#include "third_party/blink/renderer/modules/peerconnection/adapters/ice_transport_proxy.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/web_rtc_cross_thread_copier.h"
#include "third_party/blink/renderer/platform/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/web_task_runner.h"

namespace blink {

IceTransportHost::IceTransportHost(
    scoped_refptr<base::SingleThreadTaskRunner> proxy_thread,
    base::WeakPtr<IceTransportProxy> proxy,
    std::unique_ptr<cricket::PortAllocator> port_allocator)
    : proxy_thread_(std::move(proxy_thread)),
      port_allocator_(std::move(port_allocator)),
      proxy_(std::move(proxy)) {
  DETACH_FROM_THREAD(thread_checker_);
  DCHECK(proxy_thread_);
  DCHECK(proxy_);
  DCHECK(port_allocator_);
}

IceTransportHost::~IceTransportHost() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void IceTransportHost::Initialize(rtc::Thread* host_thread_rtc_thread) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // TODO(bugs.webrtc.org/9419): Remove once WebRTC can be built as a component.
  if (!rtc::ThreadManager::Instance()->CurrentThread()) {
    rtc::ThreadManager::Instance()->SetCurrentThread(host_thread_rtc_thread);
  }
  // These settings are copied from PeerConnection:
  // https://codesearch.chromium.org/chromium/src/third_party/webrtc/pc/peerconnection.cc?l=4708&rcl=820ebd0f661696043959b5105b2814e0edd8b694
  port_allocator_->set_step_delay(cricket::kMinimumStepDelay);
  port_allocator_->set_flags(port_allocator_->flags() |
                             cricket::PORTALLOCATOR_ENABLE_SHARED_SOCKET |
                             cricket::PORTALLOCATOR_ENABLE_IPV6 |
                             cricket::PORTALLOCATOR_ENABLE_IPV6_ON_WIFI);
  port_allocator_->Initialize();
  transport_ = std::make_unique<cricket::P2PTransportChannel>(
      "", 0, port_allocator_.get());
  transport_->SignalGatheringState.connect(
      this, &IceTransportHost::OnGatheringStateChanged);
  transport_->SignalCandidateGathered.connect(
      this, &IceTransportHost::OnCandidateGathered);
  transport_->SignalStateChanged.connect(this,
                                         &IceTransportHost::OnStateChanged);
  // The ICE tiebreaker is used to determine which side is controlling/
  // controlled when both sides start in the same role. The number is randomly
  // generated so that each peer can calculate a.tiebreaker <= b.tiebreaker
  // consistently.
  transport_->SetIceTiebreaker(rtc::CreateRandomId64());
}

void IceTransportHost::StartGathering(
    const cricket::IceParameters& local_parameters,
    const cricket::ServerAddresses& stun_servers,
    const std::vector<cricket::RelayServerConfig>& turn_servers,
    int32_t candidate_filter) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  port_allocator_->set_candidate_filter(candidate_filter);
  port_allocator_->SetConfiguration(stun_servers, turn_servers,
                                    port_allocator_->candidate_pool_size(),
                                    port_allocator_->prune_turn_ports());

  transport_->SetIceParameters(local_parameters);
  transport_->MaybeStartGathering();
  DCHECK_EQ(transport_->gathering_state(), cricket::kIceGatheringGathering);
}

void IceTransportHost::SetRole(cricket::IceRole role) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  transport_->SetIceRole(role);
}

void IceTransportHost::SetRemoteParameters(
    const cricket::IceParameters& remote_parameters) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  transport_->SetRemoteIceParameters(remote_parameters);
}

void IceTransportHost::AddRemoteCandidate(const cricket::Candidate& candidate) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  transport_->AddRemoteCandidate(candidate);
}

void IceTransportHost::ClearRemoteCandidates() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto remote_candidates = transport_->remote_candidates();
  for (const auto& remote_candidate : remote_candidates) {
    transport_->RemoveRemoteCandidate(remote_candidate);
  }
}

void IceTransportHost::OnGatheringStateChanged(
    cricket::IceTransportInternal* transport) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_EQ(transport, transport_.get());
  PostCrossThreadTask(
      *proxy_thread_, FROM_HERE,
      CrossThreadBind(&IceTransportProxy::OnGatheringStateChanged, proxy_,
                      transport_->gathering_state()));
}

void IceTransportHost::OnCandidateGathered(
    cricket::IceTransportInternal* transport,
    const cricket::Candidate& candidate) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_EQ(transport, transport_.get());
  PostCrossThreadTask(*proxy_thread_, FROM_HERE,
                      CrossThreadBind(&IceTransportProxy::OnCandidateGathered,
                                      proxy_, candidate));
}

void IceTransportHost::OnStateChanged(
    cricket::IceTransportInternal* transport) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_EQ(transport, transport_.get());
  PostCrossThreadTask(*proxy_thread_, FROM_HERE,
                      CrossThreadBind(&IceTransportProxy::OnStateChanged,
                                      proxy_, transport_->GetState()));
}

}  // namespace blink
