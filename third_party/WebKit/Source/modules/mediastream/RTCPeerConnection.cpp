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

#include "config.h"

#include "modules/mediastream/RTCPeerConnection.h"

#include "bindings/v8/ArrayValue.h"
#include "bindings/v8/ExceptionState.h"
#include "core/dom/Document.h"
#include "core/dom/Event.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ScriptExecutionContext.h"
#include "core/html/VoidCallback.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/page/Frame.h"
#include "core/platform/mediastream/RTCConfiguration.h"
#include "core/platform/mediastream/RTCDataChannelHandler.h"
#include "modules/mediastream/MediaConstraintsImpl.h"
#include "modules/mediastream/MediaStreamEvent.h"
#include "modules/mediastream/RTCDTMFSender.h"
#include "modules/mediastream/RTCDataChannel.h"
#include "modules/mediastream/RTCDataChannelEvent.h"
#include "modules/mediastream/RTCErrorCallback.h"
#include "modules/mediastream/RTCIceCandidate.h"
#include "modules/mediastream/RTCIceCandidateEvent.h"
#include "modules/mediastream/RTCSessionDescription.h"
#include "modules/mediastream/RTCSessionDescriptionCallback.h"
#include "modules/mediastream/RTCSessionDescriptionRequestImpl.h"
#include "modules/mediastream/RTCStatsCallback.h"
#include "modules/mediastream/RTCStatsRequestImpl.h"
#include "modules/mediastream/RTCVoidRequestImpl.h"
#include "public/platform/WebRTCDataChannelInit.h"
#include "public/platform/WebRTCICECandidate.h"
#include "public/platform/WebRTCSessionDescription.h"

