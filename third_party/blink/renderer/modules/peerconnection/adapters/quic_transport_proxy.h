// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_QUIC_TRANSPORT_PROXY_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_QUIC_TRANSPORT_PROXY_H_

#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "net/third_party/quic/core/quic_types.h"
#include "third_party/webrtc/rtc_base/scoped_ref_ptr.h"

namespace rtc {
class RTCCertificate;
struct SSLFingerprint;
}  // namespace rtc

namespace blink {

class IceTransportProxy;
class QuicTransportHost;

// This class allows the QUIC implementation (P2PQuicTransport) to run on a
// thread different from the thread from which it is controlled. All
// interactions with the QUIC implementation happen asynchronously.
//
// The QuicTransportProxy is intended to be used with an IceTransportProxy --
// see the IceTransportProxy class documentation for background and terms. The
// proxy and host threads used with the QuicTransportProxy should be the same as
// the ones used with the connected IceTransportProxy.
class QuicTransportProxy final {
 public:
  // Delegate for receiving callbacks from the QUIC implementation. These all
  // run on the proxy thread.
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Called when the QUIC handshake finishes and fingerprints have been
    // verified.
    virtual void OnConnected() {}
    // Called when the remote side has indicated it is closed.
    virtual void OnRemoteStopped() {}
    // Called when the connection is closed due to a QUIC error. This can happen
    // locally by the framer or remotely by the peer.
    virtual void OnConnectionFailed(const std::string& error_details,
                                    bool from_remote) {}
  };

  // Construct a Proxy with the underlying QUIC implementation running on the
  // same thread as the IceTransportProxy. Callbacks will be serviced by the
  // given delegate.
  // The delegate and IceTransportProxy must outlive the QuicTransportProxy.
  // The QuicTransportProxy will immediately connect to the given
  // IceTransportProxy; it can be disconnected by destroying the
  // QuicTransportProxy object.
  QuicTransportProxy(
      Delegate* delegate,
      IceTransportProxy* ice_transport_proxy,
      quic::Perspective perspective,
      const std::vector<rtc::scoped_refptr<rtc::RTCCertificate>>& certificates);
  ~QuicTransportProxy();

  scoped_refptr<base::SingleThreadTaskRunner> proxy_thread() const;
  scoped_refptr<base::SingleThreadTaskRunner> host_thread() const;

  void Start(
      std::vector<std::unique_ptr<rtc::SSLFingerprint>> remote_fingerprints);
  void Stop();

 private:
  // Callbacks from QuicTransportHost.
  friend class QuicTransportHost;
  void OnConnected();
  void OnRemoteStopped();
  void OnConnectionFailed(const std::string& error_details, bool from_remote);

  // Since the Host is deleted on the host thread (Via OnTaskRunnerDeleter), as
  // long as this is alive it is safe to post tasks to it (using unretained).
  std::unique_ptr<QuicTransportHost, base::OnTaskRunnerDeleter> host_;
  Delegate* const delegate_;
  IceTransportProxy* ice_transport_proxy_;

  THREAD_CHECKER(thread_checker_);

  // Must be the last member.
  base::WeakPtrFactory<QuicTransportProxy> weak_ptr_factory_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_QUIC_TRANSPORT_PROXY_H_
