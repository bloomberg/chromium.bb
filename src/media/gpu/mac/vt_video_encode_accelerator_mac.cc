// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/mac/vt_video_encode_accelerator_mac.h"

#include <memory>

#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/mac/mac_logging.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_task_runner_handle.h"
#include "media/base/bitrate.h"
#include "media/base/mac/video_frame_mac.h"
#include "media/base/media_log.h"
#include "media/base/media_util.h"

// This is a min version of macOS where we want to support SVC encoding via
// EnableLowLatencyRateControl flag. The flag is actually supported since 11.3,
// but there we see frame drops even with ample bitrate budget. Excessive frame
// drops were fixed in 12.0.1.
#define LOW_LATENCY_FLAG_AVAILABLE_VER 12.0.1

namespace media {

namespace {

// TODO(emircan): Check if we can find the actual system capabilities via
// creating VTCompressionSessions with varying requirements.
// See https://crbug.com/584784.
const size_t kBitsPerByte = 8;
const size_t kDefaultResolutionWidth = 640;
const size_t kDefaultResolutionHeight = 480;
const size_t kMaxFrameRateNumerator = 30;
const size_t kMaxFrameRateDenominator = 1;
const size_t kMaxResolutionWidth = 4096;
const size_t kMaxResolutionHeight = 2160;
const size_t kNumInputBuffers = 3;

const VideoCodecProfile kSupportedProfiles[] = {
    H264PROFILE_BASELINE, H264PROFILE_MAIN, H264PROFILE_HIGH};

static CFStringRef VideoCodecProfileToVTProfile(VideoCodecProfile profile) {
  switch (profile) {
    case H264PROFILE_BASELINE:
      return kVTProfileLevel_H264_Baseline_AutoLevel;
    case H264PROFILE_MAIN:
      return kVTProfileLevel_H264_Main_AutoLevel;
    case H264PROFILE_HIGH:
      return kVTProfileLevel_H264_High_AutoLevel;
    default:
      NOTREACHED();
  }
  return kVTProfileLevel_H264_Baseline_AutoLevel;
}

}  // namespace

struct VTVideoEncodeAccelerator::InProgressFrameEncode {
  InProgressFrameEncode() = delete;

  InProgressFrameEncode(base::TimeDelta rtp_timestamp, base::TimeTicks ref_time)
      : timestamp(rtp_timestamp), reference_time(ref_time) {}

  InProgressFrameEncode(const InProgressFrameEncode&) = delete;
  InProgressFrameEncode& operator=(const InProgressFrameEncode&) = delete;

  const base::TimeDelta timestamp;
  const base::TimeTicks reference_time;
};

struct VTVideoEncodeAccelerator::EncodeOutput {
  EncodeOutput() = delete;

  EncodeOutput(VTEncodeInfoFlags info_flags,
               CMSampleBufferRef sbuf,
               base::TimeDelta timestamp)
      : info(info_flags),
        sample_buffer(sbuf, base::scoped_policy::RETAIN),
        capture_timestamp(timestamp) {}

  EncodeOutput(const EncodeOutput&) = delete;
  EncodeOutput& operator=(const EncodeOutput&) = delete;

  const VTEncodeInfoFlags info;
  const base::ScopedCFTypeRef<CMSampleBufferRef> sample_buffer;
  const base::TimeDelta capture_timestamp;
};

struct VTVideoEncodeAccelerator::BitstreamBufferRef {
  BitstreamBufferRef() = delete;

  BitstreamBufferRef(int32_t id,
                     base::WritableSharedMemoryMapping mapping,
                     size_t size)
      : id(id), mapping(std::move(mapping)), size(size) {}

  BitstreamBufferRef(const BitstreamBufferRef&) = delete;
  BitstreamBufferRef& operator=(const BitstreamBufferRef&) = delete;

