// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_rtp_sender.h"

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_rtc_dtmf_sender_handler.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_track.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_dtmf_sender.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_error_util.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_peer_connection.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_rtp_capabilities.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_stats_report.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_void_request_script_promise_resolver_impl.h"
#include "third_party/blink/renderer/modules/peerconnection/web_rtc_stats_report_callback_resolver.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_void_request.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

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
    ScriptState::Scope scope(resolver_->GetScriptState());
    ExceptionState exception_state(resolver_->GetScriptState()->GetIsolate(),
                                   ExceptionState::kExecutionContext,
                                   "RTCRtpSender", "replaceTrack");
    ThrowExceptionFromRTCError(error, exception_state);
    resolver_->Reject(exception_state);
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
      : RTCVoidRequestScriptPromiseResolverImpl(resolver,
                                                "RTCRtpSender",
                                                "setParameters"),
        sender_(sender) {}

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

bool HasInvalidModification(const RTCRtpSendParameters* parameters,
                            const RTCRtpSendParameters* new_parameters) {
  if (parameters->hasTransactionId() != new_parameters->hasTransactionId() ||
      (parameters->hasTransactionId() &&
       parameters->transactionId() != new_parameters->transactionId())) {
    return true;
  }

  if (parameters->hasEncodings() != new_parameters->hasEncodings())
    return true;
  if (parameters->hasEncodings()) {
    if (parameters->encodings().size() != new_parameters->encodings().size())
      return true;
  }

  if (parameters->hasHeaderExtensions() !=
      new_parameters->hasHeaderExtensions())
    return true;

  if (parameters->hasHeaderExtensions()) {
    if (parameters->headerExtensions().size() !=
        new_parameters->headerExtensions().size())
      return true;

    for (wtf_size_t i = 0; i < parameters->headerExtensions().size(); ++i) {
      const auto& header_extension = parameters->headerExtensions()[i];
      const auto& new_header_extension = new_parameters->headerExtensions()[i];
      if (header_extension->hasUri() != new_header_extension->hasUri() ||
          (header_extension->hasUri() &&
           header_extension->uri() != new_header_extension->uri()) ||
          header_extension->hasId() != new_header_extension->hasId() ||
          (header_extension->hasId() &&
           header_extension->id() != new_header_extension->id()) ||
          header_extension->hasEncrypted() !=
              new_header_extension->hasEncrypted() ||
          (header_extension->hasEncrypted() &&
           header_extension->encrypted() !=
               new_header_extension->encrypted())) {
        return true;
      }
    }
  }

  if (parameters->hasRtcp() != new_parameters->hasRtcp() ||
      (parameters->hasRtcp() &&
       ((parameters->rtcp()->hasCname() != new_parameters->rtcp()->hasCname() ||
         (parameters->rtcp()->hasCname() &&
          parameters->rtcp()->cname() != new_parameters->rtcp()->cname())) ||
        (parameters->rtcp()->hasReducedSize() !=
             new_parameters->rtcp()->hasReducedSize() ||
         (parameters->rtcp()->hasReducedSize() &&
          parameters->rtcp()->reducedSize() !=
              new_parameters->rtcp()->reducedSize()))))) {
    return true;
  }

  if (parameters->hasCodecs() != new_parameters->hasCodecs())
    return true;

  if (parameters->hasCodecs()) {
    if (parameters->codecs().size() != new_parameters->codecs().size())
      return true;

    for (wtf_size_t i = 0; i < parameters->codecs().size(); ++i) {
      const auto& codec = parameters->codecs()[i];
      const auto& new_codec = new_parameters->codecs()[i];
      if (codec->hasPayloadType() != new_codec->hasPayloadType() ||
          (codec->hasPayloadType() &&
           codec->payloadType() != new_codec->payloadType()) ||
          codec->hasMimeType() != new_codec->hasMimeType() ||
          (codec->hasMimeType() &&
           codec->mimeType() != new_codec->mimeType()) ||
          codec->hasClockRate() != new_codec->hasClockRate() ||
          (codec->hasClockRate() &&
           codec->clockRate() != new_codec->clockRate()) ||
          codec->hasChannels() != new_codec->hasChannels() ||
          (codec->hasChannels() &&
           codec->channels() != new_codec->channels()) ||
          codec->hasSdpFmtpLine() != new_codec->hasSdpFmtpLine() ||
          (codec->hasSdpFmtpLine() &&
           codec->sdpFmtpLine() != new_codec->sdpFmtpLine())) {
        return true;
      }
    }
  }

  return false;
}

