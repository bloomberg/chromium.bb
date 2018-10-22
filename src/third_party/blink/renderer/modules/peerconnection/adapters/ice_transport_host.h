// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_ICE_TRANSPORT_HOST_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_ICE_TRANSPORT_HOST_H_

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "third_party/webrtc/p2p/base/p2ptransportchannel.h"

namespace rtc {
class Thread;
}

namespace blink {

class IceTransportProxy;

// This class is the host side correspondent to the IceTransportProxy. See the
// IceTransportProxy documentation for background. This class lives on the host
// thread and proxies calls between the IceTransportProxy and the
// P2PTransportChannel (which is single-threaded).
//
//     proxy thread                               host thread
// +------------------+   unique_ptr   +------------------------------+
// |                  |   =========>   |                              |
// | client <-> Proxy |                | Host <-> P2PTransportChannel |
// |                  |   <---------   |                              |
// +------------------+    WeakPtr     +------------------------------+
//
// Since the client code controls the Proxy lifetime, the Proxy has a unique_ptr
// to the Host that lives on the host thread. The unique_ptr has an
// OnTaskRunnerDeleter so that when the Proxy is destroyed a task will be queued
// to delete the Host as well (and the P2PTransportChannel with it). The Host
// needs a pointer back to the Proxy to post callbacks, and by using a WeakPtr
// any callbacks run on the proxy thread after the proxy has been deleted will
// be safely dropped.
//
// The Host can be constructed on any thread but after that point all methods
// must be called on the host thread.
class IceTransportHost final : public sigslot::has_slots<> {
 public:
  IceTransportHost(scoped_refptr<base::SingleThreadTaskRunner> proxy_thread,
                   base::WeakPtr<IceTransportProxy> proxy,
                   std::unique_ptr<cricket::PortAllocator> port_allocator);
  ~IceTransportHost() override;

  void Initialize(rtc::Thread* host_thread_rtc_thread);

  void StartGathering(
      const cricket::IceParameters& local_parameters,
      const cricket::ServerAddresses& stun_servers,
      const std::vector<cricket::RelayServerConfig>& turn_servers,
      int32_t candidate_filter);

  void SetRole(cricket::IceRole role);
  void SetRemoteParameters(const cricket::IceParameters& remote_parameters);

  void AddRemoteCandidate(const cricket::Candidate& candidate);
  void ClearRemoteCandidates();

 private:
  // Callbacks from P2PTransportChannel.
  void OnGatheringStateChanged(cricket::IceTransportInternal* transport);
  void OnCandidateGathered(cricket::IceTransportInternal* transport,
                           const cricket::Candidate& candidate);
  void OnStateChanged(cricket::IceTransportInternal* transport);

  const scoped_refptr<base::SingleThreadTaskRunner> proxy_thread_;
  std::unique_ptr<cricket::PortAllocator> port_allocator_;
  std::unique_ptr<cricket::P2PTransportChannel> transport_;
  base::WeakPtr<IceTransportProxy> proxy_;

  THREAD_CHECKER(thread_checker_);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_ICE_TRANSPORT_HOST_H_
