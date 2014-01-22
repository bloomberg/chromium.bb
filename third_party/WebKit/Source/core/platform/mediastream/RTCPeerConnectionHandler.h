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

#ifndef RTCPeerConnectionHandler_h
#define RTCPeerConnectionHandler_h

#include "core/platform/mediastream/MediaStreamDescriptor.h"
#include "public/platform/WebRTCPeerConnectionHandler.h"
#include "public/platform/WebRTCPeerConnectionHandlerClient.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassRefPtr.h"

namespace blink {
class WebMediaStream;
class WebRTCDataChannelHandler;
class WebRTCICECandidate;
class WebRTCSessionDescription;
struct WebRTCDataChannelInit;
}

namespace WebCore {

class MediaStreamComponent;
class RTCConfiguration;
class RTCDTMFSenderHandler;
class RTCPeerConnectionHandlerClient;
class RTCSessionDescriptionRequest;
class RTCStatsRequest;
class RTCVoidRequest;

class RTCPeerConnectionHandler FINAL : public blink::WebRTCPeerConnectionHandlerClient {
public:
    static PassOwnPtr<RTCPeerConnectionHandler> create(RTCPeerConnectionHandlerClient*);
    virtual ~RTCPeerConnectionHandler();

    bool createWebHandler();

    bool initialize(PassRefPtr<RTCConfiguration>, blink::WebMediaConstraints);

    void createOffer(PassRefPtr<RTCSessionDescriptionRequest>, blink::WebMediaConstraints);
    void createAnswer(PassRefPtr<RTCSessionDescriptionRequest>, blink::WebMediaConstraints);
    void setLocalDescription(PassRefPtr<RTCVoidRequest>, blink::WebRTCSessionDescription);
    void setRemoteDescription(PassRefPtr<RTCVoidRequest>, blink::WebRTCSessionDescription);
    blink::WebRTCSessionDescription localDescription();
    blink::WebRTCSessionDescription remoteDescription();
    bool updateIce(PassRefPtr<RTCConfiguration>, blink::WebMediaConstraints);

    // DEPRECATED
    bool addIceCandidate(blink::WebRTCICECandidate);

    bool addIceCandidate(PassRefPtr<RTCVoidRequest>, blink::WebRTCICECandidate);
    bool addStream(PassRefPtr<MediaStreamDescriptor>, blink::WebMediaConstraints);
    void removeStream(PassRefPtr<MediaStreamDescriptor>);
    void getStats(PassRefPtr<RTCStatsRequest>);
    PassOwnPtr<blink::WebRTCDataChannelHandler> createDataChannel(const String& label, const blink::WebRTCDataChannelInit&);
    PassOwnPtr<RTCDTMFSenderHandler> createDTMFSender(PassRefPtr<MediaStreamComponent>);
    void stop();

    // blink::WebRTCPeerConnectionHandlerClient implementation.
    virtual void negotiationNeeded() OVERRIDE;
    virtual void didGenerateICECandidate(const blink::WebRTCICECandidate&) OVERRIDE;
    virtual void didChangeSignalingState(blink::WebRTCPeerConnectionHandlerClient::SignalingState) OVERRIDE;
    virtual void didChangeICEGatheringState(blink::WebRTCPeerConnectionHandlerClient::ICEGatheringState) OVERRIDE;
    virtual void didChangeICEConnectionState(blink::WebRTCPeerConnectionHandlerClient::ICEConnectionState) OVERRIDE;
    virtual void didAddRemoteStream(const blink::WebMediaStream&) OVERRIDE;
    virtual void didRemoveRemoteStream(const blink::WebMediaStream&) OVERRIDE;
    virtual void didAddRemoteDataChannel(blink::WebRTCDataChannelHandler*) OVERRIDE;

    static blink::WebRTCPeerConnectionHandler* toWebRTCPeerConnectionHandler(RTCPeerConnectionHandler*);

private:
    explicit RTCPeerConnectionHandler(RTCPeerConnectionHandlerClient*);

    OwnPtr<blink::WebRTCPeerConnectionHandler> m_webHandler;
    RTCPeerConnectionHandlerClient* m_client;
};

} // namespace WebCore

#endif // RTCPeerConnectionHandler_h