  const int32_t id;
  const base::WritableSharedMemoryMapping mapping;
  const size_t size;
};

// .5 is set as a minimum to prevent overcompensating for large temporary
// overshoots. We don't want to degrade video quality too badly.
// .95 is set to prevent oscillations. When a lower bitrate is set on the
// encoder than previously set, its output seems to have a brief period of
// drastically reduced bitrate, so we want to avoid that. In steady state
// conditions, 0.95 seems to give us better overall bitrate over long periods
// of time.
VTVideoEncodeAccelerator::VTVideoEncodeAccelerator()
    : h264_profile_(H264PROFILE_BASELINE),
      bitrate_adjuster_(.5, .95),
      client_task_runner_(base::SequencedTaskRunnerHandle::Get()),
      encoder_thread_task_runner_(
          base::ThreadPool::CreateSingleThreadTaskRunner({})),
      encoder_task_weak_factory_(this) {
  encoder_weak_ptr_ = encoder_task_weak_factory_.GetWeakPtr();
}

VTVideoEncodeAccelerator::~VTVideoEncodeAccelerator() {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);
  DCHECK(!encoder_task_weak_factory_.HasWeakPtrs());
}

VideoEncodeAccelerator::SupportedProfiles
VTVideoEncodeAccelerator::GetSupportedProfiles() {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);

  SupportedProfiles profiles;
  const bool rv = CreateCompressionSession(
      gfx::Size(kDefaultResolutionWidth, kDefaultResolutionHeight));
  DestroyCompressionSession();
  if (!rv) {
    VLOG(1)
        << "Hardware encode acceleration is not available on this platform.";
    return profiles;
  }

  SupportedProfile profile;
  profile.max_framerate_numerator = kMaxFrameRateNumerator;
  profile.max_framerate_denominator = kMaxFrameRateDenominator;
  profile.max_resolution = gfx::Size(kMaxResolutionWidth, kMaxResolutionHeight);
  if (__builtin_available(macOS LOW_LATENCY_FLAG_AVAILABLE_VER, *))
    profile.scalability_modes.push_back(SVCScalabilityMode::kL1T2);
  for (const auto& supported_profile : kSupportedProfiles) {
    profile.profile = supported_profile;
    profiles.push_back(profile);
  }
  return profiles;
}

bool VTVideoEncodeAccelerator::Initialize(const Config& config,
                                          Client* client,
                                          std::unique_ptr<MediaLog> media_log) {
  DVLOG(3) << __func__ << ": " << config.AsHumanReadableString();
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);
  DCHECK(client);

  // NullMediaLog silently and safely does nothing.
  if (!media_log)
    media_log = std::make_unique<media::NullMediaLog>();

  // Clients are expected to call Flush() before reinitializing the encoder.
  DCHECK_EQ(pending_encodes_, 0);

  if (config.input_format != PIXEL_FORMAT_I420 &&
      config.input_format != PIXEL_FORMAT_NV12) {
    MEDIA_LOG(ERROR, media_log.get())
        << "Input format not supported= "
        << VideoPixelFormatToString(config.input_format);
    return false;
  }
  if (std::find(std::begin(kSupportedProfiles), std::end(kSupportedProfiles),
                config.output_profile) == std::end(kSupportedProfiles)) {
    MEDIA_LOG(ERROR, media_log.get()) << "Output profile not supported= "
                                      << GetProfileName(config.output_profile);
    return false;
  }
  h264_profile_ = config.output_profile;

  client_ptr_factory_ = std::make_unique<base::WeakPtrFactory<Client>>(client);
  client_ = client_ptr_factory_->GetWeakPtr();
  input_visible_size_ = config.input_visible_size;
  if (config.initial_framerate.has_value())
    frame_rate_ = config.initial_framerate.value();
  else
    frame_rate_ = kMaxFrameRateNumerator / kMaxFrameRateDenominator;
  bitrate_ = config.bitrate;
  bitstream_buffer_size_ = config.input_visible_size.GetArea();
  require_low_delay_ = config.require_low_delay;

  if (config.HasTemporalLayer())
    num_temporal_layers_ = config.spatial_layers.front().num_of_temporal_layers;

  if (num_temporal_layers_ > 2) {
    MEDIA_LOG(ERROR, media_log.get())
        << "Unsupported number of SVC temporal layers.";
    return false;
  }

  if (!ResetCompressionSession()) {
    MEDIA_LOG(ERROR, media_log.get()) << "Failed creating compression session.";
    return false;
  }

  client_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&Client::RequireBitstreamBuffers, client_,
                                kNumInputBuffers, input_visible_size_,
                                bitstream_buffer_size_));
  return true;
}

