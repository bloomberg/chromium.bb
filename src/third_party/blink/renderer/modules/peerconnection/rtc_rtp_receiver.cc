// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_rtp_receiver.h"

#include "third_party/blink/public/common/privacy_budget/identifiability_metric_builder.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_study_settings.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_surface.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_token_builder.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_insertable_streams.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_rtcp_parameters.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_rtp_capabilities.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_rtp_codec_parameters.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_rtp_decoding_parameters.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_rtp_header_extension_capability.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_rtp_header_extension_parameters.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/streams/writable_stream.h"
#include "third_party/blink/renderer/modules/peerconnection/identifiability_metrics.h"
#include "third_party/blink/renderer/modules/peerconnection/peer_connection_dependency_factory.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_dtls_transport.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_audio_receiver_sink_optimizer.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_audio_receiver_source_optimizer.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_audio_underlying_sink.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_audio_underlying_source.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_video_underlying_sink.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_video_underlying_source.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_peer_connection.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_rtp_sender.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_stats_report.h"
#include "third_party/blink/renderer/modules/peerconnection/web_rtc_stats_report_callback_resolver.h"
#include "third_party/blink/renderer/platform/bindings/microtask.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_encoded_video_stream_transformer.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_stats.h"
#include "third_party/blink/renderer/platform/privacy_budget/identifiability_digest_helpers.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/webrtc/api/rtp_parameters.h"

namespace blink {

RTCRtpReceiver::RTCRtpReceiver(RTCPeerConnection* pc,
                               std::unique_ptr<RTCRtpReceiverPlatform> receiver,
                               MediaStreamTrack* track,
                               MediaStreamVector streams,
                               bool force_encoded_audio_insertable_streams,
                               bool force_encoded_video_insertable_streams)
    : ExecutionContextLifecycleObserver(pc->GetExecutionContext()),
      pc_(pc),
      receiver_(std::move(receiver)),
      track_(track),
      streams_(std::move(streams)),
      force_encoded_audio_insertable_streams_(
          force_encoded_audio_insertable_streams),
      force_encoded_video_insertable_streams_(
          force_encoded_video_insertable_streams) {
  DCHECK(pc_);
  DCHECK(receiver_);
  DCHECK(track_);
  if (force_encoded_audio_insertable_streams_ && track_->kind() == "audio") {
    encoded_audio_transformer_ =
        receiver_->GetEncodedAudioStreamTransformer()->GetBroker();
    RegisterEncodedAudioStreamCallback();
  }
  if (force_encoded_video_insertable_streams_ && track_->kind() == "video")
    RegisterEncodedVideoStreamCallback();
}

MediaStreamTrack* RTCRtpReceiver::track() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return track_;
}

RTCDtlsTransport* RTCRtpReceiver::transport() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return transport_;
}

RTCDtlsTransport* RTCRtpReceiver::rtcpTransport() {
  // Chrome does not support turning off RTCP-mux.
  return nullptr;
}

absl::optional<double> RTCRtpReceiver::playoutDelayHint() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return playout_delay_hint_;
}

void RTCRtpReceiver::setPlayoutDelayHint(absl::optional<double> hint,
                                         ExceptionState& exception_state) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (hint.has_value() && hint.value() < 0.0) {
    exception_state.ThrowTypeError("playoutDelayHint can't be negative");
    return;
  }

  playout_delay_hint_ = hint;
  receiver_->SetJitterBufferMinimumDelay(playout_delay_hint_);
}

HeapVector<Member<RTCRtpSynchronizationSource>>
RTCRtpReceiver::getSynchronizationSources(ScriptState* script_state,
                                          ExceptionState& exception_state) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Window is detached");
    return HeapVector<Member<RTCRtpSynchronizationSource>>();
  }

  UpdateSourcesIfNeeded();

  LocalDOMWindow* window = LocalDOMWindow::From(script_state);
  DocumentLoadTiming& time_converter =
      window->GetFrame()->Loader().GetDocumentLoader()->GetTiming();

  HeapVector<Member<RTCRtpSynchronizationSource>> synchronization_sources;
  for (const auto& web_source : web_sources_) {
    if (web_source->SourceType() != RTCRtpSource::Type::kSSRC)
      continue;
    RTCRtpSynchronizationSource* synchronization_source =
        MakeGarbageCollected<RTCRtpSynchronizationSource>();
    synchronization_source->setTimestamp(
        time_converter.MonotonicTimeToPseudoWallTime(web_source->Timestamp())
            .InMilliseconds());
    synchronization_source->setSource(web_source->Source());
    if (web_source->AudioLevel().has_value()) {
      synchronization_source->setAudioLevel(web_source->AudioLevel().value());
    }
    if (web_source->CaptureTimestamp().has_value()) {
      synchronization_source->setCaptureTimestamp(
          web_source->CaptureTimestamp().value());
    }
    if (web_source->SenderCaptureTimeOffset().has_value()) {
      synchronization_source->setSenderCaptureTimeOffset(
          web_source->SenderCaptureTimeOffset().value());
    }
    synchronization_source->setRtpTimestamp(web_source->RtpTimestamp());
    synchronization_sources.push_back(synchronization_source);
  }
  return synchronization_sources;
}

