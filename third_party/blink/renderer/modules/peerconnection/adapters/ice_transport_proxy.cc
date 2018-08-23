// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/adapters/ice_transport_proxy.h"

#include "third_party/blink/renderer/modules/peerconnection/adapters/ice_transport_host.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/web_rtc_cross_thread_copier.h"
#include "third_party/blink/renderer/platform/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/web_task_runner.h"

namespace blink {

IceTransportProxy::IceTransportProxy(
    FrameScheduler* frame_scheduler,
    scoped_refptr<base::SingleThreadTaskRunner> host_thread,
    rtc::Thread* host_thread_rtc_thread,
    Delegate* delegate,
    std::unique_ptr<cricket::PortAllocator> port_allocator)
    : host_thread_(std::move(host_thread)),
      host_(nullptr, base::OnTaskRunnerDeleter(host_thread_)),
      delegate_(delegate),
      connection_handle_for_scheduler_(
          frame_scheduler->OnActiveConnectionCreated()),
      weak_ptr_factory_(this) {
  DCHECK(host_thread_);
  DCHECK(delegate_);
  scoped_refptr<base::SingleThreadTaskRunner> proxy_thread =
      frame_scheduler->GetTaskRunner(TaskType::kNetworking);
  DCHECK(proxy_thread->BelongsToCurrentThread());
  // Wait to initialize the host until the weak_ptr_factory_ is initialized.
  // The IceTransportHost is constructed on the proxy thread but should only be
  // interacted with via PostTask to the host thread. The OnTaskRunnerDeleter
  // (configured above) will ensure it gets deleted on the host thread.
  host_.reset(new IceTransportHost(proxy_thread, weak_ptr_factory_.GetWeakPtr(),
                                   std::move(port_allocator)));
  PostCrossThreadTask(
      *host_thread_, FROM_HERE,
      CrossThreadBind(&IceTransportHost::Initialize,
                      CrossThreadUnretained(host_.get()),
                      CrossThreadUnretained(host_thread_rtc_thread)));
}

IceTransportProxy::~IceTransportProxy() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Note: The IceTransportHost will be deleted on the host thread.
}

void IceTransportProxy::StartGathering(
    const cricket::IceParameters& local_parameters,
    const cricket::ServerAddresses& stun_servers,
    const std::vector<cricket::RelayServerConfig>& turn_servers,
    int32_t candidate_filter) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  PostCrossThreadTask(
      *host_thread_, FROM_HERE,
      CrossThreadBind(&IceTransportHost::StartGathering,
                      CrossThreadUnretained(host_.get()), local_parameters,
                      stun_servers, turn_servers, candidate_filter));
}

void IceTransportProxy::OnGatheringStateChanged(
    cricket::IceGatheringState new_state) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  delegate_->OnGatheringStateChanged(new_state);
}

void IceTransportProxy::OnCandidateGathered(
    const cricket::Candidate& candidate) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  delegate_->OnCandidateGathered(candidate);
}

}  // namespace blink