// Relative weights for each priority as defined in RTCWEB-DATA
// https://tools.ietf.org/html/draft-ietf-rtcweb-data-channel
const double kPriorityWeightVeryLow = 0.5;
const double kPriorityWeightLow = 1;
const double kPriorityWeightMedium = 2;
const double kPriorityWeightHigh = 4;

std::string PriorityFromDouble(double priority) {
  // Find the middle point between 2 priority weights to match them to a
  // WebRTC priority
  const double very_low_upper_bound =
      (kPriorityWeightVeryLow + kPriorityWeightLow) / 2;
  const double low_upper_bound =
      (kPriorityWeightLow + kPriorityWeightMedium) / 2;
  const double medium_upper_bound =
      (kPriorityWeightMedium + kPriorityWeightHigh) / 2;

  if (priority < webrtc::kDefaultBitratePriority * very_low_upper_bound) {
    return "very-low";
  }
  if (priority < webrtc::kDefaultBitratePriority * low_upper_bound) {
    return "low";
  }
  if (priority < webrtc::kDefaultBitratePriority * medium_upper_bound) {
    return "medium";
  }
  return "high";
}

double PriorityToDouble(const WTF::String& priority) {
  double result = 1;

  if (priority == "very-low") {
    result = webrtc::kDefaultBitratePriority * kPriorityWeightVeryLow;
  } else if (priority == "low") {
    result = webrtc::kDefaultBitratePriority * kPriorityWeightLow;
  } else if (priority == "medium") {
    result = webrtc::kDefaultBitratePriority * kPriorityWeightMedium;
  } else if (priority == "high") {
    result = webrtc::kDefaultBitratePriority * kPriorityWeightHigh;
  } else {
    NOTREACHED();
  }
  return result;
}

std::tuple<std::vector<webrtc::RtpEncodingParameters>,
           webrtc::DegradationPreference>
ToRtpParameters(const RTCRtpSendParameters* parameters) {
  std::vector<webrtc::RtpEncodingParameters> encodings;
  if (parameters->hasEncodings()) {
    encodings.reserve(parameters->encodings().size());

    for (const auto& encoding : parameters->encodings()) {
      encodings.push_back(ToRtpEncodingParameters(encoding));
    }
  }

  webrtc::DegradationPreference degradation_preference =
      webrtc::DegradationPreference::BALANCED;
  return std::make_tuple(encodings, degradation_preference);
}

}  // namespace

webrtc::RtpEncodingParameters ToRtpEncodingParameters(
    const RTCRtpEncodingParameters* encoding) {
  // TODO(orphis): Forward missing fields from the WebRTC library:
  // codecPayloadType, dtx, ptime, maxFramerate, scaleResolutionDownBy,
  // rid
  webrtc::RtpEncodingParameters webrtc_encoding;
  webrtc_encoding.active = encoding->active();
  webrtc_encoding.bitrate_priority = PriorityToDouble(encoding->priority());
  webrtc_encoding.network_priority =
      PriorityToDouble(encoding->networkPriority());
  if (encoding->hasMaxBitrate())
    webrtc_encoding.max_bitrate_bps = clampTo<int>(encoding->maxBitrate());
  return webrtc_encoding;
}

RTCRtpHeaderExtensionParameters* ToRtpHeaderExtensionParameters(
    const webrtc::RtpHeaderExtensionParameters& webrtc_header) {
  RTCRtpHeaderExtensionParameters* header =
      RTCRtpHeaderExtensionParameters::Create();
  header->setUri(webrtc_header.uri.c_str());
  header->setId(webrtc_header.id);
  header->setEncrypted(webrtc_header.encrypt);
  return header;
}

RTCRtpCodecParameters* ToRtpCodecParameters(
    const webrtc::RtpCodecParameters& webrtc_codec) {
  RTCRtpCodecParameters* codec = RTCRtpCodecParameters::Create();
  codec->setPayloadType(webrtc_codec.payload_type);
  codec->setMimeType(WTF::String::FromUTF8(webrtc_codec.mime_type().c_str()));
  if (webrtc_codec.clock_rate)
    codec->setClockRate(webrtc_codec.clock_rate.value());
  if (webrtc_codec.num_channels)
    codec->setChannels(webrtc_codec.num_channels.value());
  if (webrtc_codec.parameters.size()) {
    std::string sdp_fmtp_line;
    for (const auto& parameter : webrtc_codec.parameters) {
      if (sdp_fmtp_line.size())
        sdp_fmtp_line += ";";
      sdp_fmtp_line += parameter.first + "=" + parameter.second;
    }
    codec->setSdpFmtpLine(sdp_fmtp_line.c_str());
  }
  return codec;
}