HeapVector<Member<RTCRtpContributingSource>>
RTCRtpReceiver::getContributingSources(ScriptState* script_state,
                                       ExceptionState& exception_state) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Window is detached");
    return HeapVector<Member<RTCRtpContributingSource>>();
  }

  UpdateSourcesIfNeeded();

  LocalDOMWindow* window = LocalDOMWindow::From(script_state);
  DocumentLoadTiming& time_converter =
      window->GetFrame()->Loader().GetDocumentLoader()->GetTiming();

  HeapVector<Member<RTCRtpContributingSource>> contributing_sources;
  for (const auto& web_source : web_sources_) {
    if (web_source->SourceType() != RTCRtpSource::Type::kCSRC)
      continue;
    RTCRtpContributingSource* contributing_source =
        MakeGarbageCollected<RTCRtpContributingSource>();
    contributing_source->setTimestamp(
        time_converter.MonotonicTimeToPseudoWallTime(web_source->Timestamp())
            .InMilliseconds());
    contributing_source->setSource(web_source->Source());
    if (web_source->AudioLevel().has_value()) {
      contributing_source->setAudioLevel(web_source->AudioLevel().value());
    }
    if (web_source->CaptureTimestamp().has_value()) {
      contributing_source->setCaptureTimestamp(
          web_source->CaptureTimestamp().value());
    }
    if (web_source->SenderCaptureTimeOffset().has_value()) {
      contributing_source->setSenderCaptureTimeOffset(
          web_source->SenderCaptureTimeOffset().value());
    }
    contributing_source->setRtpTimestamp(web_source->RtpTimestamp());
    contributing_sources.push_back(contributing_source);
  }
  return contributing_sources;
}

ScriptPromise RTCRtpReceiver::getStats(ScriptState* script_state) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();
  receiver_->GetStats(
      WTF::Bind(WebRTCStatsReportCallbackResolver, WrapPersistent(resolver)),
      GetExposedGroupIds(script_state));
  return promise;
}

RTCInsertableStreams* RTCRtpReceiver::createEncodedStreams(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (track_->kind() == "audio")
    return createEncodedAudioStreams(script_state, exception_state);
  DCHECK_EQ(track_->kind(), "video");
  return createEncodedVideoStreams(script_state, exception_state);
}

RTCInsertableStreams* RTCRtpReceiver::createEncodedAudioStreams(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!force_encoded_audio_insertable_streams_) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Encoded audio streams not requested at PC initialization");
    return nullptr;
  }
  if (encoded_audio_streams_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Encoded audio streams already created");
    return nullptr;
  }

  InitializeEncodedAudioStreams(script_state);
  return encoded_audio_streams_;
}

RTCInsertableStreams* RTCRtpReceiver::createEncodedVideoStreams(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!force_encoded_video_insertable_streams_) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Encoded video streams not requested at PC initialization");
    return nullptr;
  }
  if (encoded_video_streams_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Encoded video streams already created");
    return nullptr;
  }

  InitializeEncodedVideoStreams(script_state);
  return encoded_video_streams_;
}

RTCRtpReceiverPlatform* RTCRtpReceiver::platform_receiver() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return receiver_.get();
}

MediaStreamVector RTCRtpReceiver::streams() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return streams_;
}

void RTCRtpReceiver::set_streams(MediaStreamVector streams) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  streams_ = std::move(streams);
}

void RTCRtpReceiver::set_transceiver(RTCRtpTransceiver* transceiver) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  transceiver_ = transceiver;
}

void RTCRtpReceiver::set_transport(RTCDtlsTransport* transport) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  transport_ = transport;
}