void VTVideoEncodeAccelerator::Encode(scoped_refptr<VideoFrame> frame,
                                      bool force_keyframe) {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);

  encoder_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VTVideoEncodeAccelerator::EncodeTask, encoder_weak_ptr_,
                     std::move(frame), force_keyframe));
}

void VTVideoEncodeAccelerator::UseOutputBitstreamBuffer(
    BitstreamBuffer buffer) {
  DVLOG(3) << __func__ << ": buffer size=" << buffer.size();
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);

  if (buffer.size() < bitstream_buffer_size_) {
    DLOG(ERROR) << "Output BitstreamBuffer isn't big enough: " << buffer.size()
                << " vs. " << bitstream_buffer_size_;
    client_->NotifyError(kInvalidArgumentError);
    return;
  }

  auto mapping =
      base::UnsafeSharedMemoryRegion::Deserialize(buffer.TakeRegion()).Map();
  if (!mapping.IsValid()) {
    DLOG(ERROR) << "Failed mapping shared memory.";
    client_->NotifyError(kPlatformFailureError);
    return;
  }

  std::unique_ptr<BitstreamBufferRef> buffer_ref(
      new BitstreamBufferRef(buffer.id(), std::move(mapping), buffer.size()));

  encoder_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VTVideoEncodeAccelerator::UseOutputBitstreamBufferTask,
                     encoder_weak_ptr_, std::move(buffer_ref)));
}

void VTVideoEncodeAccelerator::RequestEncodingParametersChange(
    const Bitrate& bitrate,
    uint32_t framerate) {
  DVLOG(3) << __func__ << ": bitrate=" << bitrate.ToString()
           << ": framerate=" << framerate;
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);

  encoder_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &VTVideoEncodeAccelerator::RequestEncodingParametersChangeTask,
          encoder_weak_ptr_, bitrate, framerate));
}

void VTVideoEncodeAccelerator::Destroy() {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);

  // Cancel all callbacks.
  client_ptr_factory_.reset();

  // VT resources need to be cleaned up on |encoder_thread_task_runner_|,
  // but the object itself is supposed to be deleted on this runner, so when
  // DestroyTask() is done we schedule deletion of |this|
  auto delete_self = [](VTVideoEncodeAccelerator* self) { delete self; };
  encoder_thread_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&VTVideoEncodeAccelerator::DestroyTask, encoder_weak_ptr_),
      base::BindOnce(delete_self, base::Unretained(this)));
}

void VTVideoEncodeAccelerator::Flush(FlushCallback flush_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);

  encoder_thread_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VTVideoEncodeAccelerator::FlushTask,
                                encoder_weak_ptr_, std::move(flush_callback)));
}

bool VTVideoEncodeAccelerator::IsFlushSupported() {
  return true;
}

