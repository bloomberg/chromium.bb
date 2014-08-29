// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CAST_EXTENSION_SESSION_H_
#define REMOTING_HOST_CAST_EXTENSION_SESSION_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "jingle/glue/thread_wrapper.h"
#include "remoting/host/host_extension_session.h"
#include "third_party/libjingle/source/talk/app/webrtc/peerconnectioninterface.h"
#include "third_party/webrtc/base/scoped_ref_ptr.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"

namespace base {
class SingleThreadTaskRunner;
class WaitableEvent;
}  // namespace base

namespace net {
class URLRequestContextGetter;
}  // namespace net

namespace webrtc {
class MediaStreamInterface;
}  // namespace webrtc

namespace remoting {

class CastCreateSessionDescriptionObserver;

namespace protocol {
struct NetworkSettings;
}  // namespace protocol

// A HostExtensionSession implementation that enables WebRTC support using
// the PeerConnection native API.
class CastExtensionSession : public HostExtensionSession,
                             public webrtc::PeerConnectionObserver {
 public:
  virtual ~CastExtensionSession();

  // Creates and returns a CastExtensionSession object, after performing
  // initialization steps on it. The caller must take ownership of the returned
  // object.
  static scoped_ptr<CastExtensionSession> Create(
      scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
      scoped_refptr<net::URLRequestContextGetter> url_request_context_getter,
      const protocol::NetworkSettings& network_settings,
      ClientSessionControl* client_session_control,
      protocol::ClientStub* client_stub);

  // Called by webrtc::CreateSessionDescriptionObserver implementation.
  void OnCreateSessionDescription(webrtc::SessionDescriptionInterface* desc);
  void OnCreateSessionDescriptionFailure(const std::string& error);

  // HostExtensionSession interface.
  virtual void OnCreateVideoCapturer(
      scoped_ptr<webrtc::DesktopCapturer>* capturer) OVERRIDE;
  virtual bool ModifiesVideoPipeline() const OVERRIDE;
  virtual bool OnExtensionMessage(
      ClientSessionControl* client_session_control,
      protocol::ClientStub* client_stub,
      const protocol::ExtensionMessage& message) OVERRIDE;

  // webrtc::PeerConnectionObserver interface.
  virtual void OnError() OVERRIDE;
  virtual void OnSignalingChange(
      webrtc::PeerConnectionInterface::SignalingState new_state) OVERRIDE;
  virtual void OnStateChange(
      webrtc::PeerConnectionObserver::StateType state_changed) OVERRIDE;
  virtual void OnAddStream(webrtc::MediaStreamInterface* stream) OVERRIDE;
  virtual void OnRemoveStream(webrtc::MediaStreamInterface* stream) OVERRIDE;
  virtual void OnDataChannel(
      webrtc::DataChannelInterface* data_channel) OVERRIDE;
  virtual void OnRenegotiationNeeded() OVERRIDE;
  virtual void OnIceConnectionChange(
      webrtc::PeerConnectionInterface::IceConnectionState new_state) OVERRIDE;
  virtual void OnIceGatheringChange(
      webrtc::PeerConnectionInterface::IceGatheringState new_state) OVERRIDE;
  virtual void OnIceCandidate(
      const webrtc::IceCandidateInterface* candidate) OVERRIDE;
  virtual void OnIceComplete() OVERRIDE;

 private:
  CastExtensionSession(
      scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
      scoped_refptr<net::URLRequestContextGetter> url_request_context_getter,
      const protocol::NetworkSettings& network_settings,
      ClientSessionControl* client_session_control,
      protocol::ClientStub* client_stub);

  // Parses |message| for a Session Description and sets the remote
  // description, returning true if successful.
  bool ParseAndSetRemoteDescription(base::DictionaryValue* message);

  // Parses |message| for a PeerConnection ICE candidate and adds it to the
  // Peer Connection, returning true if successful.
  bool ParseAndAddICECandidate(base::DictionaryValue* message);

  // Sends a message to the client through |client_stub_|. This method must be
  // called on the network thread.
  //
  // A protocol::ExtensionMessage consists of two string fields: type and data.
  //
  // The type field must be |kExtensionMessageType|.
  // The data field must be a JSON formatted string with two compulsory
  // top level keys: |kTopLevelSubject| and |kTopLevelData|.
  //
  // The |subject| of a message describes the message to the receiving peer,
  // effectively identifying the command the receiving peer should perform.
  // The |subject| MUST be one of constants formatted as kSubject* defined in
  // the .cc file. This set of subjects is identical between host and client,
  // thus standardizing how they communicate.
  // The |data| of a message depends on the |subject| of the message.
  //
  // Examples of what ExtensionMessage.data() could look like:
  //
  // Host Ready Message:
  // Notifies the remote peer that we are ready to receive an offer.
  //
  // {
  //   "subject": "ready",
  //   "chromoting_data": "Host Ready to receive offers"
  // }
  //
  // WebRTC Offer Message:
  // Represents the offer received from the remote peer. The local
  // peer would then respond with a webrtc_answer message.
  // {
  //   "subject": "webrtc_offer",
  //   "chromoting_data": {
  //      "sdp" : "...",
  //      "type" : "offer"
  //    }
  // }
  //
  // WebRTC Candidate Message:
  // Represents an ICE candidate received from the remote peer. Each peer
  // shares its local ICE candidates in this way, until a connection is
  // established.
  //
  // {
  //   "subject": "webrtc_candidate",
  //   "chromoting_data": {
  //      "candidate" : "...",
  //      "sdpMid" : "...",
  //      "sdpMLineIndex" : "..."
  //    }
  // }
  //
  bool SendMessageToClient(const std::string& subject, const std::string& data);

  // Creates the jingle wrapper for the current thread, sets send to allowed,
  // and saves a pointer to the relevant thread pointer in ptr. If |event|
  // is not NULL, signals the event on completion.
  void EnsureTaskAndSetSend(rtc::Thread** ptr,
                            base::WaitableEvent* event = NULL);

  // Wraps each task runner in JingleThreadWrapper using EnsureTaskAndSetSend(),
  // returning true if successful. Wrapping the task runners allows them to be
  // shared with and used by the (about to be created) PeerConnectionFactory.
  bool WrapTasksAndSave();

  // Initializes PeerConnectionFactory and PeerConnection and sends a "ready"
  // message to client. Returns true if these steps are performed successfully.
  bool InitializePeerConnection();

  // Constructs a CastVideoCapturerAdapter, a VideoSource, a VideoTrack and a
  // MediaStream |stream_|, which it adds to the |peer_connection_|. Returns
  // true if these steps are performed successfully. This method is called only
  // when a PeerConnection offer is received from the client.
  bool SetupVideoStream(scoped_ptr<webrtc::DesktopCapturer> desktop_capturer);

  // Polls a single stats report from the PeerConnection immediately. Called
  // periodically using |stats_polling_timer_| after a PeerConnection has been
  // established.
  void PollPeerConnectionStats();

  // Closes |peer_connection_|, releases |peer_connection_|, |stream_| and
  // |peer_conn_factory_| and stops the worker thread.
  void CleanupPeerConnection();

  // Check if the connection is active.
  bool connection_active() const;

  // TaskRunners that will be used to setup the PeerConnectionFactory's
  // signalling thread and worker thread respectively.
  scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> worker_task_runner_;

  // Objects related to the WebRTC PeerConnection.
  rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection_;
  rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> peer_conn_factory_;
  rtc::scoped_refptr<webrtc::MediaStreamInterface> stream_;
  rtc::scoped_refptr<CastCreateSessionDescriptionObserver>
      create_session_desc_observer_;

  // Parameters passed to ChromiumPortAllocatorFactory on creation.
  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter_;
  const protocol::NetworkSettings& network_settings_;

  // Interface to interact with ClientSession.
  ClientSessionControl* client_session_control_;

  // Interface through which messages can be sent to the client.
  protocol::ClientStub* client_stub_;

  // Used to track webrtc connection statistics.
  rtc::scoped_refptr<webrtc::StatsObserver> stats_observer_;

  // Used to repeatedly poll stats from the |peer_connection_|.
  base::RepeatingTimer<CastExtensionSession> stats_polling_timer_;

  // True if a PeerConnection offer from the client has been received. This
  // necessarily means that the host is not the caller in this attempted
  // peer connection.
  bool received_offer_;

  // True if the webrtc::ScreenCapturer has been grabbed through the
  // OnCreateVideoCapturer() callback.
  bool has_grabbed_capturer_;

  // PeerConnection signaling and worker threads created from
  // JingleThreadWrappers. Each is created by calling
  // jingle_glue::EnsureForCurrentMessageLoop() and thus deletes itself
  // automatically when the associated MessageLoop is destroyed.
  rtc::Thread* signaling_thread_wrapper_;
  rtc::Thread* worker_thread_wrapper_;

  // Worker thread that is wrapped to create |worker_thread_wrapper_|.
  base::Thread worker_thread_;

  DISALLOW_COPY_AND_ASSIGN(CastExtensionSession);
};

}  // namespace remoting

#endif  // REMOTING_HOST_CAST_EXTENSION_SESSION_H_
