// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RTCRtpSender_h
#define RTCRtpSender_h

#include "bindings/core/v8/ScriptPromise.h"
#include "modules/mediastream/MediaStream.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/GarbageCollected.h"
#include "platform/heap/Member.h"
#include "platform/heap/Visitor.h"
#include "platform/wtf/text/WTFString.h"
#include "public/platform/WebRTCRtpSender.h"

namespace blink {

class MediaStreamTrack;
class RTCDTMFSender;
class RTCPeerConnection;
class RTCRtpParameters;

// https://w3c.github.io/webrtc-pc/#rtcrtpsender-interface
class RTCRtpSender final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // TODO(hbos): Get rid of sender's reference to RTCPeerConnection?
  // https://github.com/w3c/webrtc-pc/issues/1712
  RTCRtpSender(RTCPeerConnection*,
               std::unique_ptr<WebRTCRtpSender>,
               MediaStreamTrack*,
               MediaStreamVector streams);

  MediaStreamTrack* track();
  ScriptPromise replaceTrack(ScriptState*, MediaStreamTrack*);
  RTCDTMFSender* dtmf();
  void getParameters(RTCRtpParameters&);
  ScriptPromise setParameters(ScriptState*, const RTCRtpParameters&);
  ScriptPromise getStats(ScriptState*);

  WebRTCRtpSender* web_sender();
  // Sets the track. This must be called when the |WebRTCRtpSender| has its
  // track updated, and the |track| must match the |WebRTCRtpSender::Track|.
  void SetTrack(MediaStreamTrack*);
  MediaStreamVector streams() const;

  virtual void Trace(blink::Visitor*);

 private:
  Member<RTCPeerConnection> pc_;
  std::unique_ptr<WebRTCRtpSender> sender_;
  Member<MediaStreamTrack> track_;
  // The spec says that "kind" should be looked up in transceiver, but
  // keeping it in sender at least until transceiver is implemented.
  String kind_;
  Member<RTCDTMFSender> dtmf_;
  MediaStreamVector streams_;
};

}  // namespace blink

#endif  // RTCRtpSender_h
