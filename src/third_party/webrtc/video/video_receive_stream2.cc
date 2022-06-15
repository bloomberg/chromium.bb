/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/video_receive_stream2.h"

#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <memory>
#include <set>
#include <string>
#include <utility>

#include "absl/algorithm/container.h"
#include "absl/container/inlined_vector.h"
#include "absl/functional/bind_front.h"
#include "absl/types/optional.h"
#include "api/array_view.h"
#include "api/crypto/frame_decryptor_interface.h"
#include "api/scoped_refptr.h"
#include "api/sequence_checker.h"
#include "api/task_queue/task_queue_base.h"
#include "api/units/frequency.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "api/video/encoded_image.h"
#include "api/video/frame_buffer.h"
#include "api/video_codecs/h264_profile_level_id.h"
#include "api/video_codecs/sdp_video_format.h"
#include "api/video_codecs/video_codec.h"
#include "api/video_codecs/video_decoder_factory.h"
#include "api/video_codecs/video_encoder.h"
#include "call/rtp_stream_receiver_controller_interface.h"
#include "call/rtx_receive_stream.h"
#include "common_video/include/incoming_video_stream.h"
#include "modules/video_coding/frame_buffer2.h"
#include "modules/video_coding/frame_helpers.h"
#include "modules/video_coding/include/video_codec_interface.h"
#include "modules/video_coding/include/video_coding_defines.h"
#include "modules/video_coding/include/video_error_codes.h"
#include "modules/video_coding/timing/timing.h"
#include "modules/video_coding/utility/vp8_header_parser.h"
#include "rtc_base/checks.h"
#include "rtc_base/experiments/rtt_mult_experiment.h"
#include "rtc_base/location.h"
#include "rtc_base/logging.h"
#include "rtc_base/strings/string_builder.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/task_queue.h"
#include "rtc_base/task_utils/pending_task_safety_flag.h"
#include "rtc_base/task_utils/to_queued_task.h"
#include "rtc_base/thread_annotations.h"
#include "rtc_base/time_utils.h"
#include "rtc_base/trace_event.h"
#include "system_wrappers/include/clock.h"
#include "video/call_stats2.h"
#include "video/frame_decode_scheduler.h"
#include "video/frame_dumping_decoder.h"
#include "video/receive_statistics_proxy2.h"
#include "video/video_receive_stream_timeout_tracker.h"

namespace webrtc {

namespace internal {

namespace {

// The default delay before re-requesting a key frame to be sent.
constexpr TimeDelta kMaxWaitForKeyFrame = TimeDelta::Millis(200);
constexpr TimeDelta kMinBaseMinimumDelay = TimeDelta::Zero();
constexpr TimeDelta kMaxBaseMinimumDelay = TimeDelta::Seconds(10);
constexpr TimeDelta kMaxWaitForFrame = TimeDelta::Seconds(3);

// Create a decoder for the preferred codec before the stream starts and any
// other decoder lazily on demand.
constexpr int kDefaultMaximumPreStreamDecoders = 1;

// Concrete instance of RecordableEncodedFrame wrapping needed content
// from EncodedFrame.
class WebRtcRecordableEncodedFrame : public RecordableEncodedFrame {
 public:
  explicit WebRtcRecordableEncodedFrame(
      const EncodedFrame& frame,
      RecordableEncodedFrame::EncodedResolution resolution)
      : buffer_(frame.GetEncodedData()),
        render_time_ms_(frame.RenderTime()),
        codec_(frame.CodecSpecific()->codecType),
        is_key_frame_(frame.FrameType() == VideoFrameType::kVideoFrameKey),
        resolution_(resolution) {
    if (frame.ColorSpace()) {
      color_space_ = *frame.ColorSpace();
    }
  }

  // VideoEncodedSinkInterface::FrameBuffer
  rtc::scoped_refptr<const EncodedImageBufferInterface> encoded_buffer()
      const override {
    return buffer_;
  }

  absl::optional<webrtc::ColorSpace> color_space() const override {
    return color_space_;
  }

  VideoCodecType codec() const override { return codec_; }

  bool is_key_frame() const override { return is_key_frame_; }

  EncodedResolution resolution() const override { return resolution_; }

  Timestamp render_time() const override {
    return Timestamp::Millis(render_time_ms_);
  }

 private:
  rtc::scoped_refptr<EncodedImageBufferInterface> buffer_;
  int64_t render_time_ms_;
  VideoCodecType codec_;
  bool is_key_frame_;
  EncodedResolution resolution_;
  absl::optional<webrtc::ColorSpace> color_space_;
};

RenderResolution InitialDecoderResolution(const FieldTrialsView& field_trials) {
  FieldTrialOptional<int> width("w");
  FieldTrialOptional<int> height("h");
  ParseFieldTrial({&width, &height},
                  field_trials.Lookup("WebRTC-Video-InitialDecoderResolution"));
  if (width && height) {
    return RenderResolution(width.Value(), height.Value());
  }

  return RenderResolution(320, 180);
}

// Video decoder class to be used for unknown codecs. Doesn't support decoding
// but logs messages to LS_ERROR.
class NullVideoDecoder : public webrtc::VideoDecoder {
 public:
  bool Configure(const Settings& settings) override {
    RTC_LOG(LS_ERROR) << "Can't initialize NullVideoDecoder.";
    return true;
  }

  int32_t Decode(const webrtc::EncodedImage& input_image,
                 bool missing_frames,
                 int64_t render_time_ms) override {
    RTC_LOG(LS_ERROR) << "The NullVideoDecoder doesn't support decoding.";
    return WEBRTC_VIDEO_CODEC_OK;
  }