void VTVideoEncodeAccelerator::EncodeTask(scoped_refptr<VideoFrame> frame,
                                          bool force_keyframe) {
  DCHECK(encoder_thread_task_runner_->BelongsToCurrentThread());
  DCHECK(compression_session_);
  DCHECK(frame);

  // TODO(emircan): See if we can eliminate a copy here by using
  // CVPixelBufferPool for the allocation of incoming VideoFrames.
  base::ScopedCFTypeRef<CVPixelBufferRef> pixel_buffer =
      WrapVideoFrameInCVPixelBuffer(*frame);
  base::ScopedCFTypeRef<CFDictionaryRef> frame_props =
      video_toolbox::DictionaryWithKeyValue(
          kVTEncodeFrameOptionKey_ForceKeyFrame,
          force_keyframe ? kCFBooleanTrue : kCFBooleanFalse);

  base::TimeTicks ref_time =
      frame->metadata().reference_time.value_or(base::TimeTicks::Now());
  auto timestamp_cm =
      CMTimeMake(frame->timestamp().InMicroseconds(), USEC_PER_SEC);
  // Wrap information we'll need after the frame is encoded in a heap object.
  // We'll get the pointer back from the VideoToolbox completion callback.
  std::unique_ptr<InProgressFrameEncode> request(
      new InProgressFrameEncode(frame->timestamp(), ref_time));

  if (bitrate_.mode() == Bitrate::Mode::kConstant) {
    // In CBR mode, we adjust bitrate before every encode based on past history
    // of bitrate adherence.
    SetAdjustedConstantBitrate(bitrate_adjuster_.GetAdjustedBitrateBps());
  }

  // We can pass the ownership of |request| to the encode callback if
  // successful. Otherwise let it fall out of scope.
  OSStatus status = VTCompressionSessionEncodeFrame(
      compression_session_, pixel_buffer, timestamp_cm, kCMTimeInvalid,
      frame_props, reinterpret_cast<void*>(request.get()), nullptr);
  if (status != noErr) {
    DLOG(ERROR) << " VTCompressionSessionEncodeFrame failed: " << status;
    NotifyError(kPlatformFailureError);
  } else {
    ++pending_encodes_;
    CHECK(request.release());
  }
}

void VTVideoEncodeAccelerator::UseOutputBitstreamBufferTask(
    std::unique_ptr<BitstreamBufferRef> buffer_ref) {
  DCHECK(encoder_thread_task_runner_->BelongsToCurrentThread());

  // If there is already EncodeOutput waiting, copy its output first.
  if (!encoder_output_queue_.empty()) {
    std::unique_ptr<VTVideoEncodeAccelerator::EncodeOutput> encode_output =
        std::move(encoder_output_queue_.front());
    encoder_output_queue_.pop_front();
    ReturnBitstreamBuffer(std::move(encode_output), std::move(buffer_ref));
    return;
  }

  bitstream_buffer_queue_.push_back(std::move(buffer_ref));
}

void VTVideoEncodeAccelerator::RequestEncodingParametersChangeTask(
    const Bitrate& bitrate,
    uint32_t framerate) {
  DCHECK(encoder_thread_task_runner_->BelongsToCurrentThread());

  if (!compression_session_) {
    NotifyError(kPlatformFailureError);
    return;
  }

  frame_rate_ = framerate;
  video_toolbox::SessionPropertySetter session_property_setter(
      compression_session_);
  session_property_setter.Set(kVTCompressionPropertyKey_ExpectedFrameRate,
                              frame_rate_);

  switch (bitrate.mode()) {
    case Bitrate::Mode::kConstant:
      if (bitrate.target_bps() != static_cast<uint32_t>(target_bitrate_)) {
        target_bitrate_ = bitrate.target_bps();
        bitrate_adjuster_.SetTargetBitrateBps(target_bitrate_);
        SetAdjustedConstantBitrate(bitrate_adjuster_.GetAdjustedBitrateBps());
      }
      break;
    case Bitrate::Mode::kVariable:
      SetVariableBitrate(bitrate);
      break;
    default:
      NOTREACHED();
  }
  bitrate_ = bitrate;
}

void VTVideoEncodeAccelerator::SetAdjustedConstantBitrate(int32_t bitrate) {
  DCHECK(encoder_thread_task_runner_->BelongsToCurrentThread());

  if (bitrate == encoder_set_bitrate_)
    return;

  encoder_set_bitrate_ = bitrate;
  video_toolbox::SessionPropertySetter session_property_setter(
      compression_session_);
  [[maybe_unused]] bool rv = session_property_setter.Set(
      kVTCompressionPropertyKey_AverageBitRate, encoder_set_bitrate_);
  rv &= session_property_setter.Set(
      kVTCompressionPropertyKey_DataRateLimits,
      video_toolbox::ArrayWithIntegerAndFloat(
          encoder_set_bitrate_ / kBitsPerByte, 1.0f));
  DLOG_IF(ERROR, !rv)
      << "Couldn't change bitrate parameters of encode session.";
}

