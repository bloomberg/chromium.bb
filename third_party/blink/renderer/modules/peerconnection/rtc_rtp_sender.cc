// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_rtp_sender.h"
#include "third_party/blink/public/platform/web_rtc_dtmf_sender_handler.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_track.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_dtmf_sender.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_error_util.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_peer_connection.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_rtp_parameters.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_void_request_script_promise_resolver_impl.h"
#include "third_party/blink/renderer/modules/peerconnection/web_rtc_stats_report_callback_resolver.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_void_request.h"

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

  void RequestFailed(const webrtc::RTCError& error) override {
    resolver_->Reject(CreateDOMExceptionFromRTCError(error));
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

class SetParametersRequest : public RTCVoidRequestScriptPromiseResolverImpl {
 public:
  SetParametersRequest(ScriptPromiseResolver* resolver, RTCRtpSender* sender)
      : RTCVoidRequestScriptPromiseResolverImpl(resolver), sender_(sender) {}

  void RequestSucceeded() override {
    sender_->ClearLastReturnedParameters();
    RTCVoidRequestScriptPromiseResolverImpl::RequestSucceeded();
  }

  void RequestFailed(const webrtc::RTCError& error) override {
    sender_->ClearLastReturnedParameters();
    RTCVoidRequestScriptPromiseResolverImpl::RequestFailed(error);
  }

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(sender_);
    RTCVoidRequestScriptPromiseResolverImpl::Trace(visitor);
  }

 private:
  Member<RTCRtpSender> sender_;
};

bool HasInvalidModification(const RTCRtpParameters& parameters,
                            const RTCRtpParameters& new_parameters) {
  if (parameters.hasTransactionId() != new_parameters.hasTransactionId() ||
      (parameters.hasTransactionId() &&
       parameters.transactionId() != new_parameters.transactionId()))
    return true;

  if (parameters.hasEncodings() != new_parameters.hasEncodings())
    return true;
  if (parameters.hasEncodings()) {
    if (parameters.encodings().size() != new_parameters.encodings().size())
      return true;

    for (std::size_t i = 0; i < parameters.encodings().size(); ++i) {
      const auto& encoding = parameters.encodings()[i];
      const auto& new_encoding = new_parameters.encodings()[i];
      if (encoding.hasRid() != new_encoding.hasRid() ||
          (encoding.hasRid() && encoding.rid() != new_encoding.rid())) {
        return true;
      }
    }
  }

  if (parameters.hasHeaderExtensions() != new_parameters.hasHeaderExtensions())
    return true;

  if (parameters.hasHeaderExtensions()) {
    if (parameters.headerExtensions().size() !=
        new_parameters.headerExtensions().size())
      return true;

    for (size_t i = 0; i < parameters.headerExtensions().size(); ++i) {
      const auto& header_extension = parameters.headerExtensions()[i];
      const auto& new_header_extension = new_parameters.headerExtensions()[i];
      if (header_extension.hasUri() != new_header_extension.hasUri() ||
          (header_extension.hasUri() &&
           header_extension.uri() != new_header_extension.uri()) ||
          header_extension.hasId() != new_header_extension.hasId() ||
          (header_extension.hasId() &&
           header_extension.id() != new_header_extension.id()) ||
          header_extension.hasEncrypted() !=
              new_header_extension.hasEncrypted() ||
          (header_extension.hasEncrypted() &&
           header_extension.encrypted() != new_header_extension.encrypted())) {
        return true;
      }
    }
  }

  if (parameters.hasRtcp() != new_parameters.hasRtcp() ||
      (parameters.hasRtcp() &&
       ((parameters.rtcp().hasCname() != new_parameters.rtcp().hasCname() ||
         (parameters.rtcp().hasCname() &&
          parameters.rtcp().cname() != new_parameters.rtcp().cname())) ||
        (parameters.rtcp().hasReducedSize() !=
             new_parameters.rtcp().hasReducedSize() ||
         (parameters.rtcp().hasReducedSize() &&
          parameters.rtcp().reducedSize() !=
              new_parameters.rtcp().reducedSize())))))
    return true;

  if (parameters.hasCodecs() != new_parameters.hasCodecs())
    return true;

  if (parameters.hasCodecs()) {
    if (parameters.codecs().size() != new_parameters.codecs().size())
      return true;

    for (std::size_t i = 0; i < parameters.codecs().size(); ++i) {
      const auto& codec = parameters.codecs()[i];
      const auto& new_codec = new_parameters.codecs()[i];
      if (codec.hasPayloadType() != new_codec.hasPayloadType() ||
          (codec.hasPayloadType() &&
           codec.payloadType() != new_codec.payloadType()) ||
          codec.hasMimeType() != new_codec.hasMimeType() ||
          (codec.hasMimeType() && codec.mimeType() != new_codec.mimeType()) ||
          codec.hasClockRate() != new_codec.hasClockRate() ||
          (codec.hasClockRate() &&
           codec.clockRate() != new_codec.clockRate()) ||
          codec.hasChannels() != new_codec.hasChannels() ||
          (codec.hasChannels() && codec.channels() != new_codec.channels()) ||
          codec.hasSdpFmtpLine() != new_codec.hasSdpFmtpLine() ||
          (codec.hasSdpFmtpLine() &&
           codec.sdpFmtpLine() != new_codec.sdpFmtpLine())) {
        return true;
      }
    }
  }

  return false;
}