  int32_t RegisterDecodeCompleteCallback(
      webrtc::DecodedImageCallback* callback) override {
    RTC_LOG(LS_ERROR)
        << "Can't register decode complete callback on NullVideoDecoder.";
    return WEBRTC_VIDEO_CODEC_OK;
  }

  int32_t Release() override { return WEBRTC_VIDEO_CODEC_OK; }

  const char* ImplementationName() const override { return "NullVideoDecoder"; }
};

bool IsKeyFrameAndUnspecifiedResolution(const EncodedFrame& frame) {
  return frame.FrameType() == VideoFrameType::kVideoFrameKey &&
         frame.EncodedImage()._encodedWidth == 0 &&
         frame.EncodedImage()._encodedHeight == 0;
}

// TODO(https://bugs.webrtc.org/9974): Consider removing this workaround.
// Maximum time between frames before resetting the FrameBuffer to avoid RTP
// timestamps wraparound to affect FrameBuffer.
constexpr TimeDelta kInactiveStreamThreshold = TimeDelta::Minutes(10);

std::string OptionalDelayToLogString(const absl::optional<TimeDelta> opt) {
  return opt.has_value() ? ToLogString(*opt) : "<unset>";
}

}  // namespace

TimeDelta DetermineMaxWaitForFrame(
    const VideoReceiveStreamInterface::Config& config,
    bool is_keyframe) {
  // A (arbitrary) conversion factor between the remotely signalled NACK buffer
  // time (if not present defaults to 1000ms) and the maximum time we wait for a
  // remote frame. Chosen to not change existing defaults when using not
  // rtx-time.
  const int conversion_factor = 3;
  const TimeDelta rtp_history =
      TimeDelta::Millis(config.rtp.nack.rtp_history_ms);

  if (rtp_history > TimeDelta::Zero() &&
      conversion_factor * rtp_history < kMaxWaitForFrame) {
    return is_keyframe ? rtp_history : conversion_factor * rtp_history;
  }
  return is_keyframe ? kMaxWaitForKeyFrame : kMaxWaitForFrame;
}

VideoReceiveStream2::VideoReceiveStream2(
    TaskQueueFactory* task_queue_factory,
    Call* call,
    int num_cpu_cores,
    PacketRouter* packet_router,
    VideoReceiveStreamInterface::Config config,
    CallStats* call_stats,
    Clock* clock,
    std::unique_ptr<VCMTiming> timing,
    NackPeriodicProcessor* nack_periodic_processor,
    DecodeSynchronizer* decode_sync)
    : task_queue_factory_(task_queue_factory),
      transport_adapter_(config.rtcp_send_transport),
      config_(std::move(config)),
      num_cpu_cores_(num_cpu_cores),
      call_(call),
      clock_(clock),
      call_stats_(call_stats),
      source_tracker_(clock_),
      stats_proxy_(remote_ssrc(),
                   clock_,
                   call->worker_thread(),
                   call->trials()),
      rtp_receive_statistics_(ReceiveStatistics::Create(clock_)),
      timing_(std::move(timing)),
      video_receiver_(clock_, timing_.get(), call->trials()),
      rtp_video_stream_receiver_(call->worker_thread(),
                                 clock_,
                                 &transport_adapter_,
                                 call_stats->AsRtcpRttStats(),
                                 packet_router,
                                 &config_,
                                 rtp_receive_statistics_.get(),
                                 &stats_proxy_,
                                 &stats_proxy_,
                                 nack_periodic_processor,
                                 this,     // NackSender
                                 nullptr,  // Use default KeyFrameRequestSender
                                 this,     // OnCompleteFrameCallback
                                 std::move(config_.frame_decryptor),
                                 std::move(config_.frame_transformer),
                                 call->trials()),
      rtp_stream_sync_(call->worker_thread(), this),
      max_wait_for_keyframe_(DetermineMaxWaitForFrame(config_, true)),
      max_wait_for_frame_(DetermineMaxWaitForFrame(config_, false)),
      maximum_pre_stream_decoders_("max", kDefaultMaximumPreStreamDecoders),
      decode_sync_(decode_sync),
      decode_queue_(task_queue_factory_->CreateTaskQueue(
          "DecodingQueue",
          TaskQueueFactory::Priority::HIGH)) {
  RTC_LOG(LS_INFO) << "VideoReceiveStream2: " << config_.ToString();

  RTC_DCHECK(call_->worker_thread());
  RTC_DCHECK(config_.renderer);
  RTC_DCHECK(call_stats_);
  packet_sequence_checker_.Detach();

  RTC_DCHECK(!config_.decoders.empty());
  RTC_CHECK(config_.decoder_factory);
  std::set<int> decoder_payload_types;
  for (const Decoder& decoder : config_.decoders) {
    RTC_CHECK(decoder_payload_types.find(decoder.payload_type) ==
              decoder_payload_types.end())
        << "Duplicate payload type (" << decoder.payload_type
        << ") for different decoders.";
    decoder_payload_types.insert(decoder.payload_type);
  }

  timing_->set_render_delay(TimeDelta::Millis(config_.render_delay_ms));

  frame_buffer_ = FrameBufferProxy::CreateFromFieldTrial(
      clock_, call_->worker_thread(), timing_.get(), &stats_proxy_,
      &decode_queue_, this, max_wait_for_keyframe_, max_wait_for_frame_,
      decode_sync_, call_->trials());

  if (rtx_ssrc()) {
    rtx_receive_stream_ = std::make_unique<RtxReceiveStream>(
        &rtp_video_stream_receiver_, config_.rtp.rtx_associated_payload_types,
        remote_ssrc(), rtp_receive_statistics_.get());
  } else {
    rtp_receive_statistics_->EnableRetransmitDetection(remote_ssrc(), true);
  }

  ParseFieldTrial(
      {
          &maximum_pre_stream_decoders_,
      },
      call_->trials().Lookup("WebRTC-PreStreamDecoders"));
}

VideoReceiveStream2::~VideoReceiveStream2() {
  RTC_DCHECK_RUN_ON(&worker_sequence_checker_);
  RTC_LOG(LS_INFO) << "~VideoReceiveStream2: " << config_.ToString();
  RTC_DCHECK(!media_receiver_);
  RTC_DCHECK(!rtx_receiver_);
  Stop();
}

void VideoReceiveStream2::RegisterWithTransport(
    RtpStreamReceiverControllerInterface* receiver_controller) {
  RTC_DCHECK_RUN_ON(&packet_sequence_checker_);
  RTC_DCHECK(!media_receiver_);
  RTC_DCHECK(!rtx_receiver_);

  // Register with RtpStreamReceiverController.
  media_receiver_ = receiver_controller->CreateReceiver(
      remote_ssrc(), &rtp_video_stream_receiver_);
  if (rtx_ssrc()) {
    RTC_DCHECK(rtx_receive_stream_);
    rtx_receiver_ = receiver_controller->CreateReceiver(
        rtx_ssrc(), rtx_receive_stream_.get());
  }
}

void VideoReceiveStream2::UnregisterFromTransport() {
  RTC_DCHECK_RUN_ON(&packet_sequence_checker_);
  media_receiver_.reset();
  rtx_receiver_.reset();
}

const std::string& VideoReceiveStream2::sync_group() const {
  RTC_DCHECK_RUN_ON(&packet_sequence_checker_);
  return config_.sync_group;
}

void VideoReceiveStream2::SignalNetworkState(NetworkState state) {
  RTC_DCHECK_RUN_ON(&worker_sequence_checker_);
  rtp_video_stream_receiver_.SignalNetworkState(state);
}

bool VideoReceiveStream2::DeliverRtcp(const uint8_t* packet, size_t length) {
  RTC_DCHECK_RUN_ON(&packet_sequence_checker_);
  return rtp_video_stream_receiver_.DeliverRtcp(packet, length);
}

void VideoReceiveStream2::SetSync(Syncable* audio_syncable) {
  RTC_DCHECK_RUN_ON(&packet_sequence_checker_);
  rtp_stream_sync_.ConfigureSync(audio_syncable);
}

void VideoReceiveStream2::SetLocalSsrc(uint32_t local_ssrc) {
  RTC_DCHECK_RUN_ON(&packet_sequence_checker_);
  if (config_.rtp.local_ssrc == local_ssrc)
    return;

  // TODO(tommi): Make sure we don't rely on local_ssrc via the config struct.
  const_cast<uint32_t&>(config_.rtp.local_ssrc) = local_ssrc;
  rtp_video_stream_receiver_.OnLocalSsrcChange(local_ssrc);
}

void VideoReceiveStream2::Start() {
  RTC_DCHECK_RUN_ON(&worker_sequence_checker_);

  if (decoder_running_) {
    return;
  }

  const bool protected_by_fec = config_.rtp.protected_by_flexfec ||
                                rtp_video_stream_receiver_.IsUlpfecEnabled();

  if (rtp_video_stream_receiver_.IsRetransmissionsEnabled() &&
      protected_by_fec) {
    frame_buffer_->SetProtectionMode(kProtectionNackFEC);
  }

  transport_adapter_.Enable();
  rtc::VideoSinkInterface<VideoFrame>* renderer = nullptr;
  if (config_.enable_prerenderer_smoothing) {
    incoming_video_stream_.reset(new IncomingVideoStream(
        task_queue_factory_, config_.render_delay_ms, this));
    renderer = incoming_video_stream_.get();
  } else {
    renderer = this;
  }

  int decoders_count = 0;
  for (const Decoder& decoder : config_.decoders) {
    // Create up to maximum_pre_stream_decoders_ up front, wait the the other
    // decoders until they are requested (i.e., we receive the corresponding
    // payload).
    if (decoders_count < maximum_pre_stream_decoders_) {
      CreateAndRegisterExternalDecoder(decoder);
      ++decoders_count;
    }

    VideoDecoder::Settings settings;
    settings.set_codec_type(
        PayloadStringToCodecType(decoder.video_format.name));
    settings.set_max_render_resolution(
        InitialDecoderResolution(call_->trials()));
    settings.set_number_of_cores(num_cpu_cores_);

    const bool raw_payload =
        config_.rtp.raw_payload_types.count(decoder.payload_type) > 0;
    {
      // TODO(bugs.webrtc.org/11993): Make this call on the network thread.
      RTC_DCHECK_RUN_ON(&packet_sequence_checker_);
      rtp_video_stream_receiver_.AddReceiveCodec(
          decoder.payload_type, settings.codec_type(),
          decoder.video_format.parameters, raw_payload);
    }
    video_receiver_.RegisterReceiveCodec(decoder.payload_type, settings);
  }

  RTC_DCHECK(renderer != nullptr);
  video_stream_decoder_.reset(
      new VideoStreamDecoder(&video_receiver_, &stats_proxy_, renderer));

  // Make sure we register as a stats observer *after* we've prepared the
  // `video_stream_decoder_`.
  call_stats_->RegisterStatsObserver(this);

  // Start decoding on task queue.
  video_receiver_.DecoderThreadStarting();
  stats_proxy_.DecoderThreadStarting();
  decode_queue_.PostTask([this] {
    RTC_DCHECK_RUN_ON(&decode_queue_);
    decoder_stopped_ = false;
  });
  frame_buffer_->StartNextDecode(true);
  decoder_running_ = true;

  {
    // TODO(bugs.webrtc.org/11993): Make this call on the network thread.
    RTC_DCHECK_RUN_ON(&packet_sequence_checker_);
    rtp_video_stream_receiver_.StartReceive();
  }
}

void VideoReceiveStream2::Stop() {
  RTC_DCHECK_RUN_ON(&worker_sequence_checker_);
  {
    // TODO(bugs.webrtc.org/11993): Make this call on the network thread.
    // Also call `GetUniqueFramesSeen()` at the same time (since it's a counter
    // that's updated on the network thread).
    RTC_DCHECK_RUN_ON(&packet_sequence_checker_);
    rtp_video_stream_receiver_.StopReceive();
  }

  stats_proxy_.OnUniqueFramesCounted(
      rtp_video_stream_receiver_.GetUniqueFramesSeen());

  frame_buffer_->StopOnWorker();
  call_stats_->DeregisterStatsObserver(this);
  if (decoder_running_) {
    rtc::Event done;
    decode_queue_.PostTask([this, &done] {
      RTC_DCHECK_RUN_ON(&decode_queue_);
      decoder_stopped_ = true;
      done.Set();
    });
    done.Wait(rtc::Event::kForever);

    decoder_running_ = false;
    video_receiver_.DecoderThreadStopped();
    stats_proxy_.DecoderThreadStopped();
    // Deregister external decoders so they are no longer running during
    // destruction. This effectively stops the VCM since the decoder thread is
    // stopped, the VCM is deregistered and no asynchronous decoder threads are
    // running.
    for (const Decoder& decoder : config_.decoders)
      video_receiver_.RegisterExternalDecoder(nullptr, decoder.payload_type);

    UpdateHistograms();
  }

  video_stream_decoder_.reset();
  incoming_video_stream_.reset();
  transport_adapter_.Disable();
}

void VideoReceiveStream2::SetRtpExtensions(
    std::vector<RtpExtension> extensions) {
  RTC_DCHECK_RUN_ON(&packet_sequence_checker_);
  rtp_video_stream_receiver_.SetRtpExtensions(extensions);
  // TODO(tommi): We don't use the `c.rtp.extensions` member in the
  // VideoReceiveStream2 class, so this const_cast<> is a temporary hack to keep
  // things consistent between VideoReceiveStream2 and RtpVideoStreamReceiver2
  // for debugging purposes. The `packet_sequence_checker_` gives us assurances
  // that from a threading perspective, this is still safe. The accessors that
  // give read access to this state, run behind the same check.
  // The alternative to the const_cast<> would be to make `config_` non-const
  // and guarded by `packet_sequence_checker_`. However the scope of that state
  // is huge (the whole Config struct), and would require all methods that touch
  // the struct to abide the needs of the `extensions` member.
  const_cast<std::vector<RtpExtension>&>(config_.rtp.extensions) =
      std::move(extensions);
}

RtpHeaderExtensionMap VideoReceiveStream2::GetRtpExtensionMap() const {
  RTC_DCHECK_RUN_ON(&packet_sequence_checker_);
  return rtp_video_stream_receiver_.GetRtpExtensions();
}

bool VideoReceiveStream2::transport_cc() const {
  RTC_DCHECK_RUN_ON(&packet_sequence_checker_);
  return config_.rtp.transport_cc;
}

void VideoReceiveStream2::SetTransportCc(bool transport_cc) {
  RTC_DCHECK_RUN_ON(&packet_sequence_checker_);
  // TODO(tommi): Stop using the config struct for the internal state.
  const_cast<bool&>(config_.rtp.transport_cc) = transport_cc;
}

void VideoReceiveStream2::CreateAndRegisterExternalDecoder(
    const Decoder& decoder) {
  TRACE_EVENT0("webrtc",
               "VideoReceiveStream2::CreateAndRegisterExternalDecoder");
  std::unique_ptr<VideoDecoder> video_decoder =
      config_.decoder_factory->CreateVideoDecoder(decoder.video_format);
  // If we still have no valid decoder, we have to create a "Null" decoder
  // that ignores all calls. The reason we can get into this state is that the
  // old decoder factory interface doesn't have a way to query supported
  // codecs.
  if (!video_decoder) {
    video_decoder = std::make_unique<NullVideoDecoder>();
  }

  std::string decoded_output_file =
      call_->trials().Lookup("WebRTC-DecoderDataDumpDirectory");
  // Because '/' can't be used inside a field trial parameter, we use ';'
  // instead.
  // This is only relevant to WebRTC-DecoderDataDumpDirectory
  // field trial. ';' is chosen arbitrary. Even though it's a legal character
  // in some file systems, we can sacrifice ability to use it in the path to
  // dumped video, since it's developers-only feature for debugging.
  absl::c_replace(decoded_output_file, ';', '/');
  if (!decoded_output_file.empty()) {
    char filename_buffer[256];
    rtc::SimpleStringBuilder ssb(filename_buffer);
    ssb << decoded_output_file << "/webrtc_receive_stream_" << remote_ssrc()
        << "-" << rtc::TimeMicros() << ".ivf";
    video_decoder = CreateFrameDumpingDecoderWrapper(
        std::move(video_decoder), FileWrapper::OpenWriteOnly(ssb.str()));
  }

  video_decoders_.push_back(std::move(video_decoder));
  video_receiver_.RegisterExternalDecoder(video_decoders_.back().get(),
                                          decoder.payload_type);
}

VideoReceiveStreamInterface::Stats VideoReceiveStream2::GetStats() const {
  RTC_DCHECK_RUN_ON(&worker_sequence_checker_);
  VideoReceiveStream2::Stats stats = stats_proxy_.GetStats();
  stats.total_bitrate_bps = 0;
  StreamStatistician* statistician =
      rtp_receive_statistics_->GetStatistician(stats.ssrc);
  if (statistician) {
    stats.rtp_stats = statistician->GetStats();
    stats.total_bitrate_bps = statistician->BitrateReceived();
  }
  if (rtx_ssrc()) {
    StreamStatistician* rtx_statistician =
        rtp_receive_statistics_->GetStatistician(rtx_ssrc());
    if (rtx_statistician)
      stats.total_bitrate_bps += rtx_statistician->BitrateReceived();
  }
  return stats;
}

void VideoReceiveStream2::UpdateHistograms() {
  RTC_DCHECK_RUN_ON(&worker_sequence_checker_);
  absl::optional<int> fraction_lost;
  StreamDataCounters rtp_stats;
  StreamStatistician* statistician =
      rtp_receive_statistics_->GetStatistician(remote_ssrc());
  if (statistician) {
    fraction_lost = statistician->GetFractionLostInPercent();
    rtp_stats = statistician->GetReceiveStreamDataCounters();
  }
  if (rtx_ssrc()) {
    StreamStatistician* rtx_statistician =
        rtp_receive_statistics_->GetStatistician(rtx_ssrc());
    if (rtx_statistician) {
      StreamDataCounters rtx_stats =
          rtx_statistician->GetReceiveStreamDataCounters();
      stats_proxy_.UpdateHistograms(fraction_lost, rtp_stats, &rtx_stats);
      return;
    }
  }
  stats_proxy_.UpdateHistograms(fraction_lost, rtp_stats, nullptr);
}

bool VideoReceiveStream2::SetBaseMinimumPlayoutDelayMs(int delay_ms) {
  RTC_DCHECK_RUN_ON(&worker_sequence_checker_);
  TimeDelta delay = TimeDelta::Millis(delay_ms);
  if (delay < kMinBaseMinimumDelay || delay > kMaxBaseMinimumDelay) {
    return false;
  }

  base_minimum_playout_delay_ = delay;
  UpdatePlayoutDelays();
  return true;
}

int VideoReceiveStream2::GetBaseMinimumPlayoutDelayMs() const {
  RTC_DCHECK_RUN_ON(&worker_sequence_checker_);
  constexpr TimeDelta kDefaultBaseMinPlayoutDelay = TimeDelta::Millis(-1);
  // Unset must be -1.
  static_assert(-1 == kDefaultBaseMinPlayoutDelay.ms(), "");
  return base_minimum_playout_delay_.value_or(kDefaultBaseMinPlayoutDelay).ms();
}

void VideoReceiveStream2::OnFrame(const VideoFrame& video_frame) {
  VideoFrameMetaData frame_meta(video_frame, clock_->CurrentTime());

  // TODO(bugs.webrtc.org/10739): we should set local capture clock offset for
  // `video_frame.packet_infos`. But VideoFrame is const qualified here.

  call_->worker_thread()->PostTask(
      ToQueuedTask(task_safety_, [frame_meta, this]() {
        RTC_DCHECK_RUN_ON(&worker_sequence_checker_);
        int64_t video_playout_ntp_ms;
        int64_t sync_offset_ms;
        double estimated_freq_khz;
        if (rtp_stream_sync_.GetStreamSyncOffsetInMs(
                frame_meta.rtp_timestamp, frame_meta.render_time_ms(),
                &video_playout_ntp_ms, &sync_offset_ms, &estimated_freq_khz)) {
          stats_proxy_.OnSyncOffsetUpdated(video_playout_ntp_ms, sync_offset_ms,
                                           estimated_freq_khz);
        }
        stats_proxy_.OnRenderedFrame(frame_meta);
      }));

  source_tracker_.OnFrameDelivered(video_frame.packet_infos());
  config_.renderer->OnFrame(video_frame);
  webrtc::MutexLock lock(&pending_resolution_mutex_);
  if (pending_resolution_.has_value()) {
    if (!pending_resolution_->empty() &&
        (video_frame.width() != static_cast<int>(pending_resolution_->width) ||
         video_frame.height() !=
             static_cast<int>(pending_resolution_->height))) {
      RTC_LOG(LS_WARNING)
          << "Recordable encoded frame stream resolution was reported as "
          << pending_resolution_->width << "x" << pending_resolution_->height
          << " but the stream is now " << video_frame.width()
          << video_frame.height();
    }
    pending_resolution_ = RecordableEncodedFrame::EncodedResolution{
        static_cast<unsigned>(video_frame.width()),
        static_cast<unsigned>(video_frame.height())};
  }
}

void VideoReceiveStream2::SetFrameDecryptor(
    rtc::scoped_refptr<webrtc::FrameDecryptorInterface> frame_decryptor) {
  rtp_video_stream_receiver_.SetFrameDecryptor(std::move(frame_decryptor));
}

void VideoReceiveStream2::SetDepacketizerToDecoderFrameTransformer(
    rtc::scoped_refptr<FrameTransformerInterface> frame_transformer) {
  rtp_video_stream_receiver_.SetDepacketizerToDecoderFrameTransformer(
      std::move(frame_transformer));
}

void VideoReceiveStream2::SendNack(
    const std::vector<uint16_t>& sequence_numbers,
    bool buffering_allowed) {
  RTC_DCHECK_RUN_ON(&worker_sequence_checker_);
  RTC_DCHECK(buffering_allowed);
  rtp_video_stream_receiver_.RequestPacketRetransmit(sequence_numbers);
}

void VideoReceiveStream2::RequestKeyFrame(Timestamp now) {
  // Running on worker_sequence_checker_.
  // Called from RtpVideoStreamReceiver (rtp_video_stream_receiver_ is
  // ultimately responsible).
  rtp_video_stream_receiver_.RequestKeyFrame();
  decode_queue_.PostTask([this, now]() {
    RTC_DCHECK_RUN_ON(&decode_queue_);
    last_keyframe_request_ = now;
  });
}

void VideoReceiveStream2::OnCompleteFrame(std::unique_ptr<EncodedFrame> frame) {
  RTC_DCHECK_RUN_ON(&worker_sequence_checker_);

  // TODO(https://bugs.webrtc.org/9974): Consider removing this workaround.
  // TODO(https://bugs.webrtc.org/13343): Remove this check when FrameBuffer3 is
  // deployed. With FrameBuffer3, this case is properly handled and tested in
  // the FrameBufferProxyTest.PausedStream unit test.
  Timestamp time_now = clock_->CurrentTime();
  if (last_complete_frame_time_ &&
      time_now - *last_complete_frame_time_ > kInactiveStreamThreshold) {
    frame_buffer_->Clear();
  }
  last_complete_frame_time_ = time_now;

  const VideoPlayoutDelay& playout_delay = frame->EncodedImage().playout_delay_;
  if (playout_delay.min_ms >= 0) {
    frame_minimum_playout_delay_ = TimeDelta::Millis(playout_delay.min_ms);
    UpdatePlayoutDelays();
  }
  if (playout_delay.max_ms >= 0) {
    frame_maximum_playout_delay_ = TimeDelta::Millis(playout_delay.max_ms);
    UpdatePlayoutDelays();
  }

  auto last_continuous_pid = frame_buffer_->InsertFrame(std::move(frame));
  if (last_continuous_pid.has_value()) {
    {
      // TODO(bugs.webrtc.org/11993): Call on the network thread.
      RTC_DCHECK_RUN_ON(&packet_sequence_checker_);
      rtp_video_stream_receiver_.FrameContinuous(*last_continuous_pid);
    }
  }
}

void VideoReceiveStream2::OnRttUpdate(int64_t avg_rtt_ms, int64_t max_rtt_ms) {
  RTC_DCHECK_RUN_ON(&worker_sequence_checker_);
  // TODO(bugs.webrtc.org/13757): Replace with TimeDelta.
  frame_buffer_->UpdateRtt(max_rtt_ms);
  rtp_video_stream_receiver_.UpdateRtt(max_rtt_ms);
  stats_proxy_.OnRttUpdate(avg_rtt_ms);
}

uint32_t VideoReceiveStream2::id() const {
  RTC_DCHECK_RUN_ON(&worker_sequence_checker_);
  return remote_ssrc();
}

absl::optional<Syncable::Info> VideoReceiveStream2::GetInfo() const {
  RTC_DCHECK_RUN_ON(&packet_sequence_checker_);
  absl::optional<Syncable::Info> info =
      rtp_video_stream_receiver_.GetSyncInfo();

  if (!info)
    return absl::nullopt;

  info->current_delay_ms = timing_->TargetVideoDelay().ms();
  return info;
}

bool VideoReceiveStream2::GetPlayoutRtpTimestamp(uint32_t* rtp_timestamp,
                                                 int64_t* time_ms) const {
  RTC_DCHECK_NOTREACHED();
  return false;
}

void VideoReceiveStream2::SetEstimatedPlayoutNtpTimestampMs(
    int64_t ntp_timestamp_ms,
    int64_t time_ms) {
  RTC_DCHECK_NOTREACHED();
}

bool VideoReceiveStream2::SetMinimumPlayoutDelay(int delay_ms) {
  RTC_DCHECK_RUN_ON(&worker_sequence_checker_);
  syncable_minimum_playout_delay_ = TimeDelta::Millis(delay_ms);
  UpdatePlayoutDelays();
  return true;
}

TimeDelta VideoReceiveStream2::GetMaxWait() const {
  return keyframe_required_ ? max_wait_for_keyframe_ : max_wait_for_frame_;
}

void VideoReceiveStream2::OnEncodedFrame(std::unique_ptr<EncodedFrame> frame) {
  if (!decode_queue_.IsCurrent()) {
    decode_queue_.PostTask([this, frame = std::move(frame)]() mutable {
      OnEncodedFrame(std::move(frame));
    });
    return;
  }
  RTC_DCHECK_RUN_ON(&decode_queue_);
  if (decoder_stopped_)
    return;
  HandleEncodedFrame(std::move(frame));
  frame_buffer_->StartNextDecode(keyframe_required_);
}

void VideoReceiveStream2::OnDecodableFrameTimeout(TimeDelta wait_time) {
  if (!call_->worker_thread()->IsCurrent()) {
    call_->worker_thread()->PostTask(ToQueuedTask(
        task_safety_,
        [this, wait_time] { OnDecodableFrameTimeout(wait_time); }));
    return;
  }

  // TODO(bugs.webrtc.org/11993): PostTask to the network thread.
  RTC_DCHECK_RUN_ON(&packet_sequence_checker_);
  HandleFrameBufferTimeout(clock_->CurrentTime(), wait_time);

  decode_queue_.PostTask([this] {
    RTC_DCHECK_RUN_ON(&decode_queue_);
    frame_buffer_->StartNextDecode(keyframe_required_);
  });
}

void VideoReceiveStream2::HandleEncodedFrame(
    std::unique_ptr<EncodedFrame> frame) {
  // Running on `decode_queue_`.
  Timestamp now = clock_->CurrentTime();

  // Current OnPreDecode only cares about QP for VP8.
  int qp = -1;
  if (frame->CodecSpecific()->codecType == kVideoCodecVP8) {
    if (!vp8::GetQp(frame->data(), frame->size(), &qp)) {
      RTC_LOG(LS_WARNING) << "Failed to extract QP from VP8 video frame";
    }
  }
  stats_proxy_.OnPreDecode(frame->CodecSpecific()->codecType, qp);

  bool force_request_key_frame = false;
  int64_t decoded_frame_picture_id = -1;

  const bool keyframe_request_is_due =
      !last_keyframe_request_ ||
      now >= (*last_keyframe_request_ + max_wait_for_keyframe_);

  if (!video_receiver_.IsExternalDecoderRegistered(frame->PayloadType())) {
    // Look for the decoder with this payload type.
    for (const Decoder& decoder : config_.decoders) {
      if (decoder.payload_type == frame->PayloadType()) {
        CreateAndRegisterExternalDecoder(decoder);
        break;
      }
    }
  }

  int64_t frame_id = frame->Id();
  bool received_frame_is_keyframe =
      frame->FrameType() == VideoFrameType::kVideoFrameKey;
  int decode_result = DecodeAndMaybeDispatchEncodedFrame(std::move(frame));
  if (decode_result == WEBRTC_VIDEO_CODEC_OK ||
      decode_result == WEBRTC_VIDEO_CODEC_OK_REQUEST_KEYFRAME) {
    keyframe_required_ = false;
    frame_decoded_ = true;

    decoded_frame_picture_id = frame_id;

    if (decode_result == WEBRTC_VIDEO_CODEC_OK_REQUEST_KEYFRAME)
      force_request_key_frame = true;
  } else if (!frame_decoded_ || !keyframe_required_ ||
             keyframe_request_is_due) {
    keyframe_required_ = true;
    // TODO(philipel): Remove this keyframe request when downstream project
    //                 has been fixed.
    force_request_key_frame = true;
  }

  {
    // TODO(bugs.webrtc.org/11993): Make this PostTask to the network thread.
    call_->worker_thread()->PostTask(ToQueuedTask(
        task_safety_,
        [this, now, received_frame_is_keyframe, force_request_key_frame,
         decoded_frame_picture_id, keyframe_request_is_due]() {
          RTC_DCHECK_RUN_ON(&packet_sequence_checker_);

          if (decoded_frame_picture_id != -1)
            rtp_video_stream_receiver_.FrameDecoded(decoded_frame_picture_id);

          HandleKeyFrameGeneration(received_frame_is_keyframe, now,
                                   force_request_key_frame,
                                   keyframe_request_is_due);
        }));
  }
}

int VideoReceiveStream2::DecodeAndMaybeDispatchEncodedFrame(
    std::unique_ptr<EncodedFrame> frame) {
  // Running on decode_queue_.

  // If `buffered_encoded_frames_` grows out of control (=60 queued frames),
  // maybe due to a stuck decoder, we just halt the process here and log the
  // error.
  const bool encoded_frame_output_enabled =
      encoded_frame_buffer_function_ != nullptr &&
      buffered_encoded_frames_.size() < kBufferedEncodedFramesMaxSize;
  EncodedFrame* frame_ptr = frame.get();
  if (encoded_frame_output_enabled) {
    // If we receive a key frame with unset resolution, hold on dispatching the
    // frame and following ones until we know a resolution of the stream.
    // NOTE: The code below has a race where it can report the wrong
    // resolution for keyframes after an initial keyframe of other resolution.
    // However, the only known consumer of this information is the W3C
    // MediaRecorder and it will only use the resolution in the first encoded
    // keyframe from WebRTC, so misreporting is fine.
    buffered_encoded_frames_.push_back(std::move(frame));
    if (buffered_encoded_frames_.size() == kBufferedEncodedFramesMaxSize)
      RTC_LOG(LS_ERROR) << "About to halt recordable encoded frame output due "
                           "to too many buffered frames.";

    webrtc::MutexLock lock(&pending_resolution_mutex_);
    if (IsKeyFrameAndUnspecifiedResolution(*frame_ptr) &&
        !pending_resolution_.has_value())
      pending_resolution_.emplace();
  }

  int decode_result = video_receiver_.Decode(frame_ptr);
  if (encoded_frame_output_enabled) {
    absl::optional<RecordableEncodedFrame::EncodedResolution>
        pending_resolution;
    {
      // Fish out `pending_resolution_` to avoid taking the mutex on every lap
      // or dispatching under the mutex in the flush loop.
      webrtc::MutexLock lock(&pending_resolution_mutex_);
      if (pending_resolution_.has_value())
        pending_resolution = *pending_resolution_;
    }
    if (!pending_resolution.has_value() || !pending_resolution->empty()) {
      // Flush the buffered frames.
      for (const auto& frame : buffered_encoded_frames_) {
        RecordableEncodedFrame::EncodedResolution resolution{
            frame->EncodedImage()._encodedWidth,
            frame->EncodedImage()._encodedHeight};
        if (IsKeyFrameAndUnspecifiedResolution(*frame)) {
          RTC_DCHECK(!pending_resolution->empty());
          resolution = *pending_resolution;
        }
        encoded_frame_buffer_function_(
            WebRtcRecordableEncodedFrame(*frame, resolution));
      }
      buffered_encoded_frames_.clear();
    }
  }
  return decode_result;
}

// RTC_RUN_ON(packet_sequence_checker_)
void VideoReceiveStream2::HandleKeyFrameGeneration(
    bool received_frame_is_keyframe,
    Timestamp now,
    bool always_request_key_frame,
    bool keyframe_request_is_due) {
  bool request_key_frame = always_request_key_frame;

  // Repeat sending keyframe requests if we've requested a keyframe.
  if (keyframe_generation_requested_) {
    if (received_frame_is_keyframe) {
      keyframe_generation_requested_ = false;
    } else if (keyframe_request_is_due) {
      if (!IsReceivingKeyFrame(now)) {
        request_key_frame = true;
      }
    } else {
      // It hasn't been long enough since the last keyframe request, do nothing.
    }
  }

  if (request_key_frame) {
    // HandleKeyFrameGeneration is initiated from the decode thread -
    // RequestKeyFrame() triggers a call back to the decode thread.
    // Perhaps there's a way to avoid that.
    RequestKeyFrame(now);
  }
}

// RTC_RUN_ON(packet_sequence_checker_)
void VideoReceiveStream2::HandleFrameBufferTimeout(Timestamp now,
                                                   TimeDelta wait) {
  absl::optional<int64_t> last_packet_ms =
      rtp_video_stream_receiver_.LastReceivedPacketMs();

  // To avoid spamming keyframe requests for a stream that is not active we
  // check if we have received a packet within the last 5 seconds.
  constexpr TimeDelta kInactiveDuraction = TimeDelta::Seconds(5);
  const bool stream_is_active =
      last_packet_ms &&
      now - Timestamp::Millis(*last_packet_ms) < kInactiveDuraction;
  if (!stream_is_active)
    stats_proxy_.OnStreamInactive();

  if (stream_is_active && !IsReceivingKeyFrame(now) &&
      (!config_.crypto_options.sframe.require_frame_encryption ||
       rtp_video_stream_receiver_.IsDecryptable())) {
    RTC_LOG(LS_WARNING) << "No decodable frame in " << wait
                        << ", requesting keyframe.";
    RequestKeyFrame(now);
  }
}

// RTC_RUN_ON(packet_sequence_checker_)
bool VideoReceiveStream2::IsReceivingKeyFrame(Timestamp now) const {
  absl::optional<int64_t> last_keyframe_packet_ms =
      rtp_video_stream_receiver_.LastReceivedKeyframePacketMs();

  // If we recently have been receiving packets belonging to a keyframe then
  // we assume a keyframe is currently being received.
  bool receiving_keyframe = last_keyframe_packet_ms &&
                            now - Timestamp::Millis(*last_keyframe_packet_ms) <
                                max_wait_for_keyframe_;
  return receiving_keyframe;
}

void VideoReceiveStream2::UpdatePlayoutDelays() const {
  // Running on worker_sequence_checker_.
  const std::initializer_list<absl::optional<TimeDelta>> min_delays = {
      frame_minimum_playout_delay_, base_minimum_playout_delay_,
      syncable_minimum_playout_delay_};
  // Since nullopt < anything, this will return the largest of the minumum
  // delays, or nullopt if all are nullopt.
  absl::optional<TimeDelta> minimum_delay = std::max(min_delays);
  if (minimum_delay) {
    auto num_playout_delays_set =
        absl::c_count_if(min_delays, [](auto opt) { return opt.has_value(); });
    if (num_playout_delays_set > 1 &&
        timing_->min_playout_delay() != minimum_delay) {
      RTC_LOG(LS_WARNING)
          << "Multiple playout delays set. Actual delay value set to "
          << *minimum_delay << " frame min delay="
          << OptionalDelayToLogString(frame_maximum_playout_delay_)
          << " base min delay="
          << OptionalDelayToLogString(base_minimum_playout_delay_)
          << " sync min delay="
          << OptionalDelayToLogString(syncable_minimum_playout_delay_);
    }
    timing_->set_min_playout_delay(*minimum_delay);
    if (frame_minimum_playout_delay_ == TimeDelta::Zero() &&
        frame_maximum_playout_delay_ > TimeDelta::Zero()) {
      // TODO(kron): Estimate frame rate from video stream.
      constexpr Frequency kFrameRate = Frequency::Hertz(60);
      // Convert playout delay in ms to number of frames.
      int max_composition_delay_in_frames =
          std::lrint(*frame_maximum_playout_delay_ * kFrameRate);
      // Subtract frames in buffer.
      max_composition_delay_in_frames =
          std::max(max_composition_delay_in_frames - frame_buffer_->Size(), 0);
      timing_->SetMaxCompositionDelayInFrames(max_composition_delay_in_frames);
    }
  }

  if (frame_maximum_playout_delay_) {
    timing_->set_max_playout_delay(*frame_maximum_playout_delay_);
  }
}

std::vector<webrtc::RtpSource> VideoReceiveStream2::GetSources() const {
  return source_tracker_.GetSources();
}

VideoReceiveStream2::RecordingState
VideoReceiveStream2::SetAndGetRecordingState(RecordingState state,
                                             bool generate_key_frame) {
  RTC_DCHECK_RUN_ON(&worker_sequence_checker_);
  rtc::Event event;

  // Save old state, set the new state.
  RecordingState old_state;

  decode_queue_.PostTask(
      [this, &event, &old_state, callback = std::move(state.callback),
       generate_key_frame,
       last_keyframe_request =
           Timestamp::Millis(state.last_keyframe_request_ms.value_or(0))] {
        RTC_DCHECK_RUN_ON(&decode_queue_);
        old_state.callback = std::move(encoded_frame_buffer_function_);
        encoded_frame_buffer_function_ = std::move(callback);

        old_state.last_keyframe_request_ms =
            last_keyframe_request_.value_or(Timestamp::Zero()).ms();
        last_keyframe_request_ =
            generate_key_frame ? clock_->CurrentTime() : last_keyframe_request;

        event.Set();
      });

  if (generate_key_frame) {
    rtp_video_stream_receiver_.RequestKeyFrame();
    {
      // TODO(bugs.webrtc.org/11993): Post this to the network thread.
      RTC_DCHECK_RUN_ON(&packet_sequence_checker_);
      keyframe_generation_requested_ = true;
    }
  }

  event.Wait(rtc::Event::kForever);
  return old_state;
}

void VideoReceiveStream2::GenerateKeyFrame() {
  RTC_DCHECK_RUN_ON(&packet_sequence_checker_);
  RequestKeyFrame(clock_->CurrentTime());
  keyframe_generation_requested_ = true;
}

}  // namespace internal
}  // namespace webrtc