namespace WebCore {

PassRefPtr<RTCConfiguration> RTCPeerConnection::parseConfiguration(const Dictionary& configuration, ExceptionState& es)
{
    if (configuration.isUndefinedOrNull())
        return 0;

    ArrayValue iceServers;
    bool ok = configuration.get("iceServers", iceServers);
    if (!ok || iceServers.isUndefinedOrNull()) {
        es.throwDOMException(TypeMismatchError);
        return 0;
    }

    size_t numberOfServers;
    ok = iceServers.length(numberOfServers);
    if (!ok) {
        es.throwDOMException(TypeMismatchError);
        return 0;
    }

    RefPtr<RTCConfiguration> rtcConfiguration = RTCConfiguration::create();

    for (size_t i = 0; i < numberOfServers; ++i) {
        Dictionary iceServer;
        ok = iceServers.get(i, iceServer);
        if (!ok) {
            es.throwDOMException(TypeMismatchError);
            return 0;
        }

        String urlString, username, credential;
        ok = iceServer.get("url", urlString);
        if (!ok) {
            es.throwDOMException(TypeMismatchError);
            return 0;
        }
        KURL url(KURL(), urlString);
        if (!url.isValid() || !(url.protocolIs("turn") || url.protocolIs("turns") || url.protocolIs("stun"))) {
            es.throwDOMException(TypeMismatchError);
            return 0;
        }

        iceServer.get("username", username);
        iceServer.get("credential", credential);

        rtcConfiguration->appendServer(RTCIceServer::create(url, username, credential));
    }

    return rtcConfiguration.release();
}

PassRefPtr<RTCPeerConnection> RTCPeerConnection::create(ScriptExecutionContext* context, const Dictionary& rtcConfiguration, const Dictionary& mediaConstraints, ExceptionState& es)
{
    RefPtr<RTCConfiguration> configuration = parseConfiguration(rtcConfiguration, es);
    if (es.hadException())
        return 0;

    RefPtr<MediaConstraints> constraints = MediaConstraintsImpl::create(mediaConstraints, es);
    if (es.hadException())
        return 0;

    RefPtr<RTCPeerConnection> peerConnection = adoptRef(new RTCPeerConnection(context, configuration.release(), constraints.release(), es));
    peerConnection->suspendIfNeeded();
    if (es.hadException())
        return 0;

    return peerConnection.release();
}

RTCPeerConnection::RTCPeerConnection(ScriptExecutionContext* context, PassRefPtr<RTCConfiguration> configuration, PassRefPtr<MediaConstraints> constraints, ExceptionState& es)
    : ActiveDOMObject(context)
    , m_signalingState(SignalingStateStable)
    , m_iceGatheringState(IceGatheringStateNew)
    , m_iceConnectionState(IceConnectionStateNew)
    , m_scheduledEventTimer(this, &RTCPeerConnection::scheduledEventTimerFired)
    , m_stopped(false)
{
    ScriptWrappable::init(this);
    Document* document = toDocument(scriptExecutionContext());

    if (!document->frame()) {
        es.throwDOMException(NotSupportedError);
        return;
    }

    m_peerHandler = RTCPeerConnectionHandler::create(this);
    if (!m_peerHandler) {
        es.throwDOMException(NotSupportedError);
        return;
    }

    document->frame()->loader()->client()->dispatchWillStartUsingPeerConnectionHandler(m_peerHandler.get());

    if (!m_peerHandler->initialize(configuration, constraints)) {
        es.throwDOMException(NotSupportedError);
        return;
    }
}

RTCPeerConnection::~RTCPeerConnection()
{
    stop();
}

void RTCPeerConnection::createOffer(PassRefPtr<RTCSessionDescriptionCallback> successCallback, PassRefPtr<RTCErrorCallback> errorCallback, const Dictionary& mediaConstraints, ExceptionState& es)
{
    if (m_signalingState == SignalingStateClosed) {
        es.throwDOMException(InvalidStateError);
        return;
    }

    if (!successCallback) {
        es.throwDOMException(TypeMismatchError);
        return;
    }

    RefPtr<MediaConstraints> constraints = MediaConstraintsImpl::create(mediaConstraints, es);
    if (es.hadException())
        return;

    RefPtr<RTCSessionDescriptionRequestImpl> request = RTCSessionDescriptionRequestImpl::create(scriptExecutionContext(), successCallback, errorCallback);
    m_peerHandler->createOffer(request.release(), constraints);
}

void RTCPeerConnection::createAnswer(PassRefPtr<RTCSessionDescriptionCallback> successCallback, PassRefPtr<RTCErrorCallback> errorCallback, const Dictionary& mediaConstraints, ExceptionState& es)
{
    if (m_signalingState == SignalingStateClosed) {
        es.throwDOMException(InvalidStateError);
        return;
    }

    if (!successCallback) {
        es.throwDOMException(TypeMismatchError);
        return;
    }

    RefPtr<MediaConstraints> constraints = MediaConstraintsImpl::create(mediaConstraints, es);
    if (es.hadException())
        return;

    RefPtr<RTCSessionDescriptionRequestImpl> request = RTCSessionDescriptionRequestImpl::create(scriptExecutionContext(), successCallback, errorCallback);
    m_peerHandler->createAnswer(request.release(), constraints.release());
}

void RTCPeerConnection::setLocalDescription(PassRefPtr<RTCSessionDescription> prpSessionDescription, PassRefPtr<VoidCallback> successCallback, PassRefPtr<RTCErrorCallback> errorCallback, ExceptionState& es)
{
    if (m_signalingState == SignalingStateClosed) {
        es.throwDOMException(InvalidStateError);
        return;
    }

    RefPtr<RTCSessionDescription> sessionDescription = prpSessionDescription;
    if (!sessionDescription) {
        es.throwDOMException(TypeMismatchError);
        return;
    }

    RefPtr<RTCVoidRequestImpl> request = RTCVoidRequestImpl::create(scriptExecutionContext(), successCallback, errorCallback);
    m_peerHandler->setLocalDescription(request.release(), sessionDescription->webSessionDescription());
}

PassRefPtr<RTCSessionDescription> RTCPeerConnection::localDescription(ExceptionState& es)
{
    WebKit::WebRTCSessionDescription webSessionDescription = m_peerHandler->localDescription();
    if (webSessionDescription.isNull())
        return 0;

    RefPtr<RTCSessionDescription> sessionDescription = RTCSessionDescription::create(webSessionDescription);
    return sessionDescription.release();
}

void RTCPeerConnection::setRemoteDescription(PassRefPtr<RTCSessionDescription> prpSessionDescription, PassRefPtr<VoidCallback> successCallback, PassRefPtr<RTCErrorCallback> errorCallback, ExceptionState& es)
{
    if (m_signalingState == SignalingStateClosed) {
        es.throwDOMException(InvalidStateError);
        return;
    }

    RefPtr<RTCSessionDescription> sessionDescription = prpSessionDescription;
    if (!sessionDescription) {
        es.throwDOMException(TypeMismatchError);
        return;
    }

    RefPtr<RTCVoidRequestImpl> request = RTCVoidRequestImpl::create(scriptExecutionContext(), successCallback, errorCallback);
    m_peerHandler->setRemoteDescription(request.release(), sessionDescription->webSessionDescription());
}

PassRefPtr<RTCSessionDescription> RTCPeerConnection::remoteDescription(ExceptionState& es)
{
    WebKit::WebRTCSessionDescription webSessionDescription = m_peerHandler->remoteDescription();
    if (webSessionDescription.isNull())
        return 0;

    RefPtr<RTCSessionDescription> desc = RTCSessionDescription::create(webSessionDescription);
    return desc.release();
}

void RTCPeerConnection::updateIce(const Dictionary& rtcConfiguration, const Dictionary& mediaConstraints, ExceptionState& es)
{
    if (m_signalingState == SignalingStateClosed) {
        es.throwDOMException(InvalidStateError);
        return;
    }

    RefPtr<RTCConfiguration> configuration = parseConfiguration(rtcConfiguration, es);
    if (es.hadException())
        return;

    RefPtr<MediaConstraints> constraints = MediaConstraintsImpl::create(mediaConstraints, es);
    if (es.hadException())
        return;

    bool valid = m_peerHandler->updateIce(configuration, constraints);
    if (!valid)
        es.throwDOMException(SyntaxError);
}

void RTCPeerConnection::addIceCandidate(RTCIceCandidate* iceCandidate, ExceptionState& es)
{
    if (m_signalingState == SignalingStateClosed) {
        es.throwDOMException(InvalidStateError);
        return;
    }

    if (!iceCandidate) {
        es.throwDOMException(TypeMismatchError);
        return;
    }

    bool valid = m_peerHandler->addIceCandidate(iceCandidate->webCandidate());
    if (!valid)
        es.throwDOMException(SyntaxError);
}

void RTCPeerConnection::addIceCandidate(RTCIceCandidate* iceCandidate, PassRefPtr<VoidCallback> successCallback, PassRefPtr<RTCErrorCallback> errorCallback, ExceptionState& es)
{
    if (m_signalingState == SignalingStateClosed) {
        es.throwDOMException(InvalidStateError);
        return;
    }

    if (!iceCandidate || !successCallback || !errorCallback) {
        es.throwDOMException(TypeMismatchError);
        return;
    }

    RefPtr<RTCVoidRequestImpl> request = RTCVoidRequestImpl::create(scriptExecutionContext(), successCallback, errorCallback);

    bool implemented = m_peerHandler->addIceCandidate(request.release(), iceCandidate->webCandidate());
    if (!implemented)
        es.throwDOMException(NotSupportedError);
}

String RTCPeerConnection::signalingState() const
{
    switch (m_signalingState) {
    case SignalingStateStable:
        return "stable";
    case SignalingStateHaveLocalOffer:
        return "have-local-offer";
    case SignalingStateHaveRemoteOffer:
        return "have-remote-offer";
    case SignalingStateHaveLocalPrAnswer:
        return "have-local-pranswer";
    case SignalingStateHaveRemotePrAnswer:
        return "have-remote-pranswer";
    case SignalingStateClosed:
        return "closed";
    }

    ASSERT_NOT_REACHED();
    return String();
}

String RTCPeerConnection::iceGatheringState() const
{
    switch (m_iceGatheringState) {
    case IceGatheringStateNew:
        return "new";
    case IceGatheringStateGathering:
        return "gathering";
    case IceGatheringStateComplete:
        return "complete";
    }

    ASSERT_NOT_REACHED();
    return String();
}

String RTCPeerConnection::iceConnectionState() const
{
    switch (m_iceConnectionState) {
    case IceConnectionStateNew:
        return "new";
    case IceConnectionStateChecking:
        return "checking";
    case IceConnectionStateConnected:
        return "connected";
    case IceConnectionStateCompleted:
        return "completed";
    case IceConnectionStateFailed:
        return "failed";
    case IceConnectionStateDisconnected:
        return "disconnected";
    case IceConnectionStateClosed:
        return "closed";
    }

    ASSERT_NOT_REACHED();
    return String();
}

void RTCPeerConnection::addStream(PassRefPtr<MediaStream> prpStream, const Dictionary& mediaConstraints, ExceptionState& es)
{
    if (m_signalingState == SignalingStateClosed) {
        es.throwDOMException(InvalidStateError);
        return;
    }

    RefPtr<MediaStream> stream = prpStream;
    if (!stream) {
        es.throwDOMException(TypeMismatchError);
        return;
    }

    if (m_localStreams.contains(stream))
        return;

    RefPtr<MediaConstraints> constraints = MediaConstraintsImpl::create(mediaConstraints, es);
    if (es.hadException())
        return;

    m_localStreams.append(stream);

    bool valid = m_peerHandler->addStream(stream->descriptor(), constraints);
    if (!valid)
        es.throwDOMException(SyntaxError);
}

void RTCPeerConnection::removeStream(PassRefPtr<MediaStream> prpStream, ExceptionState& es)
{
    if (m_signalingState == SignalingStateClosed) {
        es.throwDOMException(InvalidStateError);
        return;
    }

    if (!prpStream) {
        es.throwDOMException(TypeMismatchError);
        return;
    }

    RefPtr<MediaStream> stream = prpStream;

    size_t pos = m_localStreams.find(stream);
    if (pos == notFound)
        return;

    m_localStreams.remove(pos);

    m_peerHandler->removeStream(stream->descriptor());
}

MediaStreamVector RTCPeerConnection::getLocalStreams() const
{
    return m_localStreams;
}

MediaStreamVector RTCPeerConnection::getRemoteStreams() const
{
    return m_remoteStreams;
}

MediaStream* RTCPeerConnection::getStreamById(const String& streamId)
{
    for (MediaStreamVector::iterator iter = m_localStreams.begin(); iter != m_localStreams.end(); ++iter) {
        if ((*iter)->id() == streamId)
            return iter->get();
    }

    for (MediaStreamVector::iterator iter = m_remoteStreams.begin(); iter != m_remoteStreams.end(); ++iter) {
        if ((*iter)->id() == streamId)
            return iter->get();
    }

    return 0;
}

void RTCPeerConnection::getStats(PassRefPtr<RTCStatsCallback> successCallback, PassRefPtr<MediaStreamTrack> selector)
{
    RefPtr<RTCStatsRequestImpl> statsRequest = RTCStatsRequestImpl::create(scriptExecutionContext(), successCallback, selector);
    // FIXME: Add passing selector as part of the statsRequest.
    m_peerHandler->getStats(statsRequest.release());
}

PassRefPtr<RTCDataChannel> RTCPeerConnection::createDataChannel(String label, const Dictionary& options, ExceptionState& es)
{
    if (m_signalingState == SignalingStateClosed) {
        es.throwDOMException(InvalidStateError);
        return 0;
    }

    WebKit::WebRTCDataChannelInit init;
    options.get("ordered", init.ordered);
    options.get("negotiated", init.negotiated);

    unsigned short value = 0;
    if (options.get("id", value))
        init.id = value;
    if (options.get("maxRetransmits", value))
        init.maxRetransmits = value;
    if (options.get("maxRetransmitTime", value))
        init.maxRetransmitTime = value;

    String protocolString;
    options.get("protocol", protocolString);
    init.protocol = protocolString;

    RefPtr<RTCDataChannel> channel = RTCDataChannel::create(scriptExecutionContext(), m_peerHandler.get(), label, init, es);
    if (es.hadException())
        return 0;
    m_dataChannels.append(channel);
    return channel.release();
}

bool RTCPeerConnection::hasLocalStreamWithTrackId(const String& trackId)
{
    for (MediaStreamVector::iterator iter = m_localStreams.begin(); iter != m_localStreams.end(); ++iter) {
        if ((*iter)->getTrackById(trackId))
            return true;
    }
    return false;
}

PassRefPtr<RTCDTMFSender> RTCPeerConnection::createDTMFSender(PassRefPtr<MediaStreamTrack> prpTrack, ExceptionState& es)
{
    if (m_signalingState == SignalingStateClosed) {
        es.throwDOMException(InvalidStateError);
        return 0;
    }

    if (!prpTrack) {
        es.throwTypeError();
        return 0;
    }

    RefPtr<MediaStreamTrack> track = prpTrack;

    if (!hasLocalStreamWithTrackId(track->id())) {
        es.throwDOMException(SyntaxError);
        return 0;
    }

    RefPtr<RTCDTMFSender> dtmfSender = RTCDTMFSender::create(scriptExecutionContext(), m_peerHandler.get(), track.release(), es);
    if (es.hadException())
        return 0;
    return dtmfSender.release();
}

void RTCPeerConnection::close(ExceptionState& es)
{
    if (m_signalingState == SignalingStateClosed) {
        es.throwDOMException(InvalidStateError);
        return;
    }

    m_peerHandler->stop();

    changeIceConnectionState(IceConnectionStateClosed);
    changeIceGatheringState(IceGatheringStateComplete);
    changeSignalingState(SignalingStateClosed);
}

void RTCPeerConnection::negotiationNeeded()
{
    scheduleDispatchEvent(Event::create(eventNames().negotiationneededEvent));
}

void RTCPeerConnection::didGenerateIceCandidate(WebKit::WebRTCICECandidate webCandidate)
{
    ASSERT(scriptExecutionContext()->isContextThread());
    if (webCandidate.isNull())
        scheduleDispatchEvent(RTCIceCandidateEvent::create(false, false, 0));
    else {
        RefPtr<RTCIceCandidate> iceCandidate = RTCIceCandidate::create(webCandidate);
        scheduleDispatchEvent(RTCIceCandidateEvent::create(false, false, iceCandidate.release()));
    }
}

void RTCPeerConnection::didChangeSignalingState(SignalingState newState)
{
    ASSERT(scriptExecutionContext()->isContextThread());
    changeSignalingState(newState);
}

void RTCPeerConnection::didChangeIceGatheringState(IceGatheringState newState)
{
    ASSERT(scriptExecutionContext()->isContextThread());
    changeIceGatheringState(newState);
}

void RTCPeerConnection::didChangeIceConnectionState(IceConnectionState newState)
{
    ASSERT(scriptExecutionContext()->isContextThread());
    changeIceConnectionState(newState);
}

void RTCPeerConnection::didAddRemoteStream(PassRefPtr<MediaStreamDescriptor> streamDescriptor)
{
    ASSERT(scriptExecutionContext()->isContextThread());

    if (m_signalingState == SignalingStateClosed)
        return;

    RefPtr<MediaStream> stream = MediaStream::create(scriptExecutionContext(), streamDescriptor);
    m_remoteStreams.append(stream);

    scheduleDispatchEvent(MediaStreamEvent::create(eventNames().addstreamEvent, false, false, stream.release()));
}

void RTCPeerConnection::didRemoveRemoteStream(MediaStreamDescriptor* streamDescriptor)
{
    ASSERT(scriptExecutionContext()->isContextThread());
    ASSERT(streamDescriptor->client());

    RefPtr<MediaStream> stream = static_cast<MediaStream*>(streamDescriptor->client());
    stream->streamEnded();

    if (m_signalingState == SignalingStateClosed)
        return;

    size_t pos = m_remoteStreams.find(stream);
    ASSERT(pos != notFound);
    m_remoteStreams.remove(pos);

    scheduleDispatchEvent(MediaStreamEvent::create(eventNames().removestreamEvent, false, false, stream.release()));
}

void RTCPeerConnection::didAddRemoteDataChannel(PassOwnPtr<RTCDataChannelHandler> handler)
{
    ASSERT(scriptExecutionContext()->isContextThread());

    if (m_signalingState == SignalingStateClosed)
        return;

    RefPtr<RTCDataChannel> channel = RTCDataChannel::create(scriptExecutionContext(), handler);
    m_dataChannels.append(channel);

    scheduleDispatchEvent(RTCDataChannelEvent::create(eventNames().datachannelEvent, false, false, channel.release()));
}

const AtomicString& RTCPeerConnection::interfaceName() const
{
    return eventNames().interfaceForRTCPeerConnection;
}

ScriptExecutionContext* RTCPeerConnection::scriptExecutionContext() const
{
    return ActiveDOMObject::scriptExecutionContext();
}

void RTCPeerConnection::stop()
{
    if (m_stopped)
        return;

    m_stopped = true;
    m_iceConnectionState = IceConnectionStateClosed;
    m_signalingState = SignalingStateClosed;

    Vector<RefPtr<RTCDataChannel> >::iterator i = m_dataChannels.begin();
    for (; i != m_dataChannels.end(); ++i)
        (*i)->stop();
}

EventTargetData* RTCPeerConnection::eventTargetData()
{
    return &m_eventTargetData;
}

EventTargetData* RTCPeerConnection::ensureEventTargetData()
{
    return &m_eventTargetData;
}

void RTCPeerConnection::changeSignalingState(SignalingState signalingState)
{
    if (m_signalingState != SignalingStateClosed && m_signalingState != signalingState) {
        m_signalingState = signalingState;
        scheduleDispatchEvent(Event::create(eventNames().signalingstatechangeEvent));
    }
}

void RTCPeerConnection::changeIceGatheringState(IceGatheringState iceGatheringState)
{
    m_iceGatheringState = iceGatheringState;
}

void RTCPeerConnection::changeIceConnectionState(IceConnectionState iceConnectionState)
{
    if (m_iceConnectionState != IceConnectionStateClosed && m_iceConnectionState != iceConnectionState) {
        m_iceConnectionState = iceConnectionState;
        scheduleDispatchEvent(Event::create(eventNames().iceconnectionstatechangeEvent));
    }
}

void RTCPeerConnection::scheduleDispatchEvent(PassRefPtr<Event> event)
{
    m_scheduledEvents.append(event);

    if (!m_scheduledEventTimer.isActive())
        m_scheduledEventTimer.startOneShot(0);
}

void RTCPeerConnection::scheduledEventTimerFired(Timer<RTCPeerConnection>*)
{
    if (m_stopped)
        return;

    Vector<RefPtr<Event> > events;
    events.swap(m_scheduledEvents);

    Vector<RefPtr<Event> >::iterator it = events.begin();
    for (; it != events.end(); ++it)
        dispatchEvent((*it).release());

    events.clear();
}

} // namespace WebCore