std::tuple<WebVector<WebRTCRtpEncodingParameters>,
           base::Optional<WebRTCDegradationPreference>>
ToWebRTCRtpParameters(const RTCRtpParameters& parameters) {
  WebVector<WebRTCRtpEncodingParameters> encodings;
  if (parameters.hasEncodings()) {
    encodings.reserve(parameters.encodings().size());

    for (const auto& encoding : parameters.encodings()) {
      // TODO(orphis): Forward missing fields from the WebRTC library:
      // codecPayloadType, dtx, ptime, maxFramerate, scaleResolutionDownBy,
      // rid
      const base::Optional<uint8_t> codec_payload_type;
      const base::Optional<WebRTCDtxStatus> dtx;
      bool active = encoding.active();

      const auto& js_priority = encoding.priority();
      WebRTCPriorityType priority;

      if (js_priority == "very-low") {
        priority = WebRTCPriorityType::VeryLow;
      } else if (js_priority == "low") {
        priority = WebRTCPriorityType::Low;
      } else if (js_priority == "medium") {
        priority = WebRTCPriorityType::Medium;
      } else if (js_priority == "high") {
        priority = WebRTCPriorityType::High;
      } else {
        NOTREACHED();
      }

      const base::Optional<uint32_t> ptime;
      const base::Optional<uint32_t> max_bitrate =
          encoding.hasMaxBitrate() ? encoding.maxBitrate()
                                   : base::Optional<uint32_t>();
      const base::Optional<uint32_t> max_framerate;
      const base::Optional<double> scale_resolution_down_by;
      const base::Optional<WebString> rid;

      encodings.emplace_back(codec_payload_type, dtx, active, priority, ptime,
                             max_bitrate, max_framerate,
                             scale_resolution_down_by, rid);
    }
  }
  base::Optional<WebRTCDegradationPreference> degradation_preference;
  return std::make_tuple(encodings, degradation_preference);
}

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
    // TODO(orphis): Forward missing field from the WebRTC library:
    // sdpFmtpLine
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

  last_returned_parameters_ = parameters;
}

ScriptPromise RTCRtpSender::setParameters(ScriptState* script_state,
                                          const RTCRtpParameters& parameters) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  if (!last_returned_parameters_) {
    resolver->Reject(DOMException::Create(
        kInvalidStateError,
        "getParameters() needs to be called before setParameters()."));
    return promise;
  }
  // The specification mentions that some fields in the dictionary should not
  // be modified. Some of those checks are done in the lower WebRTC layer, but
  // there is no perfect 1-1 mapping between the Javascript layer and native.
  // So we save the last returned dictionary and enforce the check at this
  // level instead.
  if (HasInvalidModification(last_returned_parameters_.value(), parameters)) {
    resolver->Reject(
        DOMException::Create(kInvalidModificationError,
                             "Read-only field modified in setParameters()."));
    return promise;
  }

  // The only values that can be set by setParameters are in the encodings
  // field and the degradationPreference field. We just forward those to the
  // native layer without having to transform all the other read-only
  // parameters.
  WebVector<WebRTCRtpEncodingParameters> encodings;
  base::Optional<WebRTCDegradationPreference> degradation_preference;
  std::tie(encodings, degradation_preference) =
      ToWebRTCRtpParameters(parameters);

  if (!degradation_preference) {
    degradation_preference = blink::WebRTCDegradationPreference::Balanced;
  }

  auto* request = new SetParametersRequest(resolver, this);
  sender_->SetParameters(std::move(encodings), degradation_preference.value(),
                         request);
  return promise;
}

void RTCRtpSender::ClearLastReturnedParameters() {
  last_returned_parameters_.reset();
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
  visitor->Trace(last_returned_parameters_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
