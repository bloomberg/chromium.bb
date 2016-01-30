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

#include "bindings/core/v8/Dictionary.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "core/dom/ActiveDOMObject.h"
#include "modules/EventTargetModules.h"
#include "modules/crypto/NormalizeAlgorithm.h"
#include "modules/mediastream/MediaStream.h"
#include "modules/mediastream/RTCIceCandidate.h"
#include "platform/AsyncMethodRunner.h"
#include "public/platform/WebMediaConstraints.h"
#include "public/platform/WebRTCPeerConnectionHandler.h"
#include "public/platform/WebRTCPeerConnectionHandlerClient.h"

namespace blink {
class ExceptionState;
class MediaStreamTrack;
class RTCConfiguration;
class RTCDTMFSender;
class RTCDataChannel;
class RTCErrorCallback;
class RTCOfferOptions;
class RTCSessionDescription;
class RTCSessionDescriptionCallback;
class RTCStatsCallback;
class VoidCallback;

class RTCPeerConnection final
    : public RefCountedGarbageCollectedEventTargetWithInlineData<RTCPeerConnection>
    , public WebRTCPeerConnectionHandlerClient
    , public ActiveDOMObject {
    REFCOUNTED_GARBAGE_COLLECTED_EVENT_TARGET(RTCPeerConnection);
    DEFINE_WRAPPERTYPEINFO();
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(RTCPeerConnection);
public:
    // TODO(hbos): Create with expired RTCCertificate should fail, see crbug.com/565278.
    static RTCPeerConnection* create(ExecutionContext*, const Dictionary&, const Dictionary&, ExceptionState&);
    ~RTCPeerConnection() override;

    void createOffer(ExecutionContext*, RTCSessionDescriptionCallback*, RTCErrorCallback*, const Dictionary&, ExceptionState&);

    void createAnswer(ExecutionContext*, RTCSessionDescriptionCallback*, RTCErrorCallback*, const Dictionary&, ExceptionState&);

    void setLocalDescription(ExecutionContext*, RTCSessionDescription*, VoidCallback*, RTCErrorCallback*, ExceptionState&);
    RTCSessionDescription* localDescription();

    void setRemoteDescription(ExecutionContext*, RTCSessionDescription*, VoidCallback*, RTCErrorCallback*, ExceptionState&);
    RTCSessionDescription* remoteDescription();

    String signalingState() const;

    void updateIce(ExecutionContext*, const Dictionary& rtcConfiguration, const Dictionary& mediaConstraints, ExceptionState&);

    // Certificate management
    // http://w3c.github.io/webrtc-pc/#sec.cert-mgmt
    static ScriptPromise generateCertificate(ScriptState*, const AlgorithmIdentifier& keygenAlgorithm, ExceptionState&);

    // DEPRECATED
    void addIceCandidate(RTCIceCandidate*, ExceptionState&);

    void addIceCandidate(RTCIceCandidate*, VoidCallback*, RTCErrorCallback*, ExceptionState&);

    String iceGatheringState() const;

    String iceConnectionState() const;

    MediaStreamVector getLocalStreams() const;

    MediaStreamVector getRemoteStreams() const;

    MediaStream* getStreamById(const String& streamId);

    void addStream(ExecutionContext*, MediaStream*, const Dictionary& mediaConstraints, ExceptionState&);

    void removeStream(MediaStream*, ExceptionState&);

    void getStats(ExecutionContext*, RTCStatsCallback* successCallback, MediaStreamTrack* selector);

    RTCDataChannel* createDataChannel(String label, const Dictionary& dataChannelDict, ExceptionState&);

    RTCDTMFSender* createDTMFSender(MediaStreamTrack*, ExceptionState&);

    void close(ExceptionState&);

    // We allow getStats after close, but not other calls or callbacks.
    bool shouldFireDefaultCallbacks() { return !m_closed && !m_stopped; }
    bool shouldFireGetStatsCallback() { return !m_stopped; }

    DEFINE_ATTRIBUTE_EVENT_LISTENER(negotiationneeded);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(icecandidate);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(signalingstatechange);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(addstream);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(removestream);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(iceconnectionstatechange);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(datachannel);

    // WebRTCPeerConnectionHandlerClient
    void negotiationNeeded() override;
    void didGenerateICECandidate(const WebRTCICECandidate&) override;
    void didChangeSignalingState(SignalingState) override;
    void didChangeICEGatheringState(ICEGatheringState) override;
    void didChangeICEConnectionState(ICEConnectionState) override;
    void didAddRemoteStream(const WebMediaStream&) override;
    void didRemoveRemoteStream(const WebMediaStream&) override;
    void didAddRemoteDataChannel(WebRTCDataChannelHandler*) override;
    void releasePeerConnectionHandler() override;
    void closePeerConnection() override;

    // EventTarget
    const AtomicString& interfaceName() const override;
    ExecutionContext* executionContext() const override;

    // ActiveDOMObject
    void suspend() override;
    void resume() override;
    void stop() override;
    // We keep the this object alive until either stopped or closed.
    bool hasPendingActivity() const override
    {
        return !m_closed && !m_stopped;
    }

    // Oilpan: need to eagerly finalize m_peerHandler
    EAGERLY_FINALIZE();
    DECLARE_VIRTUAL_TRACE();

private:
    typedef Function<bool()> BoolFunction;
    class EventWrapper : public GarbageCollectedFinalized<EventWrapper> {
    public:
        EventWrapper(PassRefPtrWillBeRawPtr<Event>, PassOwnPtr<BoolFunction>);
        // Returns true if |m_setupFunction| returns true or it is null.
        // |m_event| will only be fired if setup() returns true;
        bool setup();

        DECLARE_TRACE();

        RefPtrWillBeMember<Event> m_event;

    private:
        OwnPtr<BoolFunction> m_setupFunction;
    };

    RTCPeerConnection(ExecutionContext*, RTCConfiguration*, WebMediaConstraints, ExceptionState&);

    static RTCConfiguration* parseConfiguration(const Dictionary&, ExceptionState&);
    static RTCOfferOptions* parseOfferOptions(const Dictionary&, ExceptionState&);

    void scheduleDispatchEvent(PassRefPtrWillBeRawPtr<Event>);
    void scheduleDispatchEvent(PassRefPtrWillBeRawPtr<Event>, PassOwnPtr<BoolFunction>);
    void dispatchScheduledEvent();
    bool hasLocalStreamWithTrackId(const String& trackId);

    void changeSignalingState(WebRTCPeerConnectionHandlerClient::SignalingState);
    void changeIceGatheringState(WebRTCPeerConnectionHandlerClient::ICEGatheringState);
    // Changes the state immediately; does not fire an event.
    // Returns true if the state was changed.
    bool setIceConnectionState(WebRTCPeerConnectionHandlerClient::ICEConnectionState);
    // Changes the state asynchronously and fires an event immediately after changing the state.
    void changeIceConnectionState(WebRTCPeerConnectionHandlerClient::ICEConnectionState);

    void closeInternal();

    SignalingState m_signalingState;
    ICEGatheringState m_iceGatheringState;
    ICEConnectionState m_iceConnectionState;

    MediaStreamVector m_localStreams;
    MediaStreamVector m_remoteStreams;

    OwnPtr<WebRTCPeerConnectionHandler> m_peerHandler;

    Member<AsyncMethodRunner<RTCPeerConnection>> m_dispatchScheduledEventRunner;
    HeapVector<Member<EventWrapper>> m_scheduledEvents;

    bool m_stopped;
    bool m_closed;
};

} // namespace blink

#endif // RTCPeerConnection_h
