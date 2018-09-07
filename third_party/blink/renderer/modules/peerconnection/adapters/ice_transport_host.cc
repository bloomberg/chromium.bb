// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/adapters/ice_transport_host.h"

#include "third_party/blink/renderer/modules/peerconnection/adapters/ice_transport_adapter_impl.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/ice_transport_proxy.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/web_rtc_cross_thread_copier.h"
#include "third_party/blink/renderer/platform/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/web_task_runner.h"

namespace blink {

IceTransportHost::IceTransportHost(
    scoped_refptr<base::SingleThreadTaskRunner> proxy_thread,
    base::WeakPtr<IceTransportProxy> proxy)
    : proxy_thread_(std::move(proxy_thread)), proxy_(std::move(proxy)) {
  DETACH_FROM_THREAD(thread_checker_);
  DCHECK(proxy_thread_);
  DCHECK(proxy_);
}

IceTransportHost::~IceTransportHost() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void IceTransportHost::Initialize(
    std::unique_ptr<cricket::PortAllocator> port_allocator,
    rtc::Thread* host_thread_rtc_thread) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(port_allocator);
  DCHECK(host_thread_rtc_thread);
  transport_ = std::make_unique<IceTransportAdapterImpl>(
      this, std::move(port_allocator), host_thread_rtc_thread);
}

void IceTransportHost::StartGathering(
    const cricket::IceParameters& local_parameters,
    const cricket::ServerAddresses& stun_servers,
    const std::vector<cricket::RelayServerConfig>& turn_servers,
    IceTransportPolicy policy) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  transport_->StartGathering(local_parameters, stun_servers, turn_servers,
                             policy);
}

void IceTransportHost::Start(
    const cricket::IceParameters& remote_parameters,
    cricket::IceRole role,
    const std::vector<cricket::Candidate>& initial_remote_candidates) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  transport_->Start(remote_parameters, role, initial_remote_candidates);
}

void IceTransportHost::HandleRemoteRestart(
    const cricket::IceParameters& new_remote_parameters) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  transport_->HandleRemoteRestart(new_remote_parameters);
}

void IceTransportHost::AddRemoteCandidate(const cricket::Candidate& candidate) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  transport_->AddRemoteCandidate(candidate);

}

void IceTransportHost::OnGatheringStateChanged(
    cricket::IceGatheringState new_state) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  PostCrossThreadTask(
      *proxy_thread_, FROM_HERE,
      CrossThreadBind(&IceTransportProxy::OnGatheringStateChanged, proxy_,
                      new_state));
}

void IceTransportHost::OnCandidateGathered(
    const cricket::Candidate& candidate) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  PostCrossThreadTask(*proxy_thread_, FROM_HERE,
                      CrossThreadBind(&IceTransportProxy::OnCandidateGathered,
                                      proxy_, candidate));
}

void IceTransportHost::OnStateChanged(cricket::IceTransportState new_state) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  PostCrossThreadTask(
      *proxy_thread_, FROM_HERE,
      CrossThreadBind(&IceTransportProxy::OnStateChanged, proxy_, new_state));
}

}  // namespace blink