void VTVideoEncodeAccelerator::SetVariableBitrate(const Bitrate& bitrate) {
  DCHECK(encoder_thread_task_runner_->BelongsToCurrentThread());
  DCHECK(bitrate.mode() == Bitrate::Mode::kVariable);

  video_toolbox::SessionPropertySetter session_property_setter(
      compression_session_);
  [[maybe_unused]] bool rv =
      session_property_setter.Set(kVTCompressionPropertyKey_AverageBitRate,
                                  static_cast<int32_t>(bitrate.target_bps()));
  rv &=
      session_property_setter.Set(kVTCompressionPropertyKey_DataRateLimits,
                                  video_toolbox::ArrayWithIntegerAndFloat(
                                      bitrate.peak_bps() / kBitsPerByte, 1.0f));
  DLOG_IF(ERROR, !rv)
      << "Couldn't change bitrate parameters of encode session.";
}

void VTVideoEncodeAccelerator::DestroyTask() {
  DCHECK(encoder_thread_task_runner_->BelongsToCurrentThread());

  // Cancel all encoder thread callbacks.
  encoder_task_weak_factory_.InvalidateWeakPtrs();
  DestroyCompressionSession();
}

void VTVideoEncodeAccelerator::NotifyError(
    VideoEncodeAccelerator::Error error) {
  DCHECK(encoder_thread_task_runner_->BelongsToCurrentThread());
  client_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&Client::NotifyError, client_, error));
}

// static
void VTVideoEncodeAccelerator::CompressionCallback(void* encoder_opaque,
                                                   void* request_opaque,
                                                   OSStatus status,
                                                   VTEncodeInfoFlags info,
                                                   CMSampleBufferRef sbuf) {
  // This function may be called asynchronously, on a different thread from the
  // one that calls VTCompressionSessionEncodeFrame.
  DVLOG(3) << __func__;

  auto* encoder = reinterpret_cast<VTVideoEncodeAccelerator*>(encoder_opaque);
  DCHECK(encoder);

  // InProgressFrameEncode holds timestamp information of the encoded frame.
  std::unique_ptr<InProgressFrameEncode> frame_info(
      reinterpret_cast<InProgressFrameEncode*>(request_opaque));

  // EncodeOutput holds onto CMSampleBufferRef when posting task between
  // threads.
  std::unique_ptr<EncodeOutput> encode_output(
      new EncodeOutput(info, sbuf, frame_info->timestamp));

  // This method is NOT called on |encoder_thread_|, so we still need to
  // post a task back to it to do work.
  encoder->encoder_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VTVideoEncodeAccelerator::CompressionCallbackTask,
                     encoder->encoder_weak_ptr_, status,
                     std::move(encode_output)));
}

void VTVideoEncodeAccelerator::CompressionCallbackTask(
    OSStatus status,
    std::unique_ptr<EncodeOutput> encode_output) {
  DCHECK(encoder_thread_task_runner_->BelongsToCurrentThread());

  --pending_encodes_;
  DCHECK_GE(pending_encodes_, 0);

  if (status != noErr) {
    DLOG(ERROR) << " encode failed: " << status;
    NotifyError(kPlatformFailureError);
    return;
  }

  // If there isn't any BitstreamBuffer to copy into, add it to a queue for
  // later use.
  if (bitstream_buffer_queue_.empty()) {
    encoder_output_queue_.push_back(std::move(encode_output));
    return;
  }

  std::unique_ptr<VTVideoEncodeAccelerator::BitstreamBufferRef> buffer_ref =
      std::move(bitstream_buffer_queue_.front());
  bitstream_buffer_queue_.pop_front();
  ReturnBitstreamBuffer(std::move(encode_output), std::move(buffer_ref));
}

