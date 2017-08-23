/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Google Inc. nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef RTCPeerConnection_h
#define RTCPeerConnection_h

#include <memory>
#include "bindings/core/v8/ScriptPromise.h"
#include "core/dom/SuspendableObject.h"
#include "modules/EventTargetModules.h"
#include "modules/crypto/NormalizeAlgorithm.h"
#include "modules/mediastream/MediaStream.h"
#include "modules/peerconnection/RTCIceCandidate.h"
#include "platform/AsyncMethodRunner.h"
#include "platform/WebFrameScheduler.h"
#include "platform/bindings/ActiveScriptWrappable.h"
#include "platform/heap/HeapAllocator.h"
#include "public/platform/WebMediaConstraints.h"
#include "public/platform/WebRTCPeerConnectionHandler.h"
#include "public/platform/WebRTCPeerConnectionHandlerClient.h"

namespace blink {
class ExceptionState;
class MediaStreamTrack;
class RTCAnswerOptions;
class RTCConfiguration;
class RTCDTMFSender;
class RTCDataChannel;
class RTCDataChannelInit;
class RTCIceCandidateInitOrRTCIceCandidate;
class RTCOfferOptions;
class RTCPeerConnectionErrorCallback;
class RTCPeerConnectionTest;
class RTCRtpReceiver;
class RTCRtpSender;
class RTCSessionDescription;
class RTCSessionDescriptionCallback;
class RTCSessionDescriptionInit;
class RTCStatsCallback;
class ScriptState;
class VoidCallback;
struct WebRTCConfiguration;

class MODULES_EXPORT RTCPeerConnection final
    : public EventTargetWithInlineData,
      public WebRTCPeerConnectionHandlerClient,
      public ActiveScriptWrappable<RTCPeerConnection>,
      public SuspendableObject,
      public MediaStreamObserver {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(RTCPeerConnection);
  USING_PRE_FINALIZER(RTCPeerConnection, Dispose);

 public:
  static RTCPeerConnection* Create(ExecutionContext*,
                                   const RTCConfiguration&,
                                   const Dictionary&,
                                   ExceptionState&);
  ~RTCPeerConnection() override;

  ScriptPromise createOffer(ScriptState*, const RTCOfferOptions&);
  ScriptPromise createOffer(ScriptState*,
                            RTCSessionDescriptionCallback*,
                            RTCPeerConnectionErrorCallback*,
                            const Dictionary&,
                            ExceptionState&);

  ScriptPromise createAnswer(ScriptState*, const RTCAnswerOptions&);
  ScriptPromise createAnswer(ScriptState*,
                             RTCSessionDescriptionCallback*,
                             RTCPeerConnectionErrorCallback*,
                             const Dictionary&);

  ScriptPromise setLocalDescription(ScriptState*,
                                    const RTCSessionDescriptionInit&);
  ScriptPromise setLocalDescription(ScriptState*,
                                    const RTCSessionDescriptionInit&,
                                    VoidCallback*,
                                    RTCPeerConnectionErrorCallback*);
  RTCSessionDescription* localDescription();

  ScriptPromise setRemoteDescription(ScriptState*,
                                     const RTCSessionDescriptionInit&);
  ScriptPromise setRemoteDescription(ScriptState*,
                                     const RTCSessionDescriptionInit&,
                                     VoidCallback*,
                                     RTCPeerConnectionErrorCallback*);
  RTCSessionDescription* remoteDescription();

  String signalingState() const;

  void setConfiguration(ScriptState*, const RTCConfiguration&, ExceptionState&);

  // Certificate management
  // http://w3c.github.io/webrtc-pc/#sec.cert-mgmt
  static ScriptPromise generateCertificate(
      ScriptState*,
      const AlgorithmIdentifier& keygen_algorithm,
      ExceptionState&);

  ScriptPromise addIceCandidate(ScriptState*,
                                const RTCIceCandidateInitOrRTCIceCandidate&);
  ScriptPromise addIceCandidate(ScriptState*,
                                const RTCIceCandidateInitOrRTCIceCandidate&,
                                VoidCallback*,
                                RTCPeerConnectionErrorCallback*);

  String iceGatheringState() const;

  String iceConnectionState() const;

  MediaStreamVector getLocalStreams() const;

  MediaStreamVector getRemoteStreams() const;

  void addStream(ScriptState*,
                 MediaStream*,
                 const Dictionary& media_constraints,
                 ExceptionState&);

  void removeStream(MediaStream*, ExceptionState&);

  ScriptPromise getStats(ScriptState*,
                         RTCStatsCallback* success_callback,
                         MediaStreamTrack* selector = nullptr);
  ScriptPromise getStats(ScriptState*);

  HeapVector<Member<RTCRtpSender>> getSenders();
  HeapVector<Member<RTCRtpReceiver>> getReceivers();
  RTCRtpSender* addTrack(MediaStreamTrack*, MediaStreamVector, ExceptionState&);
  void removeTrack(RTCRtpSender*, ExceptionState&);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(track);

  RTCDataChannel* createDataChannel(ScriptState*,
                                    String label,
                                    const RTCDataChannelInit&,
                                    ExceptionState&);

  RTCDTMFSender* createDTMFSender(MediaStreamTrack*, ExceptionState&);

  void close();

  // We allow getStats after close, but not other calls or callbacks.
  bool ShouldFireDefaultCallbacks() { return !closed_ && !stopped_; }
  bool ShouldFireGetStatsCallback() { return !stopped_; }

  DEFINE_ATTRIBUTE_EVENT_LISTENER(negotiationneeded);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(icecandidate);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(signalingstatechange);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(addstream);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(removestream);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(iceconnectionstatechange);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(icegatheringstatechange);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(datachannel);

  // MediaStreamObserver
  void OnStreamAddTrack(MediaStream*, MediaStreamTrack*) override;
  void OnStreamRemoveTrack(MediaStream*, MediaStreamTrack*) override;

  // WebRTCPeerConnectionHandlerClient
  void NegotiationNeeded() override;
  void DidGenerateICECandidate(const WebRTCICECandidate&) override;
  void DidChangeSignalingState(SignalingState) override;
  void DidChangeICEGatheringState(ICEGatheringState) override;
  void DidChangeICEConnectionState(ICEConnectionState) override;
  void DidAddRemoteStream(
      const WebMediaStream&,
      WebVector<std::unique_ptr<WebRTCRtpReceiver>>*) override;
  void DidRemoveRemoteStream(const WebMediaStream&) override;
  void DidAddRemoteDataChannel(WebRTCDataChannelHandler*) override;
  void ReleasePeerConnectionHandler() override;
  void ClosePeerConnection() override;

  // EventTarget
  const AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override;

  // SuspendableObject
  void Suspend() override;
  void Resume() override;
  void ContextDestroyed(ExecutionContext*) override;

  // ScriptWrappable
  // We keep the this object alive until either stopped or closed.
  bool HasPendingActivity() const final { return !closed_ && !stopped_; }

  DECLARE_VIRTUAL_TRACE();

 private:
  FRIEND_TEST_ALL_PREFIXES(RTCPeerConnectionTest, GetAudioTrack);
  FRIEND_TEST_ALL_PREFIXES(RTCPeerConnectionTest, GetVideoTrack);
  FRIEND_TEST_ALL_PREFIXES(RTCPeerConnectionTest, GetAudioAndVideoTrack);
  FRIEND_TEST_ALL_PREFIXES(RTCPeerConnectionTest, GetTrackRemoveStreamAndGCAll);
  FRIEND_TEST_ALL_PREFIXES(RTCPeerConnectionTest,
                           GetTrackRemoveStreamAndGCWithPersistentComponent);
  FRIEND_TEST_ALL_PREFIXES(RTCPeerConnectionTest,
                           GetTrackRemoveStreamAndGCWithPersistentStream);

  typedef Function<bool()> BoolFunction;
  class EventWrapper : public GarbageCollectedFinalized<EventWrapper> {
   public:
    EventWrapper(Event*, BoolFunction);
    // Returns true if |m_setupFunction| returns true or it is null.
    // |m_event| will only be fired if setup() returns true;
    bool Setup();

    DECLARE_TRACE();

    Member<Event> event_;

   private:
    BoolFunction setup_function_;
  };

  RTCPeerConnection(ExecutionContext*,
                    const WebRTCConfiguration&,
                    WebMediaConstraints,
                    ExceptionState&);
  void Dispose();

  void ScheduleDispatchEvent(Event*);
  void ScheduleDispatchEvent(Event*, BoolFunction);
  void DispatchScheduledEvent();
  MediaStreamTrack* GetTrack(const WebMediaStreamTrack&) const;
  RTCRtpReceiver* GetOrCreateRTCRtpReceiver(
      std::unique_ptr<WebRTCRtpReceiver> web_rtp_receiver);

  // The "Change" methods set the state asynchronously and fire the
  // corresponding event immediately after changing the state (if it was really
  // changed).
  //
  // The "Set" methods are called asynchronously by the "Change" methods, and
  // set the corresponding state without firing an event, returning true if the
  // state was really changed.
  //
  // This is done because the standard guarantees that state changes and the
  // corresponding events will happen in the same task; it shouldn't be
  // possible to, for example, end up with two "icegatheringstatechange" events
  // that are delayed somehow and cause the application to read a "complete"
  // gathering state twice, missing the "gathering" state in the middle.
  //
  // TODO(deadbeef): This wasn't done for the signaling state because it
  // resulted in a change to the order of the signaling state being updated
  // relative to the SetLocalDescription or SetRemoteDescription promise being
  // resolved. Some additional refactoring would be necessary to fix this; for
  // example, passing the new signaling state along with the SRD/SLD callbacks
  // as opposed to relying on a separate event.
  void ChangeSignalingState(WebRTCPeerConnectionHandlerClient::SignalingState);

  void ChangeIceGatheringState(
      WebRTCPeerConnectionHandlerClient::ICEGatheringState);
  bool SetIceGatheringState(
      WebRTCPeerConnectionHandlerClient::ICEGatheringState);

  void ChangeIceConnectionState(
      WebRTCPeerConnectionHandlerClient::ICEConnectionState);
  bool SetIceConnectionState(
      WebRTCPeerConnectionHandlerClient::ICEConnectionState);

  void CloseInternal();

  void RecordRapporMetrics();

  SignalingState signaling_state_;
  ICEGatheringState ice_gathering_state_;
  ICEConnectionState ice_connection_state_;

  MediaStreamVector local_streams_;
  MediaStreamVector remote_streams_;
  // A map containing any track that is in use by the peer connection. This
  // includes tracks of |local_streams_|, |remote_streams_|, |rtp_senders_| and
  // |rtp_receivers_|.
  HeapHashMap<WeakMember<MediaStreamComponent>, WeakMember<MediaStreamTrack>>
      tracks_;
  HeapHashMap<uintptr_t, Member<RTCRtpSender>> rtp_senders_;
  HeapHashMap<uintptr_t, Member<RTCRtpReceiver>> rtp_receivers_;

  std::unique_ptr<WebRTCPeerConnectionHandler> peer_handler_;

  Member<AsyncMethodRunner<RTCPeerConnection>> dispatch_scheduled_event_runner_;
  HeapVector<Member<EventWrapper>> scheduled_events_;

  // This handle notifies scheduler about an active connection associated
  // with a frame. Handle should be destroyed when connection is closed.
  std::unique_ptr<WebFrameScheduler::ActiveConnectionHandle>
      connection_handle_for_scheduler_;

  bool stopped_;
  bool closed_;

  bool has_data_channels_;  // For RAPPOR metrics
};

}  // namespace blink

#endif  // RTCPeerConnection_h
