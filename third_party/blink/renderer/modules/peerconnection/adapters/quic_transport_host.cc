// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/adapters/quic_transport_host.h"

#include "net/quic/quic_chromium_alarm_factory.h"
#include "net/third_party/quic/platform/impl/quic_chromium_clock.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/ice_transport_host.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/p2p_quic_transport_factory_impl.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/quic_transport_proxy.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/web_rtc_cross_thread_copier.h"
#include "third_party/blink/renderer/platform/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/web_task_runner.h"

namespace blink {

QuicTransportHost::QuicTransportHost(
    scoped_refptr<base::SingleThreadTaskRunner> proxy_thread,
    base::WeakPtr<QuicTransportProxy> proxy)
    : proxy_thread_(std::move(proxy_thread)), proxy_(std::move(proxy)) {
  DETACH_FROM_THREAD(thread_checker_);
  DCHECK(proxy_thread_);
  DCHECK(proxy_);
}

QuicTransportHost::~QuicTransportHost() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // If the TaskRunner this is getting initialized on is destroyed before
  // Initialize is called then |ice_transport_host_| may still be null.
  if (ice_transport_host_) {
    ice_transport_host_->DisconnectConsumer(this);
  }
}

void QuicTransportHost::Initialize(
    IceTransportHost* ice_transport_host,
    scoped_refptr<base::SingleThreadTaskRunner> host_thread,
    quic::Perspective perspective,
    const std::vector<rtc::scoped_refptr<rtc::RTCCertificate>>& certificates) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(ice_transport_host);
  DCHECK(!ice_transport_host_);
  ice_transport_host_ = ice_transport_host;
  quic::QuicClock* clock = quic::QuicChromiumClock::GetInstance();
  auto alarm_factory =
      std::make_unique<net::QuicChromiumAlarmFactory>(host_thread.get(), clock);
  quic_transport_factory_.reset(
      new P2PQuicTransportFactoryImpl(clock, std::move(alarm_factory)));
  P2PQuicTransportConfig config(
      this, ice_transport_host->ConnectConsumer(this)->packet_transport(),
      certificates);
  config.is_server = (perspective == quic::Perspective::IS_SERVER);
  quic_transport_ =
      quic_transport_factory_->CreateQuicTransport(std::move(config));
}

void QuicTransportHost::Start(
    std::vector<std::unique_ptr<rtc::SSLFingerprint>> remote_fingerprints) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  quic_transport_->Start(std::move(remote_fingerprints));
}

void QuicTransportHost::Stop() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  quic_transport_->Stop();
}

void QuicTransportHost::OnRemoteStopped() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  PostCrossThreadTask(
      *proxy_thread_, FROM_HERE,
      CrossThreadBind(&QuicTransportProxy::OnRemoteStopped, proxy_));
}

void QuicTransportHost::OnConnectionFailed(const std::string& error_details,
                                           bool from_remote) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  PostCrossThreadTask(*proxy_thread_, FROM_HERE,
                      CrossThreadBind(&QuicTransportProxy::OnConnectionFailed,
                                      proxy_, error_details, from_remote));
}

void QuicTransportHost::OnConnected() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  PostCrossThreadTask(
      *proxy_thread_, FROM_HERE,
      CrossThreadBind(&QuicTransportProxy::OnConnected, proxy_));
}

}  // namespace blink