void VTVideoEncodeAccelerator::ReturnBitstreamBuffer(
    std::unique_ptr<EncodeOutput> encode_output,
    std::unique_ptr<VTVideoEncodeAccelerator::BitstreamBufferRef> buffer_ref) {
  DVLOG(3) << __func__;
  DCHECK(encoder_thread_task_runner_->BelongsToCurrentThread());

  if (encode_output->info & kVTEncodeInfo_FrameDropped) {
    DVLOG(2) << " frame dropped";
    client_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&Client::BitstreamBufferReady, client_, buffer_ref->id,
                       BitstreamBufferMetadata(
                           0, false, encode_output->capture_timestamp)));
    MaybeRunFlushCallback();
    return;
  }

  auto* sample_attachments = static_cast<CFDictionaryRef>(
      CFArrayGetValueAtIndex(CMSampleBufferGetSampleAttachmentsArray(
                                 encode_output->sample_buffer.get(), true),
                             0));
  const bool keyframe = !CFDictionaryContainsKey(
      sample_attachments, kCMSampleAttachmentKey_NotSync);
  bool belongs_to_base_layer = true;
  if (CFBooleanRef value_ptr = base::mac::GetValueFromDictionary<CFBooleanRef>(
          sample_attachments, kCMSampleAttachmentKey_IsDependedOnByOthers)) {
    belongs_to_base_layer = static_cast<bool>(CFBooleanGetValue(value_ptr));
  }

  size_t used_buffer_size = 0;
  const bool copy_rv = video_toolbox::CopySampleBufferToAnnexBBuffer(
      encode_output->sample_buffer.get(), keyframe, buffer_ref->size,
      static_cast<char*>(buffer_ref->mapping.memory()), &used_buffer_size);
  if (!copy_rv) {
    DLOG(ERROR) << "Cannot copy output from SampleBuffer to AnnexBBuffer.";
    used_buffer_size = 0;
  }

  if (bitrate_.mode() == Bitrate::Mode::kConstant) {
    // In CBR mode, we let bitrate adjuster know how much encoded data was
    // produced to better control bitrate adherence.
    bitrate_adjuster_.Update(used_buffer_size);
  }

  BitstreamBufferMetadata md(used_buffer_size, keyframe,
                             encode_output->capture_timestamp);
  md.h264.emplace().temporal_idx = belongs_to_base_layer ? 0 : 1;
  client_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&Client::BitstreamBufferReady, client_,
                                buffer_ref->id, std::move(md)));
  MaybeRunFlushCallback();
}

bool VTVideoEncodeAccelerator::ResetCompressionSession() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);

  DestroyCompressionSession();

  bool session_rv = CreateCompressionSession(input_visible_size_);
  if (!session_rv) {
    DestroyCompressionSession();
    return false;
  }

  const bool configure_rv = ConfigureCompressionSession();
  if (configure_rv)
    RequestEncodingParametersChange(bitrate_, frame_rate_);
  return configure_rv;
}

bool VTVideoEncodeAccelerator::CreateCompressionSession(
    const gfx::Size& input_size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);

  std::vector<CFTypeRef> encoder_keys{
      kVTVideoEncoderSpecification_RequireHardwareAcceleratedVideoEncoder};
  std::vector<CFTypeRef> encoder_values{kCFBooleanTrue};

  if (__builtin_available(macOS LOW_LATENCY_FLAG_AVAILABLE_VER, *)) {
    if (require_low_delay_) {
      encoder_keys.push_back(
          kVTVideoEncoderSpecification_EnableLowLatencyRateControl);
      encoder_values.push_back(kCFBooleanTrue);
    }
  }
  base::ScopedCFTypeRef<CFDictionaryRef> encoder_spec =
      video_toolbox::DictionaryWithKeysAndValues(
          encoder_keys.data(), encoder_values.data(), encoder_keys.size());

  // Create the compression session.
  // Note that the encoder object is given to the compression session as the
  // callback context using a raw pointer. The C API does not allow us to use a
  // smart pointer, nor is this encoder ref counted. However, this is still
  // safe, because we 1) we own the compression session and 2) we tear it down
  // safely. When destructing the encoder, the compression session is flushed
  // and invalidated. Internally, VideoToolbox will join all of its threads
  // before returning to the client. Therefore, when control returns to us, we
  // are guaranteed that the output callback will not execute again.
  OSStatus status = VTCompressionSessionCreate(
      kCFAllocatorDefault, input_size.width(), input_size.height(),
      kCMVideoCodecType_H264, encoder_spec,
      nullptr /* sourceImageBufferAttributes */,
      nullptr /* compressedDataAllocator */,
      &VTVideoEncodeAccelerator::CompressionCallback,
      reinterpret_cast<void*>(this), compression_session_.InitializeInto());
  if (status != noErr) {
    DLOG(ERROR) << " VTCompressionSessionCreate failed: " << status;
    return false;
  }
  DVLOG(3) << " VTCompressionSession created with input size="
           << input_size.ToString();
  return true;
}

