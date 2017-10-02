// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_WEBRTC_TRANSPORT_H_
#define REMOTING_PROTOCOL_WEBRTC_TRANSPORT_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/timer/timer.h"
#include "crypto/hmac.h"
#include "remoting/protocol/transport.h"
#include "remoting/protocol/webrtc_data_stream_adapter.h"
#include "remoting/protocol/webrtc_dummy_video_encoder.h"
#include "remoting/signaling/signal_strategy.h"
#include "third_party/webrtc/api/peerconnectioninterface.h"

namespace remoting {
namespace protocol {

class TransportContext;
class MessagePipe;
class WebrtcAudioModule;

class WebrtcTransport : public Transport {
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

    // Called when a new data channel is created by the peer.
    virtual void OnWebrtcTransportIncomingDataChannel(
        const std::string& name,
        std::unique_ptr<MessagePipe> pipe) = 0;

    // Called when an incoming media stream is added or removed.
    virtual void OnWebrtcTransportMediaStreamAdded(
        scoped_refptr<webrtc::MediaStreamInterface> stream) = 0;
    virtual void OnWebrtcTransportMediaStreamRemoved(
        scoped_refptr<webrtc::MediaStreamInterface> stream) = 0;

   protected:
    virtual ~EventHandler() {}
  };

  WebrtcTransport(rtc::Thread* worker_thread,
                  scoped_refptr<TransportContext> transport_context,
                  EventHandler* event_handler);
  ~WebrtcTransport() override;

  webrtc::PeerConnectionInterface* peer_connection();
  webrtc::PeerConnectionFactoryInterface* peer_connection_factory();
  WebrtcDummyVideoEncoderFactory* video_encoder_factory() {
    return video_encoder_factory_;
  }
  WebrtcAudioModule* audio_module();

  // Creates outgoing data channel. The channel is created in CONNECTING state.
  // The caller must wait for OnMessagePipeOpen() notification before sending
  // any messages.
  std::unique_ptr<MessagePipe> CreateOutgoingChannel(const std::string& name);

  // Transport interface.
  void Start(Authenticator* authenticator,
             SendTransportInfoCallback send_transport_info_callback) override;
  bool ProcessTransportInfo(buzz::XmlElement* transport_info) override;
  void Close(ErrorCode error);

  void SetPreferredVideoCodec(const std::string& codec);

 private:
  // PeerConnectionWrapper is responsible for PeerConnection creation,
  // ownership. It passes all events to the corresponding methods below. This is
  // necessary to make it possible to close and destroy PeerConnection
  // asynchronously, as it may be on stack when the transport is destroyed.
  class PeerConnectionWrapper;
  friend class PeerConnectionWrapper;

  void OnLocalSessionDescriptionCreated(
      std::unique_ptr<webrtc::SessionDescriptionInterface> description,
      const std::string& error);
  void OnLocalDescriptionSet(bool success, const std::string& error);
  void OnRemoteDescriptionSet(bool send_answer,
                              bool success,
                              const std::string& error);

  // PeerConnection event handlers, called by PeerConnectionWrapper.
  void OnSignalingChange(
      webrtc::PeerConnectionInterface::SignalingState new_state);
  void OnAddStream(
      rtc::scoped_refptr<webrtc::MediaStreamInterface> stream);
  void OnRemoveStream(
      rtc::scoped_refptr<webrtc::MediaStreamInterface> stream);
  void OnDataChannel(
      rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel);
  void OnRenegotiationNeeded();
  void OnIceConnectionChange(
      webrtc::PeerConnectionInterface::IceConnectionState new_state);
  void OnIceGatheringChange(
      webrtc::PeerConnectionInterface::IceGatheringState new_state);
  void OnIceCandidate(const webrtc::IceCandidateInterface* candidate);

  void RequestNegotiation();
  void SendOffer();
  void EnsurePendingTransportInfoMessage();
  void SendTransportInfo();
  void AddPendingCandidatesIfPossible();

  base::ThreadChecker thread_checker_;

  scoped_refptr<TransportContext> transport_context_;
  EventHandler* event_handler_ = nullptr;
  SendTransportInfoCallback send_transport_info_callback_;

  crypto::HMAC handshake_hmac_;

  std::unique_ptr<PeerConnectionWrapper> peer_connection_wrapper_;

  WebrtcDummyVideoEncoderFactory* video_encoder_factory_;

  bool negotiation_pending_ = false;

  bool connected_ = false;

  std::unique_ptr<buzz::XmlElement> pending_transport_info_message_;
  base::OneShotTimer transport_info_timer_;

  std::vector<std::unique_ptr<webrtc::IceCandidateInterface>>
      pending_incoming_candidates_;

  std::string preferred_video_codec_;

  base::WeakPtrFactory<WebrtcTransport> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebrtcTransport);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_WEBRTC_TRANSPORT_H_