RTCRtpSender::RTCRtpSender(RTCPeerConnection* pc,
                           std::unique_ptr<WebRTCRtpSender> sender,
                           String kind,
                           MediaStreamTrack* track,
                           MediaStreamVector streams)
    : pc_(pc),
      sender_(std::move(sender)),
      kind_(std::move(kind)),
      track_(track),
      streams_(std::move(streams)) {
  DCHECK(pc_);
  DCHECK(sender_);
  DCHECK(!track || kind_ == track->kind());
}

MediaStreamTrack* RTCRtpSender::track() {
  return track_;
}

RTCDtlsTransport* RTCRtpSender::transport() {
  if (!transceiver_)
    return nullptr;
  return pc_->LookupDtlsTransportByMid(transceiver_->mid());
}

RTCDtlsTransport* RTCRtpSender::rtcp_transport() {
  // Chrome does not support turning off RTCP-mux.
  return nullptr;
}

ScriptPromise RTCRtpSender::replaceTrack(ScriptState* script_state,
                                         MediaStreamTrack* with_track) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();
  if (pc_->IsClosed()) {
    resolver->Reject(DOMException::Create(DOMExceptionCode::kInvalidStateError,
                                          "The peer connection is closed."));
    return promise;
  }
  WebMediaStreamTrack web_track;
  if (with_track) {
    pc_->RegisterTrack(with_track);
    web_track = with_track->Component();
  }
  ReplaceTrackRequest* request =
      MakeGarbageCollected<ReplaceTrackRequest>(this, with_track, resolver);
  sender_->ReplaceTrack(web_track, request);
  return promise;
}

RTCRtpSendParameters* RTCRtpSender::getParameters() {
  RTCRtpSendParameters* parameters = RTCRtpSendParameters::Create();
  // TODO(orphis): Forward missing fields from the WebRTC library:
  // degradationPreference
  std::unique_ptr<webrtc::RtpParameters> webrtc_parameters =
      sender_->GetParameters();

  parameters->setTransactionId(webrtc_parameters->transaction_id.c_str());

  RTCRtcpParameters* rtcp = RTCRtcpParameters::Create();
  rtcp->setCname(webrtc_parameters->rtcp.cname.c_str());
  rtcp->setReducedSize(webrtc_parameters->rtcp.reduced_size);
  parameters->setRtcp(rtcp);

  HeapVector<Member<RTCRtpEncodingParameters>> encodings;
  encodings.ReserveCapacity(
      SafeCast<wtf_size_t>(webrtc_parameters->encodings.size()));
  for (const auto& webrtc_encoding : webrtc_parameters->encodings) {
    // TODO(orphis): Forward missing fields from the WebRTC library:
    // codecPayloadType, dtx, ptime, maxFramerate, scaleResolutionDownBy, rid
    RTCRtpEncodingParameters* encoding = RTCRtpEncodingParameters::Create();
    encoding->setActive(webrtc_encoding.active);
    if (webrtc_encoding.max_bitrate_bps)
      encoding->setMaxBitrate(webrtc_encoding.max_bitrate_bps.value());
    encoding->setPriority(
        PriorityFromDouble(webrtc_encoding.bitrate_priority).c_str());
    encoding->setNetworkPriority(
        PriorityFromDouble(webrtc_encoding.network_priority).c_str());
    encodings.push_back(encoding);
  }
  parameters->setEncodings(encodings);

  HeapVector<Member<RTCRtpHeaderExtensionParameters>> headers;
  headers.ReserveCapacity(
      SafeCast<wtf_size_t>(webrtc_parameters->header_extensions.size()));
  for (const auto& webrtc_header : webrtc_parameters->header_extensions) {
    headers.push_back(ToRtpHeaderExtensionParameters(webrtc_header));
  }
  parameters->setHeaderExtensions(headers);

  HeapVector<Member<RTCRtpCodecParameters>> codecs;
  codecs.ReserveCapacity(
      SafeCast<wtf_size_t>(webrtc_parameters->codecs.size()));
  for (const auto& webrtc_codec : webrtc_parameters->codecs) {
    codecs.push_back(ToRtpCodecParameters(webrtc_codec));
  }
  parameters->setCodecs(codecs);

  last_returned_parameters_ = parameters;

  return parameters;
}

