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

#ifndef WebRTCPeerConnectionHandler_h
#define WebRTCPeerConnectionHandler_h

#include "WebRTCICECandidate.h"
#include "WebRTCStats.h"
#include "WebVector.h"

namespace blink {

class WebMediaConstraints;
class WebMediaStream;
class WebMediaStreamTrack;
class WebRTCAnswerOptions;
class WebRTCDTMFSenderHandler;
class WebRTCDataChannelHandler;
enum class WebRTCErrorType;
class WebRTCOfferOptions;
class WebRTCRtpSender;
class WebRTCSessionDescription;
class WebRTCSessionDescriptionRequest;
class WebRTCStatsRequest;
class WebRTCVoidRequest;
class WebString;
struct WebRTCConfiguration;
struct WebRTCDataChannelInit;

class WebRTCPeerConnectionHandler {
 public:
  virtual ~WebRTCPeerConnectionHandler() = default;

  virtual bool Initialize(const WebRTCConfiguration&,
                          const WebMediaConstraints&) = 0;

  virtual void CreateOffer(const WebRTCSessionDescriptionRequest&,
                           const WebMediaConstraints&) = 0;
  virtual void CreateOffer(const WebRTCSessionDescriptionRequest&,
                           const WebRTCOfferOptions&) = 0;
  virtual void CreateAnswer(const WebRTCSessionDescriptionRequest&,
                            const WebMediaConstraints&) = 0;
  virtual void CreateAnswer(const WebRTCSessionDescriptionRequest&,
                            const WebRTCAnswerOptions&) = 0;
  virtual void SetLocalDescription(const WebRTCVoidRequest&,
                                   const WebRTCSessionDescription&) = 0;
  virtual void SetRemoteDescription(const WebRTCVoidRequest&,
                                    const WebRTCSessionDescription&) = 0;
  virtual WebRTCSessionDescription LocalDescription() = 0;
  virtual WebRTCSessionDescription RemoteDescription() = 0;
  virtual WebRTCErrorType SetConfiguration(const WebRTCConfiguration&) = 0;

  // DEPRECATED
  virtual bool AddICECandidate(scoped_refptr<WebRTCICECandidate>) {
    return false;
  }

  virtual bool AddICECandidate(const WebRTCVoidRequest&,
                               scoped_refptr<WebRTCICECandidate>) {
    return false;
  }
  virtual bool AddStream(const WebMediaStream&, const WebMediaConstraints&) = 0;
  virtual void RemoveStream(const WebMediaStream&) = 0;
  virtual void GetStats(const WebRTCStatsRequest&) = 0;
  // Gets stats using the new stats collection API, see
  // third_party/webrtc/api/stats/.  These will replace the old stats collection
  // API when the new API has matured enough.
  virtual void GetStats(std::unique_ptr<WebRTCStatsReportCallback>) = 0;
  virtual WebRTCDataChannelHandler* CreateDataChannel(
      const WebString& label,
      const WebRTCDataChannelInit&) = 0;
  // Gets senders used by the peer connection. These are wrappers referencing
  // webrtc-layer senders, multiple |WebRTCRtpSender| objects referencing the
  // same webrtc-layer sender have the same |id|.
  virtual WebVector<std::unique_ptr<WebRTCRtpSender>> GetSenders() = 0;
  // Adds the track to the peer connection, returning the resulting sender on
  // success and null on failure.
  virtual std::unique_ptr<WebRTCRtpSender> AddTrack(
      const WebMediaStreamTrack&,
      const WebVector<WebMediaStream>&) = 0;
  // Removes the sender, returning whether successful. On success, the sender's
  // track must have been set to null.
  virtual bool RemoveTrack(WebRTCRtpSender*) = 0;
  virtual WebRTCDTMFSenderHandler* CreateDTMFSender(
      const WebMediaStreamTrack&) = 0;
  virtual void Stop() = 0;
};

}  // namespace blink

#endif  // WebRTCPeerConnectionHandler_h
