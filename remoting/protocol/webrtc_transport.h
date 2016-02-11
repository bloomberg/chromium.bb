// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_WEBRTC_TRANSPORT_H_
#define REMOTING_PROTOCOL_WEBRTC_TRANSPORT_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/timer/timer.h"
#include "remoting/protocol/transport.h"
#include "remoting/protocol/webrtc_data_stream_adapter.h"
#include "remoting/signaling/signal_strategy.h"
#include "third_party/webrtc/api/peerconnectioninterface.h"

namespace webrtc {
class FakeAudioDeviceModule;
}  // namespace webrtc

namespace remoting {
namespace protocol {

class TransportContext;
class MessageChannelFactory;

class WebrtcTransport : public Transport,
                        public webrtc::PeerConnectionObserver {
 public:
  class EventHandler {
   public:
    // Called after |peer_connection| has been created but before handshake. The
    // handler should create data channels and media streams. Renegotiation will
    // be required in two cases after this method returns:
    //   1. When the first data channel is created, if it wasn't created by this
    //      event handler.
    //   2. Whenever a media stream is added or removed.
    virtual void OnWebrtcTransportConnecting() = 0;

    // Called when the transport is connected.
    virtual void OnWebrtcTransportConnected() = 0;

    // Called when there is an error connecting the session.
    virtual void OnWebrtcTransportError(ErrorCode error) = 0;

    // Called when an incoming media stream is added or removed.
    virtual void OnWebrtcTransportMediaStreamAdded(
        scoped_refptr<webrtc::MediaStreamInterface> stream) = 0;
    virtual void OnWebrtcTransportMediaStreamRemoved(
        scoped_refptr<webrtc::MediaStreamInterface> stream) = 0;
  };

  WebrtcTransport(rtc::Thread* worker_thread,
                  scoped_refptr<TransportContext> transport_context,
                  EventHandler* event_handler);
  ~WebrtcTransport() override;

  webrtc::PeerConnectionInterface* peer_connection() {
    return peer_connection_;
  }
  webrtc::PeerConnectionFactoryInterface* peer_connection_factory() {
    return peer_connection_factory_;
  }

  // Factories for outgoing and incoming data channels. Must be used only after
  // the transport is connected.
  MessageChannelFactory* outgoing_channel_factory() {
    return &outgoing_data_stream_adapter_;
  }
  MessageChannelFactory* incoming_channel_factory() {
    return &incoming_data_stream_adapter_;
  }

  // Transport interface.
  void Start(Authenticator* authenticator,
             SendTransportInfoCallback send_transport_info_callback) override;
  bool ProcessTransportInfo(buzz::XmlElement* transport_info) override;

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

  rtc::Thread* worker_thread_;
  scoped_refptr<TransportContext> transport_context_;
  EventHandler* event_handler_ = nullptr;
  SendTransportInfoCallback send_transport_info_callback_;

  scoped_ptr<webrtc::FakeAudioDeviceModule> fake_audio_device_module_;

  rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface>
      peer_connection_factory_;
  rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection_;

  bool negotiation_pending_ = false;

  bool connected_ = false;

  scoped_ptr<buzz::XmlElement> pending_transport_info_message_;
  base::OneShotTimer transport_info_timer_;

  ScopedVector<webrtc::IceCandidateInterface> pending_incoming_candidates_;

  WebrtcDataStreamAdapter outgoing_data_stream_adapter_;
  WebrtcDataStreamAdapter incoming_data_stream_adapter_;

  base::WeakPtrFactory<WebrtcTransport> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebrtcTransport);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_WEBRTC_TRANSPORT_H_