bool VTVideoEncodeAccelerator::ConfigureCompressionSession() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);
  DCHECK(compression_session_);

  video_toolbox::SessionPropertySetter session_property_setter(
      compression_session_);
  bool rv = true;
  rv &=
      session_property_setter.Set(kVTCompressionPropertyKey_ProfileLevel,
                                  VideoCodecProfileToVTProfile(h264_profile_));
  rv &= session_property_setter.Set(kVTCompressionPropertyKey_RealTime,
                                    require_low_delay_);

  rv &= session_property_setter.Set(
      kVTCompressionPropertyKey_AllowFrameReordering, false);
  // Limit keyframe output to 4 minutes, see https://crbug.com/658429.
  rv &= session_property_setter.Set(
      kVTCompressionPropertyKey_MaxKeyFrameInterval, 7200);
  rv &= session_property_setter.Set(
      kVTCompressionPropertyKey_MaxKeyFrameIntervalDuration, 240);
  DLOG_IF(ERROR, !rv) << " Setting session property failed.";

  if (session_property_setter.IsSupported(
          kVTCompressionPropertyKey_MaxFrameDelayCount)) {
    rv &= session_property_setter.Set(
        kVTCompressionPropertyKey_MaxFrameDelayCount,
        static_cast<int>(kNumInputBuffers));
  } else {
    DLOG(WARNING) << "MaxFrameDelayCount is not supported";
  }

  if (num_temporal_layers_ == 2) {
    if (__builtin_available(macOS LOW_LATENCY_FLAG_AVAILABLE_VER, *)) {
      if (!session_property_setter.IsSupported(
              kVTCompressionPropertyKey_BaseLayerFrameRateFraction)) {
        DLOG(ERROR) << "BaseLayerFrameRateFraction is not supported";
        return false;
      }
      rv &= session_property_setter.Set(
          kVTCompressionPropertyKey_BaseLayerFrameRateFraction, 0.5);
      DLOG_IF(ERROR, !rv) << " Setting BaseLayerFrameRate property failed.";
    } else {
      DLOG(ERROR) << "SVC encoding is not supported on this OS version.";
      rv = false;
    }
  }

  return rv;
}

void VTVideoEncodeAccelerator::DestroyCompressionSession() {
  if (compression_session_) {
    VTCompressionSessionInvalidate(compression_session_);
    compression_session_.reset();
  }
}

void VTVideoEncodeAccelerator::FlushTask(FlushCallback flush_callback) {
  DVLOG(3) << __func__;
  DCHECK(encoder_thread_task_runner_->BelongsToCurrentThread());
  DCHECK(flush_callback);

  if (!compression_session_) {
    client_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(flush_callback), false));
    return;
  }

  // Even though this will block until all frames are returned, the frames will
  // be posted to the current task runner, so we can't run the flush callback
  // at this time.
  OSStatus status =
      VTCompressionSessionCompleteFrames(compression_session_, kCMTimeInvalid);

  if (status != noErr) {
    OSSTATUS_DLOG(ERROR, status)
        << " VTCompressionSessionCompleteFrames failed: " << status;
    client_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(flush_callback), /*success=*/false));
    return;
  }

  pending_flush_cb_ = std::move(flush_callback);
  MaybeRunFlushCallback();
}

void VTVideoEncodeAccelerator::MaybeRunFlushCallback() {
  DCHECK(encoder_thread_task_runner_->BelongsToCurrentThread());

  if (!pending_flush_cb_)
    return;

  if (pending_encodes_ || !encoder_output_queue_.empty())
    return;

  client_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(pending_flush_cb_), /*success=*/true));
}

}  // namespace media