void RTCRtpReceiver::UpdateSourcesIfNeeded() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!web_sources_needs_updating_)
    return;
  web_sources_ = receiver_->GetSources();
  // Clear the flag and schedule a microtask to reset it to true. This makes
  // the cache valid until the next microtask checkpoint. As such, sources
  // represent a snapshot and can be compared reliably in .js code, no risk of
  // being updated due to an RTP packet arriving. E.g.
  // "source.timestamp == source.timestamp" will always be true.
  web_sources_needs_updating_ = false;
  Microtask::EnqueueMicrotask(
      WTF::Bind(&RTCRtpReceiver::SetContributingSourcesNeedsUpdating,
                WrapWeakPersistent(this)));
}

void RTCRtpReceiver::ContextDestroyed() {
  {
    WTF::MutexLocker locker(audio_underlying_source_mutex_);
    audio_from_depacketizer_underlying_source_.Clear();
  }
  {
    WTF::MutexLocker locker(audio_underlying_sink_mutex_);
    audio_to_decoder_underlying_sink_.Clear();
  }
}

void RTCRtpReceiver::SetContributingSourcesNeedsUpdating() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  web_sources_needs_updating_ = true;
}

void RTCRtpReceiver::Trace(Visitor* visitor) const {
  visitor->Trace(pc_);
  visitor->Trace(track_);
  visitor->Trace(transport_);
  visitor->Trace(streams_);
  visitor->Trace(transceiver_);
  visitor->Trace(encoded_audio_streams_);
  visitor->Trace(video_from_depacketizer_underlying_source_);
  visitor->Trace(video_to_decoder_underlying_sink_);
  visitor->Trace(encoded_video_streams_);
  ScriptWrappable::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

RTCRtpCapabilities* RTCRtpReceiver::getCapabilities(ScriptState* state,
                                                    const String& kind) {
  if (kind != "audio" && kind != "video")
    return nullptr;

  RTCRtpCapabilities* capabilities = RTCRtpCapabilities::Create();
  capabilities->setCodecs(HeapVector<Member<RTCRtpCodecCapability>>());
  capabilities->setHeaderExtensions(
      HeapVector<Member<RTCRtpHeaderExtensionCapability>>());

  std::unique_ptr<webrtc::RtpCapabilities> rtc_capabilities =
      PeerConnectionDependencyFactory::From(*ExecutionContext::From(state))
          .GetReceiverCapabilities(kind);

  HeapVector<Member<RTCRtpCodecCapability>> codecs;
  codecs.ReserveInitialCapacity(
      SafeCast<wtf_size_t>(rtc_capabilities->codecs.size()));
  for (const auto& rtc_codec : rtc_capabilities->codecs) {
    auto* codec = RTCRtpCodecCapability::Create();
    codec->setMimeType(WTF::String::FromUTF8(rtc_codec.mime_type()));
    if (rtc_codec.clock_rate)
      codec->setClockRate(rtc_codec.clock_rate.value());
    if (rtc_codec.num_channels)
      codec->setChannels(rtc_codec.num_channels.value());
    if (!rtc_codec.parameters.empty()) {
      std::string sdp_fmtp_line;
      for (const auto& parameter : rtc_codec.parameters) {
        if (!sdp_fmtp_line.empty())
          sdp_fmtp_line += ";";
        if (parameter.first.empty()) {
          sdp_fmtp_line += parameter.second;
        } else {
          sdp_fmtp_line += parameter.first + "=" + parameter.second;
        }
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
    header_extension->setUri(WTF::String::FromUTF8(rtc_header_extension.uri));
    header_extensions.push_back(header_extension);
  }
  capabilities->setHeaderExtensions(header_extensions);

  if (IdentifiabilityStudySettings::Get()->IsTypeAllowed(
          IdentifiableSurface::Type::kRtcRtpReceiverGetCapabilities)) {
    IdentifiableTokenBuilder builder;
    IdentifiabilityAddRTCRtpCapabilitiesToBuilder(builder, *capabilities);
    IdentifiabilityMetricBuilder(ExecutionContext::From(state)->UkmSourceID())
        .Add(IdentifiableSurface::FromTypeAndToken(
                 IdentifiableSurface::Type::kRtcRtpReceiverGetCapabilities,
                 IdentifiabilityBenignStringToken(kind)),
             builder.GetToken())
        .Record(ExecutionContext::From(state)->UkmRecorder());
  }
  return capabilities;
}

RTCRtpReceiveParameters* RTCRtpReceiver::getParameters() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  RTCRtpReceiveParameters* parameters = RTCRtpReceiveParameters::Create();
  std::unique_ptr<webrtc::RtpParameters> webrtc_parameters =
      receiver_->GetParameters();

  RTCRtcpParameters* rtcp = RTCRtcpParameters::Create();
  rtcp->setReducedSize(webrtc_parameters->rtcp.reduced_size);
  parameters->setRtcp(rtcp);

  HeapVector<Member<RTCRtpDecodingParameters>> encodings;
  encodings.ReserveCapacity(
      SafeCast<wtf_size_t>(webrtc_parameters->encodings.size()));
  for (const auto& webrtc_encoding : webrtc_parameters->encodings) {
    RTCRtpDecodingParameters* encoding = RTCRtpDecodingParameters::Create();
    if (!webrtc_encoding.rid.empty()) {
      // TODO(orphis): Add rid when supported by WebRTC
    }
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

  return parameters;
}

void RTCRtpReceiver::RegisterEncodedAudioStreamCallback() {
  encoded_audio_transformer_->SetTransformerCallback(
      WTF::CrossThreadBindRepeating(
          &RTCRtpReceiver::OnAudioFrameFromDepacketizer,
          WrapCrossThreadWeakPersistent(this)));
}

void RTCRtpReceiver::UnregisterEncodedAudioStreamCallback() {
  encoded_audio_transformer_->ResetTransformerCallback();
}

void RTCRtpReceiver::SetAudioUnderlyingSource(
    RTCEncodedAudioUnderlyingSource* new_underlying_source,
    scoped_refptr<base::SingleThreadTaskRunner> new_source_task_runner) {
  if (!GetExecutionContext()) {
    // If our context is destroyed, then the RTCRtpReceiver, underlying
    // source(s), and transformer are about to be garbage collected, so there's
    // no reason to continue.
    return;
  }
  {
    WTF::MutexLocker locker(audio_underlying_source_mutex_);
    audio_from_depacketizer_underlying_source_->OnSourceTransferStarted();
    audio_from_depacketizer_underlying_source_ = new_underlying_source;
  }

  encoded_audio_transformer_->SetSourceTaskRunner(
      std::move(new_source_task_runner));
}

void RTCRtpReceiver::SetAudioUnderlyingSink(
    RTCEncodedAudioUnderlyingSink* new_underlying_sink) {
  if (!GetExecutionContext()) {
    // If our context is destroyed, then the RTCRtpReceiver and underlying
    // sink(s) are about to be garbage collected, so there's no reason to
    // continue.
    return;
  }
  WTF::MutexLocker locker(audio_underlying_sink_mutex_);
  audio_to_decoder_underlying_sink_ = new_underlying_sink;
}

void RTCRtpReceiver::InitializeEncodedAudioStreams(ScriptState* script_state) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!encoded_audio_streams_);
  DCHECK(force_encoded_audio_insertable_streams_);

  encoded_audio_streams_ = RTCInsertableStreams::Create();

  {
    WTF::MutexLocker locker(audio_underlying_source_mutex_);
    DCHECK(!audio_from_depacketizer_underlying_source_);

    // Set up readable.
    audio_from_depacketizer_underlying_source_ =
        MakeGarbageCollected<RTCEncodedAudioUnderlyingSource>(
            script_state,
            WTF::CrossThreadBindOnce(
                &RTCRtpReceiver::UnregisterEncodedAudioStreamCallback,
                WrapCrossThreadWeakPersistent(this)),
            /*is_receiver=*/true);

    auto set_underlying_source =
        WTF::CrossThreadBindRepeating(&RTCRtpReceiver::SetAudioUnderlyingSource,
                                      WrapCrossThreadWeakPersistent(this));
    auto disconnect_callback = WTF::CrossThreadBindOnce(
        &RTCRtpReceiver::UnregisterEncodedAudioStreamCallback,
        WrapCrossThreadWeakPersistent(this));
    // The high water mark for the readable stream is set to 0 so that frames
    // are removed from the queue right away, without introducing a new buffer.
    ReadableStream* readable_stream =
        ReadableStream::CreateWithCountQueueingStrategy(
            script_state, audio_from_depacketizer_underlying_source_,
            /*high_water_mark=*/0, AllowPerChunkTransferring(false),
            absl::make_unique<RtcEncodedAudioReceiverSourceOptimizer>(
                std::move(set_underlying_source),
                std::move(disconnect_callback)));
    encoded_audio_streams_->setReadable(readable_stream);
  }

  WritableStream* writable_stream;
  {
    WTF::MutexLocker locker(audio_underlying_sink_mutex_);
    DCHECK(!audio_to_decoder_underlying_sink_);

    // Set up writable.
    audio_to_decoder_underlying_sink_ =
        MakeGarbageCollected<RTCEncodedAudioUnderlyingSink>(
            script_state, encoded_audio_transformer_);

    auto set_underlying_sink =
        WTF::CrossThreadBindOnce(&RTCRtpReceiver::SetAudioUnderlyingSink,
                                 WrapCrossThreadWeakPersistent(this));

    // The high water mark for the stream is set to 1 so that the stream seems
    // ready to write, but without queuing frames.
    writable_stream = WritableStream::CreateWithCountQueueingStrategy(
        script_state, audio_to_decoder_underlying_sink_,
        /*high_water_mark=*/1,
        absl::make_unique<RtcEncodedAudioReceiverSinkOptimizer>(
            std::move(set_underlying_sink), encoded_audio_transformer_));
  }

  encoded_audio_streams_->setWritable(writable_stream);
}

void RTCRtpReceiver::OnAudioFrameFromDepacketizer(
    std::unique_ptr<webrtc::TransformableFrameInterface> encoded_audio_frame) {
  WTF::MutexLocker locker(audio_underlying_source_mutex_);
  if (audio_from_depacketizer_underlying_source_) {
    audio_from_depacketizer_underlying_source_->OnFrameFromSource(
        std::move(encoded_audio_frame));
  }
}

void RTCRtpReceiver::RegisterEncodedVideoStreamCallback() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!platform_receiver()
              ->GetEncodedVideoStreamTransformer()
              ->HasTransformerCallback());
  DCHECK_EQ(track_->kind(), "video");
  platform_receiver()
      ->GetEncodedVideoStreamTransformer()
      ->SetTransformerCallback(
          WTF::BindRepeating(&RTCRtpReceiver::OnVideoFrameFromDepacketizer,
                             WrapWeakPersistent(this)));
}

