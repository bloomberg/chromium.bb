// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/media/crypto/ppapi/clear_key_cdm.h"

#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/time.h"
#include "media/base/buffers.h"
#include "media/base/decoder_buffer.h"

#if defined(CLEAR_KEY_CDM_USE_FFMPEG_DECODER)
#include "base/at_exit.h"
#include "base/file_path.h"
#include "base/path_service.h"
#include "media/base/media.h"
#include "webkit/media/crypto/ppapi/ffmpeg_cdm_audio_decoder.h"
#include "webkit/media/crypto/ppapi/ffmpeg_cdm_video_decoder.h"

// Include FFmpeg avformat.h for av_register_all().
extern "C" {
// Temporarily disable possible loss of data warning.
MSVC_PUSH_DISABLE_WARNING(4244);
#include <libavformat/avformat.h>
MSVC_POP_WARNING();
}  // extern "C"

// TODO(tomfinegan): When COMPONENT_BUILD is not defined an AtExitManager must
// exist before the call to InitializeFFmpegLibraries(). This should no longer
// be required after http://crbug.com/91970 because we'll be able to get rid of
// InitializeFFmpegLibraries().
#if !defined COMPONENT_BUILD
static base::AtExitManager g_at_exit_manager;
#endif

// TODO(tomfinegan): InitializeFFmpegLibraries() and |g_cdm_module_initialized|
// are required for running in the sandbox, and should no longer be required
// after http://crbug.com/91970 is fixed.
static bool InitializeFFmpegLibraries() {
  FilePath file_path;
  CHECK(PathService::Get(base::DIR_EXE, &file_path));
  CHECK(media::InitializeMediaLibrary(file_path));

  // TODO(tomfinegan): Add and implement cdm::InitializeCdmModule(),
  // and call |av_register_all()| from there.
  av_register_all();

  return true;
}

static bool g_cdm_module_initialized = InitializeFFmpegLibraries();
#endif  // CLEAR_KEY_CDM_USE_FFMPEG_DECODER

static const char kClearKeyCdmVersion[] = "0.1.0.0";

// Copies |input_buffer| into a media::DecoderBuffer. If the |input_buffer| is
// empty, an empty (end-of-stream) media::DecoderBuffer is returned.
static scoped_refptr<media::DecoderBuffer> CopyDecoderBufferFrom(
    const cdm::InputBuffer& input_buffer) {
  if (!input_buffer.data) {
    DCHECK_EQ(input_buffer.data_size, 0);
    return media::DecoderBuffer::CreateEOSBuffer();
  }

  // TODO(tomfinegan): Get rid of this copy.
  scoped_refptr<media::DecoderBuffer> output_buffer =
      media::DecoderBuffer::CopyFrom(input_buffer.data, input_buffer.data_size);

  std::vector<media::SubsampleEntry> subsamples;
  for (int32_t i = 0; i < input_buffer.num_subsamples; ++i) {
    media::SubsampleEntry subsample;
    subsample.clear_bytes = input_buffer.subsamples[i].clear_bytes;
    subsample.cypher_bytes = input_buffer.subsamples[i].cipher_bytes;
    subsamples.push_back(subsample);
  }

  scoped_ptr<media::DecryptConfig> decrypt_config(new media::DecryptConfig(
      std::string(reinterpret_cast<const char*>(input_buffer.key_id),
                  input_buffer.key_id_size),
      std::string(reinterpret_cast<const char*>(input_buffer.iv),
                  input_buffer.iv_size),
      input_buffer.data_offset,
      subsamples));

  output_buffer->SetDecryptConfig(decrypt_config.Pass());
  output_buffer->SetTimestamp(
      base::TimeDelta::FromMicroseconds(input_buffer.timestamp));

  return output_buffer;
}

template<typename Type>
class ScopedResetter {
 public:
  explicit ScopedResetter(Type* object) : object_(object) {}
  ~ScopedResetter() { object_->Reset(); }

 private:
  Type* const object_;
};

template<typename Type>
static Type* AllocateAndCopy(const Type* data, int size) {
  COMPILE_ASSERT(sizeof(Type) == 1, type_size_is_not_one);
  Type* copy = new Type[size];
  memcpy(copy, data, size);
  return copy;
}

cdm::ContentDecryptionModule* CreateCdmInstance(
    cdm::Allocator* allocator, cdm::CdmHost* host) {
  DVLOG(1) << "CreateCdmInstance()";
  return new webkit_media::ClearKeyCdm(allocator, host);
}

