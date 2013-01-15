// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_MEDIA_CRYPTO_PPAPI_FFMPEG_CDM_AUDIO_DECODER_H_
#define WEBKIT_MEDIA_CRYPTO_PPAPI_FFMPEG_CDM_AUDIO_DECODER_H_

#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "base/compiler_specific.h"
#include "webkit/media/crypto/ppapi/cdm/content_decryption_module.h"

struct AVCodecContext;
struct AVFrame;

namespace media {
class AudioBus;
class AudioTimestampHelper;
}

namespace webkit_media {

// TODO(xhwang): This class is partially cloned from media::FFmpegAudioDecoder.
// When media::FFmpegAudioDecoder is updated, it's a pain to keep this class
// in sync with media::FFmpegAudioDecoder. We need a long term sustainable
// solution for this. See http://crbug.com/169203
class FFmpegCdmAudioDecoder {
 public:
  explicit FFmpegCdmAudioDecoder(cdm::Allocator* allocator);
  ~FFmpegCdmAudioDecoder();
  bool Initialize(const cdm::AudioDecoderConfig& config);
  void Deinitialize();
  void Reset();

  // Returns true when |config| is a valid audio decoder configuration.
  static bool IsValidConfig(const cdm::AudioDecoderConfig& config);

  // Decodes |compressed_buffer|. Returns |cdm::kSuccess| after storing
  // output in |decoded_frames| when output is available. Returns
  // |cdm::kNeedMoreData| when |compressed_frame| does not produce output.
  // Returns |cdm::kDecodeError| when decoding fails.
  cdm::Status DecodeBuffer(const uint8_t* compressed_buffer,
                           int32_t compressed_buffer_size,
                           int64_t timestamp,
                           cdm::AudioFrames* decoded_frames);

 private:
  void ResetTimestampState();
  void ReleaseFFmpegResources();

  base::TimeDelta GetNextOutputTimestamp() const;

  void SerializeInt64(int64_t value);

  bool is_initialized_;

  cdm::Allocator* const allocator_;

  // FFmpeg structures owned by this object.
  AVCodecContext* codec_context_;
  AVFrame* av_frame_;

  // Audio format.
  int bits_per_channel_;
  int samples_per_second_;

  // Used for computing output timestamps.
  scoped_ptr<media::AudioTimestampHelper> output_timestamp_helper_;
  int bytes_per_frame_;
  base::TimeDelta last_input_timestamp_;

  // We may need to convert the audio data coming out of FFmpeg from planar
  // float to integer.
  scoped_ptr<media::AudioBus> converter_bus_;

  // Number of output sample bytes to drop before generating output buffers.
  // This is required for handling negative timestamps when decoding Vorbis
  // audio, for example.
  int output_bytes_to_drop_;

  typedef std::vector<uint8_t> SerializedAudioFrames;
  SerializedAudioFrames serialized_audio_frames_;

  DISALLOW_COPY_AND_ASSIGN(FFmpegCdmAudioDecoder);
};

}  // namespace webkit_media

#endif  // WEBKIT_MEDIA_CRYPTO_PPAPI_FFMPEG_CDM_AUDIO_DECODER_H_