void RTCRtpReceiver::UnregisterEncodedVideoStreamCallback() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_EQ(track_->kind(), "video");
  platform_receiver()
      ->GetEncodedVideoStreamTransformer()
      ->ResetTransformerCallback();
}

void RTCRtpReceiver::InitializeEncodedVideoStreams(ScriptState* script_state) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!encoded_video_streams_);
  DCHECK(!video_from_depacketizer_underlying_source_);
  DCHECK(!video_to_decoder_underlying_sink_);
  DCHECK(force_encoded_video_insertable_streams_);

  encoded_video_streams_ = RTCInsertableStreams::Create();

  // Set up readable.
  video_from_depacketizer_underlying_source_ =
      MakeGarbageCollected<RTCEncodedVideoUnderlyingSource>(
          script_state,
          WTF::Bind(&RTCRtpReceiver::UnregisterEncodedVideoStreamCallback,
                    WrapWeakPersistent(this)));
  // The high water mark for the readable stream is set to 0 so that frames are
  // removed from the queue right away, without introducing a new buffer.
  ReadableStream* readable_stream =
      ReadableStream::CreateWithCountQueueingStrategy(
          script_state, video_from_depacketizer_underlying_source_,
          /*high_water_mark=*/0);
  encoded_video_streams_->setReadable(readable_stream);

  // Set up writable.
  video_to_decoder_underlying_sink_ =
      MakeGarbageCollected<RTCEncodedVideoUnderlyingSink>(
          script_state,
          WTF::BindRepeating(
              [](RTCRtpReceiver* receiver)
                  -> RTCEncodedVideoStreamTransformer* {
                return receiver ? receiver->platform_receiver()
                                      ->GetEncodedVideoStreamTransformer()
                                : nullptr;
              },
              WrapWeakPersistent(this)),
          webrtc::TransformableFrameInterface::Direction::kReceiver);
  // The high water mark for the stream is set to 1 so that the stream seems
  // ready to write, but without queuing frames.
  WritableStream* writable_stream =
      WritableStream::CreateWithCountQueueingStrategy(
          script_state, video_to_decoder_underlying_sink_,
          /*high_water_mark=*/1);
  encoded_video_streams_->setWritable(writable_stream);
}

void RTCRtpReceiver::OnVideoFrameFromDepacketizer(
    std::unique_ptr<webrtc::TransformableVideoFrameInterface>
        encoded_video_frame) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (video_from_depacketizer_underlying_source_) {
    video_from_depacketizer_underlying_source_->OnFrameFromSource(
        std::move(encoded_video_frame));
  }
}

}  // namespace blink
