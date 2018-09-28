// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_QUIC_TRANSPORT_HOST_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_QUIC_TRANSPORT_HOST_H_

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "net/third_party/quic/core/quic_types.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/p2p_quic_transport.h"

namespace blink {

class IceTransportHost;
class P2PQuicTransportFactory;
class QuicTransportProxy;

// The host class is the host side correspondent to the QuicTransportProxy. See
// the QuicTransportProxy documentation for background. This class lives on the
// host thread and proxies calls between the QuicTransportProxy and the
// P2PQuicTransport (which is single-threaded).
//
//        proxy thread                                host thread
// +-----------------------+             +-----------------------------------+
// |                       |             |                                   |
// |        <-> ICE Proxy  |  =========> |  ICE Host <-> P2PTransportChannel |
// |               ^       |  <--------- |     ^                ^            |
// | client        |       |             |     |                |            |
// |               v       |             |     v                v            |
// |        <-> QUIC Proxy |  =========> | QUIC Host <-> P2PQuicTransport    |
// |                       |  <--------- |                                   |
// +-----------------------+             +-----------------------------------+
//
// The QuicTransportHost connects to the underlying IceTransportHost in
// Initialize and disconnects in the destructor. The IceTransportHost must
// outlive the QuicTransportHost.
//
// The Host can be constructed on any thread but after that point all methods
// must be called on the host thread.
class QuicTransportHost final : public P2PQuicTransport::Delegate {
 public:
  QuicTransportHost(scoped_refptr<base::SingleThreadTaskRunner> proxy_thread,
                    base::WeakPtr<QuicTransportProxy> transport_proxy);
  ~QuicTransportHost() override;

  void Initialize(
      IceTransportHost* ice_transport_host,
      scoped_refptr<base::SingleThreadTaskRunner> host_thread,
      quic::Perspective perspective,
      const std::vector<rtc::scoped_refptr<rtc::RTCCertificate>>& certificates);

  void Start(
      std::vector<std::unique_ptr<rtc::SSLFingerprint>> remote_fingerprints);
  void Stop();

 private:
  // P2PQuicTransport::Delegate overrides.
  void OnRemoteStopped() override;
  void OnConnectionFailed(const std::string& error_details,
                          bool from_remote) override;
  void OnConnected() override;

  const scoped_refptr<base::SingleThreadTaskRunner> proxy_thread_;
  std::unique_ptr<P2PQuicTransportFactory> quic_transport_factory_;
  std::unique_ptr<P2PQuicTransport> quic_transport_;
  base::WeakPtr<QuicTransportProxy> proxy_;
  IceTransportHost* ice_transport_host_ = nullptr;

  THREAD_CHECKER(thread_checker_);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_QUIC_TRANSPORT_HOST_H_
