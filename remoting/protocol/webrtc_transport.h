// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_WEBRTC_TRANSPORT_H_
#define REMOTING_PROTOCOL_WEBRTC_TRANSPORT_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/timer/timer.h"
#include "remoting/protocol/transport.h"
#include "remoting/protocol/webrtc_data_stream_adapter.h"
#include "remoting/signaling/signal_strategy.h"
#include "third_party/libjingle/source/talk/app/webrtc/peerconnectioninterface.h"

namespace webrtc {
class FakeAudioDeviceModule;
}  // namespace webrtc

namespace remoting {
namespace protocol {

class WebrtcTransport : public Transport,
                        public webrtc::PeerConnectionObserver {
 public:
  WebrtcTransport(rtc::Thread* worker_thread,
                  rtc::scoped_refptr<webrtc::PortAllocatorFactoryInterface>
                      port_allocator_factory,
                  TransportRole role);
  ~WebrtcTransport() override;

  webrtc::PeerConnectionInterface* peer_connection() {
    return peer_connection_;
  }
  webrtc::PeerConnectionFactoryInterface* peer_connection_factory() {
    return peer_connection_factory_;
  }

  // Transport interface.
  void Start(EventHandler* event_handler,
             Authenticator* authenticator) override;
  bool ProcessTransportInfo(buzz::XmlElement* transport_info) override;
  StreamChannelFactory* GetStreamChannelFactory() override;
  StreamChannelFactory* GetMultiplexedChannelFactory() override;
  WebrtcTransport* AsWebrtcTransport() override;

 private:
  void OnLocalSessionDescriptionCreated(
      scoped_ptr<webrtc::SessionDescriptionInterface> description,
      const std::string& error);
  void OnLocalDescriptionSet(bool success, const std::string& error);
  void OnRemoteDescriptionSet(bool send_answer,
                              bool success,
                              const std::string& error);

  // webrtc::PeerConnectionObserver interface.
  void OnSignalingChange(
      webrtc::PeerConnectionInterface::SignalingState new_state) override;
  void OnAddStream(webrtc::MediaStreamInterface* stream) override;
  void OnRemoveStream(webrtc::MediaStreamInterface* stream) override;
  void OnDataChannel(webrtc::DataChannelInterface* data_channel) override;
  void OnRenegotiationNeeded() override;
  void OnIceConnectionChange(
      webrtc::PeerConnectionInterface::IceConnectionState new_state) override;
  void OnIceGatheringChange(
      webrtc::PeerConnectionInterface::IceGatheringState new_state) override;
  void OnIceCandidate(const webrtc::IceCandidateInterface* candidate) override;

  void RequestNegotiation();
  void SendOffer();
  void EnsurePendingTransportInfoMessage();
  void SendTransportInfo();
  void AddPendingCandidatesIfPossible();

  void Close(ErrorCode error);

  base::ThreadChecker thread_checker_;

  rtc::scoped_refptr<webrtc::PortAllocatorFactoryInterface>
      port_allocator_factory_;
  TransportRole role_;
  EventHandler* event_handler_ = nullptr;
  rtc::Thread* worker_thread_;

  scoped_ptr<webrtc::FakeAudioDeviceModule> fake_audio_device_module_;

  rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface>
      peer_connection_factory_;
  rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection_;

  bool negotiation_pending_ = false;

  scoped_ptr<buzz::XmlElement> pending_transport_info_message_;
  base::OneShotTimer transport_info_timer_;

  ScopedVector<webrtc::IceCandidateInterface> pending_incoming_candidates_;

  std::list<rtc::scoped_refptr<webrtc::MediaStreamInterface>>
      unclaimed_streams_;

  WebrtcDataStreamAdapter data_stream_adapter_;

  base::WeakPtrFactory<WebrtcTransport> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebrtcTransport);
};

class WebrtcTransportFactory : public TransportFactory {
 public:
  WebrtcTransportFactory(
      rtc::Thread* worker_thread,
      SignalStrategy* signal_strategy,
      rtc::scoped_refptr<webrtc::PortAllocatorFactoryInterface>
          port_allocator_factory,
      TransportRole role);
  ~WebrtcTransportFactory() override;

  // TransportFactory interface.
  scoped_ptr<Transport> CreateTransport() override;

 private:
  rtc::Thread* worker_thread_;
  SignalStrategy* signal_strategy_;
  rtc::scoped_refptr<webrtc::PortAllocatorFactoryInterface>
      port_allocator_factory_;
  TransportRole role_;

  DISALLOW_COPY_AND_ASSIGN(WebrtcTransportFactory);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_WEBRTC_TRANSPORT_H_
