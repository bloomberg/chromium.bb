// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/media/crypto/ppapi/ffmpeg_cdm_audio_decoder.h"

#include <algorithm>

#include "base/logging.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/buffers.h"
#include "media/base/data_buffer.h"
#include "media/base/limits.h"
#include "webkit/media/crypto/ppapi/cdm/content_decryption_module.h"

// Include FFmpeg header files.
extern "C" {
// Temporarily disable possible loss of data warning.
MSVC_PUSH_DISABLE_WARNING(4244);
#include <libavcodec/avcodec.h>
MSVC_POP_WARNING();
}  // extern "C"

namespace webkit_media {

// Maximum number of channels with defined layout in src/media.
static const int kMaxChannels = 8;

static CodecID CdmAudioCodecToCodecID(
    cdm::AudioDecoderConfig::AudioCodec audio_codec) {
  switch (audio_codec) {
    case cdm::AudioDecoderConfig::kCodecVorbis:
      return CODEC_ID_VORBIS;
    case cdm::AudioDecoderConfig::kCodecAac:
      return CODEC_ID_AAC;
    case cdm::AudioDecoderConfig::kUnknownAudioCodec:
    default:
      NOTREACHED() << "Unsupported cdm::AudioCodec: " << audio_codec;
      return CODEC_ID_NONE;
  }
}

static void CdmAudioDecoderConfigToAVCodecContext(
    const cdm::AudioDecoderConfig& config,
    AVCodecContext* codec_context) {
  codec_context->codec_type = AVMEDIA_TYPE_AUDIO;
  codec_context->codec_id = CdmAudioCodecToCodecID(config.codec);

  switch (config.bits_per_channel) {
    case 8:
      codec_context->sample_fmt = AV_SAMPLE_FMT_U8;
      break;
    case 16:
      codec_context->sample_fmt = AV_SAMPLE_FMT_S16;
      break;
    case 32:
      codec_context->sample_fmt = AV_SAMPLE_FMT_S32;
      break;
    default:
      DVLOG(1) << "CdmAudioDecoderConfigToAVCodecContext() Unsupported bits "
                  "per channel: " << config.bits_per_channel;
      codec_context->sample_fmt = AV_SAMPLE_FMT_NONE;
  }

  codec_context->channels = config.channel_count;
  codec_context->sample_rate = config.samples_per_second;

  if (config.extra_data) {
    codec_context->extradata_size = config.extra_data_size;
    codec_context->extradata = reinterpret_cast<uint8_t*>(
        av_malloc(config.extra_data_size + FF_INPUT_BUFFER_PADDING_SIZE));
    memcpy(codec_context->extradata, config.extra_data,
           config.extra_data_size);
    memset(codec_context->extradata + config.extra_data_size, '\0',
           FF_INPUT_BUFFER_PADDING_SIZE);
  } else {
    codec_context->extradata = NULL;
    codec_context->extradata_size = 0;
  }
}

FFmpegCdmAudioDecoder::FFmpegCdmAudioDecoder(cdm::Allocator* allocator)
    : is_initialized_(false),
      allocator_(allocator),
      codec_context_(NULL),
      av_frame_(NULL),
      bits_per_channel_(0),
      samples_per_second_(0),
      bytes_per_frame_(0),
      last_input_timestamp_(media::kNoTimestamp()),
      output_bytes_to_drop_(0) {
}

FFmpegCdmAudioDecoder::~FFmpegCdmAudioDecoder() {
  ReleaseFFmpegResources();
}

bool FFmpegCdmAudioDecoder::Initialize(const cdm::AudioDecoderConfig& config) {
  DVLOG(1) << "Initialize()";

  if (!IsValidConfig(config)) {
    LOG(ERROR) << "Initialize(): invalid audio decoder configuration.";
    return false;
  }

  if (is_initialized_) {
    LOG(ERROR) << "Initialize(): Already initialized.";
    return false;
  }

  // Initialize AVCodecContext structure.
  codec_context_ = avcodec_alloc_context3(NULL);
  CdmAudioDecoderConfigToAVCodecContext(config, codec_context_);

  // MP3 decodes to S16P which we don't support, tell it to use S16 instead.
  if (codec_context_->sample_fmt == AV_SAMPLE_FMT_S16P)
    codec_context_->request_sample_fmt = AV_SAMPLE_FMT_S16;

  AVCodec* codec = avcodec_find_decoder(codec_context_->codec_id);
  if (!codec || avcodec_open2(codec_context_, codec, NULL) < 0) {
    DLOG(ERROR) << "Could not initialize audio decoder: "
                << codec_context_->codec_id;
    return false;
  }

  // Ensure avcodec_open2() respected our format request.
  if (codec_context_->sample_fmt == AV_SAMPLE_FMT_S16P) {
    DLOG(ERROR) << "Unable to configure a supported sample format: "
                << codec_context_->sample_fmt;
    return false;
  }

  // Some codecs will only output float data, so we need to convert to integer
  // before returning the decoded buffer.
  if (codec_context_->sample_fmt == AV_SAMPLE_FMT_FLTP ||
      codec_context_->sample_fmt == AV_SAMPLE_FMT_FLT) {
    // Preallocate the AudioBus for float conversions.  We can treat interleaved
    // float data as a single planar channel since our output is expected in an
    // interleaved format anyways.
    int channels = codec_context_->channels;
    if (codec_context_->sample_fmt == AV_SAMPLE_FMT_FLT)
      channels = 1;
    converter_bus_ = media::AudioBus::CreateWrapper(channels);
  }

  // Success!
  av_frame_ = avcodec_alloc_frame();
  bits_per_channel_ = config.bits_per_channel;
  samples_per_second_ = config.samples_per_second;
  bytes_per_frame_ = codec_context_->channels * bits_per_channel_ / 8;
  output_timestamp_helper_.reset(new media::AudioTimestampHelper(
      bytes_per_frame_, config.samples_per_second));
  serialized_audio_frames_.reserve(bytes_per_frame_ * samples_per_second_);
  is_initialized_ = true;

  return true;
}

void FFmpegCdmAudioDecoder::Deinitialize() {
  DVLOG(1) << "Deinitialize()";
  ReleaseFFmpegResources();
  is_initialized_ = false;
  ResetTimestampState();
}

void FFmpegCdmAudioDecoder::Reset() {
  DVLOG(1) << "Reset()";
  avcodec_flush_buffers(codec_context_);
  ResetTimestampState();
}

// static
bool FFmpegCdmAudioDecoder::IsValidConfig(
    const cdm::AudioDecoderConfig& config) {
  return config.codec != cdm::AudioDecoderConfig::kUnknownAudioCodec &&
         config.channel_count > 0 &&
         config.channel_count <= kMaxChannels &&
         config.bits_per_channel > 0 &&
         config.bits_per_channel <= media::limits::kMaxBitsPerSample &&
         config.samples_per_second > 0 &&
         config.samples_per_second <= media::limits::kMaxSampleRate;
}

cdm::Status FFmpegCdmAudioDecoder::DecodeBuffer(
    const uint8_t* compressed_buffer,
    int32_t compressed_buffer_size,
    int64_t input_timestamp,
    cdm::AudioFrames* decoded_frames) {
  DVLOG(1) << "DecodeBuffer()";
  const bool is_end_of_stream = compressed_buffer_size == 0;
  base::TimeDelta timestamp =
      base::TimeDelta::FromMicroseconds(input_timestamp);

  bool is_vorbis = codec_context_->codec_id == CODEC_ID_VORBIS;
  if (!is_end_of_stream) {
    if (last_input_timestamp_ == media::kNoTimestamp()) {
      if (is_vorbis && timestamp < base::TimeDelta()) {
        // Dropping frames for negative timestamps as outlined in section A.2
        // in the Vorbis spec. http://xiph.org/vorbis/doc/Vorbis_I_spec.html
        int frames_to_drop = floor(
            0.5 + -timestamp.InSecondsF() * samples_per_second_);
        output_bytes_to_drop_ = bytes_per_frame_ * frames_to_drop;
      } else {
        last_input_timestamp_ = timestamp;
      }
    } else if (timestamp != media::kNoTimestamp()) {
      if (timestamp < last_input_timestamp_) {
        base::TimeDelta diff = timestamp - last_input_timestamp_;
        DVLOG(1) << "Input timestamps are not monotonically increasing! "
                 << " ts " << timestamp.InMicroseconds() << " us"
                 << " diff " << diff.InMicroseconds() << " us";
        return cdm::kDecodeError;
      }

      last_input_timestamp_ = timestamp;
    }
  }

  AVPacket packet;
  av_init_packet(&packet);
  packet.data = const_cast<uint8_t*>(compressed_buffer);
  packet.size = compressed_buffer_size;

  // Each audio packet may contain several frames, so we must call the decoder
  // until we've exhausted the packet.  Regardless of the packet size we always
  // want to hand it to the decoder at least once, otherwise we would end up
  // skipping end of stream packets since they have a size of zero.
  do {
    // Reset frame to default values.
    avcodec_get_frame_defaults(av_frame_);

    int frame_decoded = 0;
    int result = avcodec_decode_audio4(
        codec_context_, av_frame_, &frame_decoded, &packet);

    if (result < 0) {
      DCHECK(!is_end_of_stream)
          << "End of stream buffer produced an error! "
          << "This is quite possibly a bug in the audio decoder not handling "
          << "end of stream AVPackets correctly.";

      DLOG(ERROR)
          << "Error decoding an audio frame with timestamp: "
          << timestamp.InMicroseconds() << " us, duration: "
          << timestamp.InMicroseconds() << " us, packet size: "
          << compressed_buffer_size << " bytes";

      return cdm::kDecodeError;
    }

    // Update packet size and data pointer in case we need to call the decoder
    // with the remaining bytes from this packet.
    packet.size -= result;
    packet.data += result;

    if (output_timestamp_helper_->base_timestamp() == media::kNoTimestamp() &&
        !is_end_of_stream) {
      DCHECK(timestamp != media::kNoTimestamp());
      if (output_bytes_to_drop_ > 0) {
        // Currently Vorbis is the only codec that causes us to drop samples.
        // If we have to drop samples it always means the timeline starts at 0.
        DCHECK_EQ(codec_context_->codec_id, CODEC_ID_VORBIS);
        output_timestamp_helper_->SetBaseTimestamp(base::TimeDelta());
      } else {
        output_timestamp_helper_->SetBaseTimestamp(timestamp);
      }
    }

    int decoded_audio_size = 0;
    if (frame_decoded) {
      int output_sample_rate = av_frame_->sample_rate;
      if (output_sample_rate != samples_per_second_) {
        DLOG(ERROR) << "Output sample rate (" << output_sample_rate
                    << ") doesn't match expected rate " << samples_per_second_;
        return cdm::kDecodeError;
      }

      decoded_audio_size = av_samples_get_buffer_size(
          NULL, codec_context_->channels, av_frame_->nb_samples,
          codec_context_->sample_fmt, 1);
      // If we're decoding into float, adjust audio size.
      if (converter_bus_ && bits_per_channel_ / 8 != sizeof(float)) {
        DCHECK(codec_context_->sample_fmt == AV_SAMPLE_FMT_FLT ||
               codec_context_->sample_fmt == AV_SAMPLE_FMT_FLTP);
        decoded_audio_size *=
            static_cast<float>(bits_per_channel_ / 8) / sizeof(float);
      }
    }

    int start_sample = 0;
    if (decoded_audio_size > 0 && output_bytes_to_drop_ > 0) {
      DCHECK_EQ(decoded_audio_size % bytes_per_frame_, 0)
          << "Decoder didn't output full frames";

      int dropped_size = std::min(decoded_audio_size, output_bytes_to_drop_);
      start_sample = dropped_size / bytes_per_frame_;
      decoded_audio_size -= dropped_size;
      output_bytes_to_drop_ -= dropped_size;
    }

    scoped_refptr<media::DataBuffer> output;
    if (decoded_audio_size > 0) {
      DCHECK_EQ(decoded_audio_size % bytes_per_frame_, 0)
          << "Decoder didn't output full frames";

      // Convert float data using an AudioBus.
      if (converter_bus_) {
        // Setup the AudioBus as a wrapper of the AVFrame data and then use
        // AudioBus::ToInterleaved() to convert the data as necessary.
        int skip_frames = start_sample;
        int total_frames = av_frame_->nb_samples - start_sample;
        if (codec_context_->sample_fmt == AV_SAMPLE_FMT_FLT) {
          DCHECK_EQ(converter_bus_->channels(), 1);
          total_frames *= codec_context_->channels;
          skip_frames *= codec_context_->channels;
        }
        converter_bus_->set_frames(total_frames);
        DCHECK_EQ(decoded_audio_size,
                  converter_bus_->frames() * bytes_per_frame_);

        for (int i = 0; i < converter_bus_->channels(); ++i) {
          converter_bus_->SetChannelData(i, reinterpret_cast<float*>(
              av_frame_->extended_data[i]) + skip_frames);
        }

        output = new media::DataBuffer(decoded_audio_size);
        output->SetDataSize(decoded_audio_size);
        converter_bus_->ToInterleaved(
            converter_bus_->frames(), bits_per_channel_ / 8,
            output->GetWritableData());
      } else {
        output = new media::DataBuffer(
            av_frame_->extended_data[0] + start_sample * bytes_per_frame_,
            decoded_audio_size);
      }

      base::TimeDelta output_timestamp =
          output_timestamp_helper_->GetTimestamp();
      output_timestamp_helper_->AddBytes(decoded_audio_size);

      // Serialize the audio samples into |serialized_audio_frames_|.
      SerializeInt64(output_timestamp.InMicroseconds());
      SerializeInt64(output->GetDataSize());
      serialized_audio_frames_.insert(
          serialized_audio_frames_.end(),
          output->GetData(),
          output->GetData() + output->GetDataSize());
    }
  } while (packet.size > 0);

  if (!serialized_audio_frames_.empty()) {
    decoded_frames->SetFrameBuffer(
        allocator_->Allocate(serialized_audio_frames_.size()));
    if (!decoded_frames->FrameBuffer()) {
      LOG(ERROR) << "DecodeBuffer() cdm::Allocator::Allocate failed.";
      return cdm::kDecodeError;
    }
    memcpy(decoded_frames->FrameBuffer()->Data(),
           &serialized_audio_frames_[0],
           serialized_audio_frames_.size());
    decoded_frames->FrameBuffer()->SetSize(serialized_audio_frames_.size());
    serialized_audio_frames_.clear();

    return cdm::kSuccess;
  }

  return cdm::kNeedMoreData;
}

void FFmpegCdmAudioDecoder::ResetTimestampState() {
  output_timestamp_helper_->SetBaseTimestamp(media::kNoTimestamp());
  last_input_timestamp_ = media::kNoTimestamp();
  output_bytes_to_drop_ = 0;
}

void FFmpegCdmAudioDecoder::ReleaseFFmpegResources() {
  DVLOG(1) << "ReleaseFFmpegResources()";

  if (codec_context_) {
    av_free(codec_context_->extradata);
    avcodec_close(codec_context_);
    av_free(codec_context_);
    codec_context_ = NULL;
  }
  if (av_frame_) {
    av_free(av_frame_);
    av_frame_ = NULL;
  }
}

void FFmpegCdmAudioDecoder::SerializeInt64(int64 value) {
  int previous_size = serialized_audio_frames_.size();
  serialized_audio_frames_.resize(previous_size + sizeof(value));
  memcpy(&serialized_audio_frames_[0] + previous_size, &value, sizeof(value));
}

}  // namespace webkit_media
