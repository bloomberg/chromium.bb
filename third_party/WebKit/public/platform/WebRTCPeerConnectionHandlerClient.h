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

#ifndef WebRTCPeerConnectionHandlerClient_h
#define WebRTCPeerConnectionHandlerClient_h

#include <memory>

#include "WebCommon.h"
#include "WebVector.h"

namespace blink {

class WebMediaStream;
class WebRTCDataChannelHandler;
class WebRTCICECandidate;
class WebRTCRtpReceiver;

class BLINK_PLATFORM_EXPORT WebRTCPeerConnectionHandlerClient {
 public:
  enum SignalingState {
    kSignalingStateStable = 1,
    kSignalingStateHaveLocalOffer = 2,
    kSignalingStateHaveRemoteOffer = 3,
    kSignalingStateHaveLocalPrAnswer = 4,
    kSignalingStateHaveRemotePrAnswer = 5,
    kSignalingStateClosed = 6,
  };

  enum ICEConnectionState {
    kICEConnectionStateNew = 1,
    kICEConnectionStateChecking = 2,
    kICEConnectionStateConnected = 3,
    kICEConnectionStateCompleted = 4,
    kICEConnectionStateFailed = 5,
    kICEConnectionStateDisconnected = 6,
    kICEConnectionStateClosed = 7,

    // DEPRECATED
    kICEConnectionStateStarting = 1,
  };

  enum ICEGatheringState {
    kICEGatheringStateNew = 1,
    kICEGatheringStateGathering = 2,
    kICEGatheringStateComplete = 3
  };

  virtual ~WebRTCPeerConnectionHandlerClient();

  virtual void NegotiationNeeded() = 0;
  virtual void DidGenerateICECandidate(const WebRTCICECandidate&) = 0;
  virtual void DidChangeSignalingState(SignalingState) = 0;
  virtual void DidChangeICEGatheringState(ICEGatheringState) = 0;
  virtual void DidChangeICEConnectionState(ICEConnectionState) = 0;
  // Using pointer for the receivers argument instead of value due to MOCK-macro
  // being incompatible with having to move the vector. This function may take
  // ownership of all receivers in the vector.
  // TODO(hbos): Move away from events of remote streams being added/removed
  // towards events of remote tracks being added/removed.
  // https://crbug.com/741619, 741618
  virtual void DidAddRemoteStream(
      const WebMediaStream&,
      WebVector<std::unique_ptr<WebRTCRtpReceiver>>*) = 0;
  virtual void DidRemoveRemoteStream(const WebMediaStream&) = 0;
  virtual void DidAddRemoteDataChannel(WebRTCDataChannelHandler*) = 0;
  virtual void ReleasePeerConnectionHandler() = 0;
  virtual void ClosePeerConnection();
};

}  // namespace blink

#endif  // WebRTCPeerConnectionHandlerClient_h