ScriptPromise RTCRtpSender::setParameters(
    ScriptState* script_state,
    const RTCRtpSendParameters* parameters) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  if (!last_returned_parameters_) {
    resolver->Reject(DOMException::Create(
        DOMExceptionCode::kInvalidStateError,
        "getParameters() needs to be called before setParameters()."));
    return promise;
  }
  // The specification mentions that some fields in the dictionary should not
  // be modified. Some of those checks are done in the lower WebRTC layer, but
  // there is no perfect 1-1 mapping between the Javascript layer and native.
  // So we save the last returned dictionary and enforce the check at this
  // level instead.
  if (HasInvalidModification(last_returned_parameters_, parameters)) {
    resolver->Reject(
        DOMException::Create(DOMExceptionCode::kInvalidModificationError,
                             "Read-only field modified in setParameters()."));
    return promise;
  }

  // The only values that can be set by setParameters are in the encodings
  // field and the degradationPreference field. We just forward those to the
  // native layer without having to transform all the other read-only
  // parameters.
  std::vector<webrtc::RtpEncodingParameters> encodings;
  webrtc::DegradationPreference degradation_preference;
  std::tie(encodings, degradation_preference) = ToRtpParameters(parameters);

  auto* request = MakeGarbageCollected<SetParametersRequest>(resolver, this);
  sender_->SetParameters(std::move(encodings), degradation_preference, request);
  return promise;
}

void RTCRtpSender::ClearLastReturnedParameters() {
  last_returned_parameters_ = nullptr;
}

ScriptPromise RTCRtpSender::getStats(ScriptState* script_state) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();
  sender_->GetStats(WebRTCStatsReportCallbackResolver::Create(resolver),
                    GetRTCStatsFilter(script_state));
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

void RTCRtpSender::set_streams(MediaStreamVector streams) {
  streams_ = std::move(streams);
}

void RTCRtpSender::set_transceiver(RTCRtpTransceiver* transceiver) {
  transceiver_ = transceiver;
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
  visitor->Trace(transceiver_);
  ScriptWrappable::Trace(visitor);
}

RTCRtpCapabilities* RTCRtpSender::getCapabilities(const String& kind) {
  if (kind != "audio" && kind != "video")
    return nullptr;

  RTCRtpCapabilities* capabilities = RTCRtpCapabilities::Create();
  capabilities->setCodecs(HeapVector<Member<RTCRtpCodecCapability>>());
  capabilities->setHeaderExtensions(
      HeapVector<Member<RTCRtpHeaderExtensionCapability>>());

  std::unique_ptr<webrtc::RtpCapabilities> rtc_capabilities =
      blink::Platform::Current()->GetRtpSenderCapabilities(kind);

  HeapVector<Member<RTCRtpCodecCapability>> codecs;
  codecs.ReserveInitialCapacity(
      SafeCast<wtf_size_t>(rtc_capabilities->codecs.size()));
  for (const auto& rtc_codec : rtc_capabilities->codecs) {
    auto* codec = RTCRtpCodecCapability::Create();
    codec->setMimeType(WTF::String::FromUTF8(rtc_codec.mime_type().c_str()));
    if (rtc_codec.clock_rate)
      codec->setClockRate(rtc_codec.clock_rate.value());
    if (rtc_codec.num_channels)
      codec->setChannels(rtc_codec.num_channels.value());
    if (rtc_codec.parameters.size()) {
      std::string sdp_fmtp_line;
      for (const auto& parameter : rtc_codec.parameters) {
        if (sdp_fmtp_line.size())
          sdp_fmtp_line += ";";
        sdp_fmtp_line += parameter.first + "=" + parameter.second;
      }
      codec->setSdpFmtpLine(sdp_fmtp_line.c_str());
    }
    codecs.push_back(codec);
  }
  capabilities->setCodecs(codecs);

  HeapVector<Member<RTCRtpHeaderExtensionCapability>> header_extensions;
  header_extensions.ReserveInitialCapacity(
      SafeCast<wtf_size_t>(rtc_capabilities->header_extensions.size()));
  for (const auto& rtc_header_extension : rtc_capabilities->header_extensions) {
    auto* header_extension = RTCRtpHeaderExtensionCapability::Create();
    header_extension->setUri(
        WTF::String::FromUTF8(rtc_header_extension.uri.c_str()));
    header_extensions.push_back(header_extension);
  }
  capabilities->setHeaderExtensions(header_extensions);

  return capabilities;
}

}  // namespace blink