void DestroyCdmInstance(cdm::ContentDecryptionModule* instance) {
  DVLOG(1) << "DestroyCdmInstance()";
  delete instance;
}

const char* GetCdmVersion() {
  return kClearKeyCdmVersion;
}

namespace webkit_media {

ClearKeyCdm::Client::Client() : status_(kKeyError), key_message_length_(0) {}

ClearKeyCdm::Client::~Client() {}

void ClearKeyCdm::Client::Reset() {
  status_ = kKeyError;
  session_id_.clear();
  key_message_.reset();
  key_message_length_ = 0;
  default_url_.clear();
}

void ClearKeyCdm::Client::KeyAdded(const std::string& key_system,
                                   const std::string& session_id) {
  status_ = kKeyAdded;
  session_id_ = session_id;
}

void ClearKeyCdm::Client::KeyError(const std::string& key_system,
                                   const std::string& session_id,
                                   media::Decryptor::KeyError error_code,
                                   int system_code) {
  status_ = kKeyError;
  session_id_ = session_id;
}

void ClearKeyCdm::Client::KeyMessage(const std::string& key_system,
                                     const std::string& session_id,
                                     scoped_array<uint8> message,
                                     int message_length,
                                     const std::string& default_url) {
  status_ = kKeyMessage;
  session_id_ = session_id;
  key_message_ = message.Pass();
  key_message_length_ = message_length;
}

void ClearKeyCdm::Client::NeedKey(const std::string& key_system,
                                  const std::string& session_id,
                                  scoped_array<uint8> init_data,
                                  int init_data_length) {
  // In the current implementation of AesDecryptor, NeedKey is not used.
  // If no key is available to decrypt an input buffer, it returns kNoKey to
  // the caller instead of firing NeedKey.
  NOTREACHED();
}

ClearKeyCdm::ClearKeyCdm(cdm::Allocator* allocator, cdm::CdmHost*)
    : decryptor_(&client_),
      allocator_(allocator) {
  DCHECK(allocator_);
#if defined(CLEAR_KEY_CDM_USE_FAKE_AUDIO_DECODER)
  channel_count_ = 0;
  bits_per_channel_ = 0;
  samples_per_second_ = 0;
  last_timestamp_ = media::kNoTimestamp();
  last_duration_ = media::kInfiniteDuration();
#endif  // CLEAR_KEY_CDM_USE_FAKE_AUDIO_DECODER
}

ClearKeyCdm::~ClearKeyCdm() {}

cdm::Status ClearKeyCdm::GenerateKeyRequest(const uint8_t* init_data,
                                            int init_data_size,
                                            cdm::KeyMessage* key_request) {
  DVLOG(1) << "GenerateKeyRequest()";
  base::AutoLock auto_lock(client_lock_);
  ScopedResetter<Client> auto_resetter(&client_);
  decryptor_.GenerateKeyRequest("", init_data, init_data_size);

  if (client_.status() != Client::kKeyMessage)
    return cdm::kSessionError;

  DCHECK(key_request);
  key_request->set_session_id(client_.session_id().data(),
                              client_.session_id().size());

  // TODO(tomfinegan): Get rid of this copy.
  key_request->set_message(allocator_->Allocate(client_.key_message_length()));

  DCHECK(key_request->message());
  DCHECK_EQ(key_request->message()->size(), client_.key_message_length());
  memcpy(reinterpret_cast<void*>(key_request->message()->data()),
         reinterpret_cast<const void*>(client_.key_message()),
         client_.key_message_length());

  key_request->set_default_url(client_.default_url().data(),
                               client_.default_url().size());
  return cdm::kSuccess;
}

cdm::Status ClearKeyCdm::AddKey(const char* session_id,
                                int session_id_size,
                                const uint8_t* key,
                                int key_size,
                                const uint8_t* key_id,
                                int key_id_size) {
  DVLOG(1) << "AddKey()";
  base::AutoLock auto_lock(client_lock_);
  ScopedResetter<Client> auto_resetter(&client_);
  decryptor_.AddKey("", key, key_size, key_id, key_id_size,
                    std::string(session_id, session_id_size));

  if (client_.status() != Client::kKeyAdded)
    return cdm::kSessionError;

  return cdm::kSuccess;
}

cdm::Status ClearKeyCdm::CancelKeyRequest(const char* session_id,
                                          int session_id_size) {
  DVLOG(1) << "CancelKeyRequest()";
  base::AutoLock auto_lock(client_lock_);
  ScopedResetter<Client> auto_resetter(&client_);
  decryptor_.CancelKeyRequest("", std::string(session_id, session_id_size));
  return cdm::kSuccess;
}

void ClearKeyCdm::TimerExpired(cdm::KeyMessage* msg, bool* populated) {
  // TODO(xhwang): do something with this?
  NOTREACHED() << "Wouldn't it be nice if CdmHost::SetTimer() was used?";
}

static void CopyDecryptResults(
    media::Decryptor::Status* status_copy,
    scoped_refptr<media::DecoderBuffer>* buffer_copy,
    media::Decryptor::Status status,
    const scoped_refptr<media::DecoderBuffer>& buffer) {
  *status_copy = status;
  *buffer_copy = buffer;
}

cdm::Status ClearKeyCdm::Decrypt(
    const cdm::InputBuffer& encrypted_buffer,
    cdm::DecryptedBlock* decrypted_block) {
  DVLOG(1) << "Decrypt()";
  DCHECK(encrypted_buffer.data);

  scoped_refptr<media::DecoderBuffer> buffer;
  cdm::Status status = DecryptToMediaDecoderBuffer(encrypted_buffer, &buffer);

  if (status != cdm::kSuccess)
    return status;

  DCHECK(buffer->GetData());
  decrypted_block->set_buffer(allocator_->Allocate(buffer->GetDataSize()));
  memcpy(reinterpret_cast<void*>(decrypted_block->buffer()->data()),
         buffer->GetData(),
         buffer->GetDataSize());
  decrypted_block->set_timestamp(buffer->GetTimestamp().InMicroseconds());

  return cdm::kSuccess;
}

cdm::Status ClearKeyCdm::InitializeAudioDecoder(
    const cdm::AudioDecoderConfig& audio_decoder_config) {
#if defined(CLEAR_KEY_CDM_USE_FFMPEG_DECODER)
  if (!audio_decoder_)
    audio_decoder_.reset(new webkit_media::FFmpegCdmAudioDecoder(allocator_));

  if (!audio_decoder_->Initialize(audio_decoder_config))
    return cdm::kSessionError;

  return cdm::kSuccess;
#elif defined(CLEAR_KEY_CDM_USE_FAKE_AUDIO_DECODER)
  channel_count_ = audio_decoder_config.channel_count;
  bits_per_channel_ = audio_decoder_config.bits_per_channel;
  samples_per_second_ = audio_decoder_config.samples_per_second;
  return cdm::kSuccess;
#else
  NOTIMPLEMENTED();
  return cdm::kSessionError;
#endif  // CLEAR_KEY_CDM_USE_FFMPEG_DECODER
}

cdm::Status ClearKeyCdm::InitializeVideoDecoder(
    const cdm::VideoDecoderConfig& video_decoder_config) {
#if defined(CLEAR_KEY_CDM_USE_FFMPEG_DECODER)
  if (!video_decoder_)
    video_decoder_.reset(new webkit_media::FFmpegCdmVideoDecoder(allocator_));

  if (!video_decoder_->Initialize(video_decoder_config))
    return cdm::kSessionError;

  return cdm::kSuccess;
#elif defined(CLEAR_KEY_CDM_USE_FAKE_VIDEO_DECODER)
  video_size_ = video_decoder_config.coded_size;
  return cdm::kSuccess;
#else
  NOTIMPLEMENTED();
  return cdm::kSessionError;
#endif  // CLEAR_KEY_CDM_USE_FFMPEG_DECODER
}

void ClearKeyCdm::ResetDecoder(cdm::StreamType decoder_type) {
  DVLOG(1) << "ResetDecoder()";
#if defined(CLEAR_KEY_CDM_USE_FFMPEG_DECODER)
  switch (decoder_type) {
    case cdm::kStreamTypeVideo:
      video_decoder_->Reset();
      break;
    case cdm::kStreamTypeAudio:
      audio_decoder_->Reset();
      break;
    default:
      NOTREACHED() << "ResetDecoder(): invalid cdm::StreamType";
  }
#elif defined(CLEAR_KEY_CDM_USE_FAKE_AUDIO_DECODER)
  if (decoder_type == cdm::kStreamTypeAudio) {
    last_timestamp_ = media::kNoTimestamp();
    last_duration_ = media::kInfiniteDuration();
  }
#endif  // CLEAR_KEY_CDM_USE_FFMPEG_DECODER
}

void ClearKeyCdm::DeinitializeDecoder(cdm::StreamType decoder_type) {
  DVLOG(1) << "DeinitializeDecoder()";
#if defined(CLEAR_KEY_CDM_USE_FFMPEG_DECODER)
  switch (decoder_type) {
    case cdm::kStreamTypeVideo:
      video_decoder_->Deinitialize();
      break;
    case cdm::kStreamTypeAudio:
      audio_decoder_->Deinitialize();
      break;
    default:
      NOTREACHED() << "DeinitializeDecoder(): invalid cdm::StreamType";
  }
#elif defined(CLEAR_KEY_CDM_USE_FAKE_AUDIO_DECODER)
  if (decoder_type == cdm::kStreamTypeAudio) {
    last_timestamp_ = media::kNoTimestamp();
    last_duration_ = media::kInfiniteDuration();
  }
#endif  // CLEAR_KEY_CDM_USE_FFMPEG_DECODER
}

cdm::Status ClearKeyCdm::DecryptAndDecodeFrame(
    const cdm::InputBuffer& encrypted_buffer,
    cdm::VideoFrame* decoded_frame) {
  DVLOG(1) << "DecryptAndDecodeFrame()";

  scoped_refptr<media::DecoderBuffer> buffer;
  cdm::Status status = DecryptToMediaDecoderBuffer(encrypted_buffer, &buffer);

  if (status != cdm::kSuccess)
    return status;

#if defined(CLEAR_KEY_CDM_USE_FFMPEG_DECODER)
  DCHECK(status == cdm::kSuccess);
  DCHECK(buffer);
  return video_decoder_->DecodeFrame(buffer.get()->GetData(),
                                     buffer->GetDataSize(),
                                     encrypted_buffer.timestamp,
                                     decoded_frame);
#elif defined(CLEAR_KEY_CDM_USE_FAKE_VIDEO_DECODER)
  // The fake decoder does not buffer any frames internally. So if the input is
  // empty (EOS), just return kNeedMoreData.
  if (buffer->IsEndOfStream())
    return cdm::kNeedMoreData;

  GenerateFakeVideoFrame(buffer->GetTimestamp(), decoded_frame);
  return cdm::kSuccess;
#else
  NOTIMPLEMENTED();
  return cdm::kDecodeError;
#endif  // CLEAR_KEY_CDM_USE_FFMPEG_DECODER
}

cdm::Status ClearKeyCdm::DecryptAndDecodeSamples(
    const cdm::InputBuffer& encrypted_buffer,
    cdm::AudioFrames* audio_frames) {
  DVLOG(1) << "DecryptAndDecodeSamples()";

  scoped_refptr<media::DecoderBuffer> buffer;
  cdm::Status status = DecryptToMediaDecoderBuffer(encrypted_buffer, &buffer);

  if (status != cdm::kSuccess)
    return status;

#if defined(CLEAR_KEY_CDM_USE_FFMPEG_DECODER)
  DCHECK(status == cdm::kSuccess);
  DCHECK(buffer);
  return audio_decoder_->DecodeBuffer(buffer.get()->GetData(),
                                      buffer->GetDataSize(),
                                      encrypted_buffer.timestamp,
                                      audio_frames);
#elif defined(CLEAR_KEY_CDM_USE_FAKE_AUDIO_DECODER)
  if (buffer->IsEndOfStream()) {
    // Upon the first EOS frame, return a frame with |last_duration_|.
    if (last_duration_ != media::kInfiniteDuration()) {
      DCHECK(last_timestamp_ != media::kNoTimestamp());
      GenerateFakeAudioFrames(audio_frames);
      last_timestamp_ = media::kNoTimestamp();
      last_duration_ = media::kInfiniteDuration();
      return cdm::kSuccess;
    }

    last_timestamp_ = media::kNoTimestamp();
    return cdm::kNeedMoreData;
  }

  base::TimeDelta cur_timestamp = buffer->GetTimestamp();
  DCHECK(cur_timestamp != media::kNoTimestamp());

  // Return kNeedMoreData for the first frame because duration is unknown.
  if (last_timestamp_ == media::kNoTimestamp()) {
    last_timestamp_ = cur_timestamp;
    return cdm::kNeedMoreData;
  }

  DCHECK(cur_timestamp > last_timestamp_);
  last_duration_ = cur_timestamp - last_timestamp_;
  last_timestamp_ = cur_timestamp;

  GenerateFakeAudioFrames(audio_frames);
  return cdm::kSuccess;
#endif  // CLEAR_KEY_CDM_USE_FAKE_AUDIO_DECODER
}

cdm::Status ClearKeyCdm::DecryptToMediaDecoderBuffer(
    const cdm::InputBuffer& encrypted_buffer,
    scoped_refptr<media::DecoderBuffer>* decrypted_buffer) {
  DCHECK(decrypted_buffer);
  scoped_refptr<media::DecoderBuffer> buffer =
      CopyDecoderBufferFrom(encrypted_buffer);

  if (buffer->IsEndOfStream()) {
    *decrypted_buffer = buffer;
    return cdm::kSuccess;
  }

  // Callback is called synchronously, so we can use variables on the stack.
  media::Decryptor::Status status = media::Decryptor::kError;
  // The AesDecryptor does not care what the stream type is. Pass kVideo
  // for both audio and video decryption.
  decryptor_.Decrypt(
      media::Decryptor::kVideo,
      buffer,
      base::Bind(&CopyDecryptResults, &status, decrypted_buffer));

  if (status == media::Decryptor::kError)
    return cdm::kDecryptError;

  if (status == media::Decryptor::kNoKey)
    return cdm::kNoKey;

  DCHECK_EQ(status, media::Decryptor::kSuccess);
  return cdm::kSuccess;
}

#if defined(CLEAR_KEY_CDM_USE_FAKE_AUDIO_DECODER)
void ClearKeyCdm::GenerateFakeAudioFrames(cdm::AudioFrames* audio_frames) {
  // For now we only generate one audio frame for the whole duration.
  // TODO(xhwang): Generate multiple audio frames for testing purposes.
  DCHECK(last_duration_ != media::kInfiniteDuration());
  int64 duration_in_microseconds = last_duration_.InMicroseconds();
  DCHECK_GT(duration_in_microseconds, 0);

  int64 timestamp = last_timestamp_.InMicroseconds();
  int64 bytes_per_sample = channel_count_ * bits_per_channel_ / 8;
  int64 frame_size = bytes_per_sample * samples_per_second_ *
      duration_in_microseconds / base::Time::kMicrosecondsPerSecond;

  const int kHeaderSize = sizeof(timestamp) + sizeof(frame_size);
  audio_frames->set_buffer(allocator_->Allocate(kHeaderSize + frame_size));
  int64* data = reinterpret_cast<int64*>(audio_frames->buffer()->data());
  *(data++) = timestamp;
  *(data++) = frame_size;
  // You won't hear anything because we have all zeros here. But the video
  // should play just fine!
  memset(data, 0, frame_size);
}
#endif  // CLEAR_KEY_CDM_USE_FAKE_AUDIO_DECODER

#if defined(CLEAR_KEY_CDM_USE_FAKE_VIDEO_DECODER)
void ClearKeyCdm::GenerateFakeVideoFrame(base::TimeDelta timestamp,
                                         cdm::VideoFrame* video_frame) {
  // Choose non-zero alignment and padding on purpose for testing.
  const int kAlignment = 8;
  const int kPadding = 16;
  const int kPlanePadding = 128;

  int width = video_size_.width;
  int height = video_size_.height;
  DCHECK_EQ(width % 2, 0);
  DCHECK_EQ(height % 2, 0);

  int y_stride = (width + kAlignment - 1) / kAlignment * kAlignment + kPadding;
  int uv_stride =
      (width / 2 + kAlignment - 1) / kAlignment * kAlignment + kPadding;
  int y_rows = height;
  int uv_rows = height / 2;
  int y_offset = 0;
  int v_offset = y_stride * y_rows + kPlanePadding;
  int u_offset = v_offset + uv_stride * uv_rows + kPlanePadding;
  int frame_size = u_offset + uv_stride * uv_rows + kPlanePadding;

  video_frame->set_format(cdm::kYv12);
  video_frame->set_size(video_size_);
  video_frame->set_frame_buffer(allocator_->Allocate(frame_size));
  video_frame->set_plane_offset(cdm::VideoFrame::kYPlane, y_offset);
  video_frame->set_plane_offset(cdm::VideoFrame::kVPlane, v_offset);
  video_frame->set_plane_offset(cdm::VideoFrame::kUPlane, u_offset);
  video_frame->set_stride(cdm::VideoFrame::kYPlane, y_stride);
  video_frame->set_stride(cdm::VideoFrame::kVPlane, uv_stride);
  video_frame->set_stride(cdm::VideoFrame::kUPlane, uv_stride);
  video_frame->set_timestamp(timestamp.InMicroseconds());

  static unsigned char color = 0;
  color += 10;

  memset(reinterpret_cast<void*>(video_frame->frame_buffer()->data()),
         color, frame_size);
}
#endif  // CLEAR_KEY_CDM_USE_FAKE_VIDEO_DECODER

}  // namespace webkit_media
