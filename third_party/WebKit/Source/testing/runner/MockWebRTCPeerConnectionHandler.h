/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
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

#ifndef MockWebRTCPeerConnectionHandler_h
#define MockWebRTCPeerConnectionHandler_h

#include "TestCommon.h"
#include "public/platform/WebNonCopyable.h"
#include "public/platform/WebRTCPeerConnectionHandler.h"
#include "public/platform/WebRTCSessionDescription.h"
#include "public/platform/WebRTCSessionDescriptionRequest.h"
#include "public/platform/WebRTCStatsRequest.h"
#include "public/testing/WebTask.h"

namespace blink {
class WebRTCPeerConnectionHandlerClient;
};

namespace WebTestRunner {

class TestInterfaces;

class MockWebRTCPeerConnectionHandler : public blink::WebRTCPeerConnectionHandler, public blink::WebNonCopyable {
public:
    MockWebRTCPeerConnectionHandler(blink::WebRTCPeerConnectionHandlerClient*, TestInterfaces*);

    virtual bool initialize(const blink::WebRTCConfiguration&, const blink::WebMediaConstraints&) OVERRIDE;

    virtual void createOffer(const blink::WebRTCSessionDescriptionRequest&, const blink::WebMediaConstraints&) OVERRIDE;
    virtual void createAnswer(const blink::WebRTCSessionDescriptionRequest&, const blink::WebMediaConstraints&) OVERRIDE;
    virtual void setLocalDescription(const blink::WebRTCVoidRequest&, const blink::WebRTCSessionDescription&) OVERRIDE;
    virtual void setRemoteDescription(const blink::WebRTCVoidRequest&, const blink::WebRTCSessionDescription&) OVERRIDE;
    virtual blink::WebRTCSessionDescription localDescription() OVERRIDE;
    virtual blink::WebRTCSessionDescription remoteDescription() OVERRIDE;
    virtual bool updateICE(const blink::WebRTCConfiguration&, const blink::WebMediaConstraints&) OVERRIDE;
    virtual bool addICECandidate(const blink::WebRTCICECandidate&) OVERRIDE;
    virtual bool addICECandidate(const blink::WebRTCVoidRequest&, const blink::WebRTCICECandidate&) OVERRIDE;
    virtual bool addStream(const blink::WebMediaStream&, const blink::WebMediaConstraints&) OVERRIDE;
    virtual void removeStream(const blink::WebMediaStream&) OVERRIDE;
    virtual void getStats(const blink::WebRTCStatsRequest&) OVERRIDE;
    virtual blink::WebRTCDataChannelHandler* createDataChannel(const blink::WebString& label, const blink::WebRTCDataChannelInit&) OVERRIDE;
    virtual blink::WebRTCDTMFSenderHandler* createDTMFSender(const blink::WebMediaStreamTrack&) OVERRIDE;
    virtual void stop() OVERRIDE;

    // WebTask related methods
    WebTaskList* taskList() { return &m_taskList; }

private:
    MockWebRTCPeerConnectionHandler() { }

    blink::WebRTCPeerConnectionHandlerClient* m_client;
    bool m_stopped;
    WebTaskList m_taskList;
    blink::WebRTCSessionDescription m_localDescription;
    blink::WebRTCSessionDescription m_remoteDescription;
    int m_streamCount;
    TestInterfaces* m_interfaces;
};

}

#endif // MockWebRTCPeerConnectionHandler_h

