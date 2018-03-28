// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/peerconnection/RTCRtpSender.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "modules/mediastream/MediaStreamTrack.h"
#include "modules/peerconnection/RTCDTMFSender.h"
#include "modules/peerconnection/RTCErrorUtil.h"
#include "modules/peerconnection/RTCPeerConnection.h"
#include "modules/peerconnection/RTCRtpParameters.h"
#include "modules/peerconnection/WebRTCStatsReportCallbackResolver.h"
#include "platform/peerconnection/RTCVoidRequest.h"
#include "public/platform/WebRTCDTMFSenderHandler.h"

namespace blink {

namespace {

class ReplaceTrackRequest : public RTCVoidRequest {
 public:
  ReplaceTrackRequest(RTCRtpSender* sender,
                      MediaStreamTrack* with_track,
                      ScriptPromiseResolver* resolver)
      : sender_(sender), with_track_(with_track), resolver_(resolver) {}
  ~ReplaceTrackRequest() override {}

  void RequestSucceeded() override {
    sender_->SetTrack(with_track_);
    resolver_->Resolve();
  }

  void RequestFailed(const WebRTCError& error) override {
    resolver_->Reject(CreateDOMExceptionFromWebRTCError(error));
  }

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(sender_);
    visitor->Trace(with_track_);
    visitor->Trace(resolver_);
    RTCVoidRequest::Trace(visitor);
  }

 private:
  Member<RTCRtpSender> sender_;
  Member<MediaStreamTrack> with_track_;
  Member<ScriptPromiseResolver> resolver_;
};

}  // namespace

RTCRtpSender::RTCRtpSender(RTCPeerConnection* pc,
                           std::unique_ptr<WebRTCRtpSender> sender,
                           MediaStreamTrack* track,
                           MediaStreamVector streams)
    : pc_(pc),
      sender_(std::move(sender)),
      track_(track),
      streams_(std::move(streams)) {
  DCHECK(pc_);
  DCHECK(sender_);
  DCHECK(track_);
  kind_ = track->kind();
}

MediaStreamTrack* RTCRtpSender::track() {
  return track_;
}

ScriptPromise RTCRtpSender::replaceTrack(ScriptState* script_state,
                                         MediaStreamTrack* with_track) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();
  if (pc_->IsClosed()) {
    resolver->Reject(DOMException::Create(kInvalidStateError,
                                          "The peer connection is closed."));
    return promise;
  }
  WebMediaStreamTrack web_track;
  if (with_track)
    web_track = with_track->Component();
  ReplaceTrackRequest* request =
      new ReplaceTrackRequest(this, with_track, resolver);
  sender_->ReplaceTrack(web_track, request);
  return promise;
}

void RTCRtpSender::getParameters(RTCRtpParameters& parameters) {
  // TODO(orphis): Forward missing fields from the WebRTC library:
  // transactionId, rtcp, headerExtensions, degradationPreference
  std::unique_ptr<WebRTCRtpParameters> web_parameters =
      sender_->GetParameters();

  HeapVector<RTCRtpEncodingParameters> encodings;
  encodings.ReserveCapacity(web_parameters->Encodings().size());
  for (const auto& web_encoding : web_parameters->Encodings()) {
    // TODO(orphis): Forward missing fields from the WebRTC library:
    // codecPayloadType, dtx, ptime, maxFramerate, scaleResolutionDownBy, rid
    encodings.emplace_back();
    RTCRtpEncodingParameters& encoding = encodings.back();
    encoding.setActive(web_encoding.Active());
    if (web_encoding.MaxBitrate())
      encoding.setMaxBitrate(web_encoding.MaxBitrate().value());

    const char* priority = "";
    switch (web_encoding.Priority()) {
      case WebRTCPriorityType::VeryLow:
        priority = "very-low";
        break;
      case WebRTCPriorityType::Low:
        priority = "low";
        break;
      case WebRTCPriorityType::Medium:
        priority = "medium";
        break;
      case WebRTCPriorityType::High:
        priority = "high";
        break;
      default:
        NOTREACHED();
    }
    encoding.setPriority(priority);
  }
  parameters.setEncodings(encodings);

  HeapVector<RTCRtpCodecParameters> codecs;
  codecs.ReserveCapacity(web_parameters->Codecs().size());
  for (const auto& web_codec : web_parameters->Codecs()) {
    // TODO(orphis): Forward missing field from the WebRTC library: sdpFmtpLine
    codecs.emplace_back();
    RTCRtpCodecParameters& codec = codecs.back();
    if (web_codec.PayloadType())
      codec.setPayloadType(web_codec.PayloadType().value());
    if (web_codec.MimeType())
      codec.setMimeType(web_codec.MimeType().value());
    if (web_codec.ClockRate())
      codec.setClockRate(web_codec.ClockRate().value());
    if (web_codec.Channels())
      codec.setChannels(web_codec.Channels().value());
  }
  parameters.setCodecs(codecs);
}

ScriptPromise RTCRtpSender::setParameters(ScriptState* script_state,
                                          const RTCRtpParameters&) {
  return ScriptPromise::RejectWithDOMException(
      script_state,
      DOMException::Create(kNotSupportedError, "Method not implemented"));
}

ScriptPromise RTCRtpSender::getStats(ScriptState* script_state) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();
  sender_->GetStats(WebRTCStatsReportCallbackResolver::Create(resolver));
  return promise;
}

WebRTCRtpSender* RTCRtpSender::web_sender() {
  return sender_.get();
}

void RTCRtpSender::SetTrack(MediaStreamTrack* track) {
  track_ = track;
  if (track) {
    if (kind_.IsNull()) {
      kind_ = track->kind();
    } else if (kind_ != track->kind()) {
      LOG(ERROR) << "Trying to set track to a different kind: Old " << kind_
                 << " new " << track->kind();
      NOTREACHED();
    }
  }
}

MediaStreamVector RTCRtpSender::streams() const {
  return streams_;
}

RTCDTMFSender* RTCRtpSender::dtmf() {
  // Lazy initialization of dtmf_ to avoid overhead when not used.
  if (!dtmf_ && kind_ == "audio") {
    auto handler = sender_->GetDtmfSender();
    if (!handler) {
      LOG(ERROR) << "Unable to create DTMF sender attribute on an audio sender";
      return nullptr;
    }
    dtmf_ =
        RTCDTMFSender::Create(pc_->GetExecutionContext(), std::move(handler));
  }
  return dtmf_;
}

void RTCRtpSender::Trace(blink::Visitor* visitor) {
  visitor->Trace(pc_);
  visitor->Trace(track_);
  visitor->Trace(dtmf_);
  visitor->Trace(streams_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
