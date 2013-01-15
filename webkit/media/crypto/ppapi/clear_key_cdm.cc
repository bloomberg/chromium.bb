// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/media/crypto/ppapi/clear_key_cdm.h"

#include <algorithm>
#include <sstream>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/time.h"
#include "media/base/decoder_buffer.h"
#include "webkit/media/crypto/ppapi/cdm_video_decoder.h"

#if defined(CLEAR_KEY_CDM_USE_FAKE_AUDIO_DECODER)
#include "base/basictypes.h"
static const int64 kNoTimestamp = kint64min;
#endif  // CLEAR_KEY_CDM_USE_FAKE_AUDIO_DECODER

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
  return true;
}

static bool g_ffmpeg_lib_initialized = InitializeFFmpegLibraries();
#endif  // CLEAR_KEY_CDM_USE_FFMPEG_DECODER

static const char kClearKeyCdmVersion[] = "0.1.0.1";
static const char kExternalClearKey[] = "org.chromium.externalclearkey";
static const int64 kSecondsPerMinute = 60;
static const int64 kMsPerSecond = 1000;
static const int64 kInitialTimerDelayMs = 200;
static const int64 kMaxTimerDelayMs = 1 * kSecondsPerMinute * kMsPerSecond;
// Heart beat message header. If a key message starts with |kHeartBeatHeader|,
// it's a heart beat message. Otherwise, it's a key request.
static const char kHeartBeatHeader[] = "HEARTBEAT";

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

void INITIALIZE_CDM_MODULE() {
#if defined(CLEAR_KEY_CDM_USE_FFMPEG_DECODER)
  DVLOG(2) << "FFmpeg libraries initialized: " << g_ffmpeg_lib_initialized;
  av_register_all();
#endif  // CLEAR_KEY_CDM_USE_FFMPEG_DECODER
}

void DeinitializeCdmModule() {
}

cdm::ContentDecryptionModule* CreateCdmInstance(const char* key_system_arg,
                                                int key_system_size,
                                                cdm::Allocator* allocator,
                                                cdm::Host* host) {
  DVLOG(1) << "CreateCdmInstance()";
  DCHECK_EQ(std::string(key_system_arg, key_system_size), kExternalClearKey);
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

ClearKeyCdm::Client::Client() : status_(kKeyError) {}

ClearKeyCdm::Client::~Client() {}

void ClearKeyCdm::Client::Reset() {
  status_ = kKeyError;
  session_id_.clear();
  key_message_.clear();
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
                                     const std::string& message,
                                     const std::string& default_url) {
  status_ = kKeyMessage;
  session_id_ = session_id;
  key_message_ = message;
  default_url_ = default_url;
}

void ClearKeyCdm::Client::NeedKey(const std::string& key_system,
                                  const std::string& session_id,
                                  const std::string& type,
                                  scoped_array<uint8> init_data,
                                  int init_data_length) {
  // In the current implementation of AesDecryptor, NeedKey is not used.
  // If no key is available to decrypt an input buffer, it returns kNoKey to
  // the caller instead of firing NeedKey.
  NOTREACHED();
}

ClearKeyCdm::ClearKeyCdm(cdm::Allocator* allocator, cdm::Host* host)
    : decryptor_(base::Bind(&Client::KeyAdded, base::Unretained(&client_)),
                 base::Bind(&Client::KeyError, base::Unretained(&client_)),
                 base::Bind(&Client::KeyMessage, base::Unretained(&client_)),
                 base::Bind(&Client::NeedKey, base::Unretained(&client_))),
      allocator_(allocator),
      host_(host),
      timer_delay_ms_(kInitialTimerDelayMs),
      timer_set_(false) {
  DCHECK(allocator_);
#if defined(CLEAR_KEY_CDM_USE_FAKE_AUDIO_DECODER)
  channel_count_ = 0;
  bits_per_channel_ = 0;
  samples_per_second_ = 0;
  output_timestamp_base_in_microseconds_ = kNoTimestamp;
  total_samples_generated_ = 0;
#endif  // CLEAR_KEY_CDM_USE_FAKE_AUDIO_DECODER
}

ClearKeyCdm::~ClearKeyCdm() {}

cdm::Status ClearKeyCdm::GenerateKeyRequest(const char* type, int type_size,
                                            const uint8_t* init_data,
                                            int init_data_size) {
  DVLOG(1) << "GenerateKeyRequest()";
  base::AutoLock auto_lock(client_lock_);
  ScopedResetter<Client> auto_resetter(&client_);
  decryptor_.GenerateKeyRequest(kExternalClearKey,
                                std::string(type, type_size),
                                init_data, init_data_size);

  if (client_.status() != Client::kKeyMessage) {
    host_->SendKeyError(NULL, 0, cdm::kUnknownError, 0);
    return cdm::kSessionError;
  }

  host_->SendKeyMessage(
      client_.session_id().data(), client_.session_id().size(),
      client_.key_message().data(), client_.key_message().size(),
      client_.default_url().data(), client_.default_url().size());

  // Only save the latest session ID for heartbeat messages.
  heartbeat_session_id_ = client_.session_id();

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

  if (!timer_set_) {
    ScheduleNextHeartBeat();
    timer_set_ = true;
  }

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

void ClearKeyCdm::TimerExpired(void* context) {
  std::string heartbeat_message;
  if (!next_heartbeat_message_.empty() &&
      context == &next_heartbeat_message_[0]) {
    heartbeat_message = next_heartbeat_message_;
  } else {
    heartbeat_message = "ERROR: Invalid timer context found!";
  }

  // This URL is only used for testing the code path for defaultURL.
  // There is no service at this URL, so applications should ignore it.
  const char url[] = "http://test.externalclearkey.chromium.org";

  host_->SendKeyMessage(
      heartbeat_session_id_.data(), heartbeat_session_id_.size(),
      heartbeat_message.data(), heartbeat_message.size(),
      url, arraysize(url) - 1);

  ScheduleNextHeartBeat();
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
  decrypted_block->SetDecryptedBuffer(
      allocator_->Allocate(buffer->GetDataSize()));
  memcpy(reinterpret_cast<void*>(decrypted_block->DecryptedBuffer()->Data()),
         buffer->GetData(),
         buffer->GetDataSize());
  decrypted_block->DecryptedBuffer()->SetSize(buffer->GetDataSize());
  decrypted_block->SetTimestamp(buffer->GetTimestamp().InMicroseconds());

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
  if (video_decoder_ && video_decoder_->is_initialized()) {
    DCHECK(!video_decoder_->is_initialized());
    return cdm::kSessionError;
  }

  // Any uninitialized decoder will be replaced.
  video_decoder_ = CreateVideoDecoder(allocator_, video_decoder_config);
  if (!video_decoder_)
    return cdm::kSessionError;

  return cdm::kSuccess;
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
    output_timestamp_base_in_microseconds_ = kNoTimestamp;
    total_samples_generated_ = 0;
  }
#endif  // CLEAR_KEY_CDM_USE_FFMPEG_DECODER
}

void ClearKeyCdm::DeinitializeDecoder(cdm::StreamType decoder_type) {
  DVLOG(1) << "DeinitializeDecoder()";
  switch (decoder_type) {
    case cdm::kStreamTypeVideo:
      video_decoder_->Deinitialize();
      break;
    case cdm::kStreamTypeAudio:
#if defined(CLEAR_KEY_CDM_USE_FFMPEG_DECODER)
      audio_decoder_->Deinitialize();
#elif defined(CLEAR_KEY_CDM_USE_FAKE_AUDIO_DECODER)
      output_timestamp_base_in_microseconds_ = kNoTimestamp;
      total_samples_generated_ = 0;
#endif
      break;
    default:
      NOTREACHED() << "DeinitializeDecoder(): invalid cdm::StreamType";
  }
}

cdm::Status ClearKeyCdm::DecryptAndDecodeFrame(
    const cdm::InputBuffer& encrypted_buffer,
    cdm::VideoFrame* decoded_frame) {
  DVLOG(1) << "DecryptAndDecodeFrame()";
  TRACE_EVENT0("eme", "ClearKeyCdm::DecryptAndDecodeFrame");

  scoped_refptr<media::DecoderBuffer> buffer;
  cdm::Status status = DecryptToMediaDecoderBuffer(encrypted_buffer, &buffer);

  if (status != cdm::kSuccess)
    return status;

  DCHECK(status == cdm::kSuccess);
  DCHECK(buffer);
  return video_decoder_->DecodeFrame(buffer.get()->GetData(),
                                     buffer->GetDataSize(),
                                     encrypted_buffer.timestamp,
                                     decoded_frame);
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
  int64 timestamp_in_microseconds = kNoTimestamp;
  if (!buffer->IsEndOfStream()) {
    timestamp_in_microseconds = buffer->GetTimestamp().InMicroseconds();
    DCHECK(timestamp_in_microseconds != kNoTimestamp);
  }
  return GenerateFakeAudioFrames(timestamp_in_microseconds, audio_frames);
#else
  return cdm::kSuccess;
#endif  // CLEAR_KEY_CDM_USE_FAKE_AUDIO_DECODER
}

void ClearKeyCdm::ScheduleNextHeartBeat() {
  // Prepare the next heartbeat message and set timer.
  std::ostringstream msg_stream;
  msg_stream << kHeartBeatHeader << " from ClearKey CDM set at time "
             << host_->GetCurrentWallTimeInSeconds() << ".";
  next_heartbeat_message_ = msg_stream.str();

  host_->SetTimer(timer_delay_ms_, &next_heartbeat_message_[0]);

  // Use a smaller timer delay at start-up to facilitate testing. Increase the
  // timer delay up to a limit to avoid message spam.
  if (timer_delay_ms_ < kMaxTimerDelayMs)
    timer_delay_ms_ = std::min(2 * timer_delay_ms_, kMaxTimerDelayMs);
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
int64 ClearKeyCdm::CurrentTimeStampInMicroseconds() const {
  return output_timestamp_base_in_microseconds_ +
         base::Time::kMicrosecondsPerSecond *
         total_samples_generated_  / samples_per_second_;
}

int ClearKeyCdm::GenerateFakeAudioFramesFromDuration(
    int64 duration_in_microseconds,
    cdm::AudioFrames* audio_frames) const {
  int64 samples_to_generate = static_cast<double>(samples_per_second_) *
      duration_in_microseconds / base::Time::kMicrosecondsPerSecond + 0.5;
  if (samples_to_generate <= 0)
    return 0;

  int64 bytes_per_sample = channel_count_ * bits_per_channel_ / 8;
  // |frame_size| must be a multiple of |bytes_per_sample|.
  int64 frame_size = bytes_per_sample * samples_to_generate;

  int64 timestamp = CurrentTimeStampInMicroseconds();

  const int kHeaderSize = sizeof(timestamp) + sizeof(frame_size);
  audio_frames->SetFrameBuffer(allocator_->Allocate(kHeaderSize + frame_size));
  uint8_t* data = audio_frames->FrameBuffer()->Data();

  memcpy(data, &timestamp, sizeof(timestamp));
  data += sizeof(timestamp);
  memcpy(data, &frame_size, sizeof(frame_size));
  data += sizeof(frame_size);
  // You won't hear anything because we have all zeros here. But the video
  // should play just fine!
  memset(data, 0, frame_size);

  audio_frames->FrameBuffer()->SetSize(kHeaderSize + frame_size);

  return samples_to_generate;
}

cdm::Status ClearKeyCdm::GenerateFakeAudioFrames(
    int64 timestamp_in_microseconds,
    cdm::AudioFrames* audio_frames) {
  if (timestamp_in_microseconds == kNoTimestamp)
    return cdm::kNeedMoreData;

  // Return kNeedMoreData for the first frame because duration is unknown.
  if (output_timestamp_base_in_microseconds_ == kNoTimestamp) {
    output_timestamp_base_in_microseconds_ = timestamp_in_microseconds;
    return cdm::kNeedMoreData;
  }

  int samples_generated = GenerateFakeAudioFramesFromDuration(
      timestamp_in_microseconds - CurrentTimeStampInMicroseconds(),
      audio_frames);
  total_samples_generated_ += samples_generated;

  return samples_generated == 0 ? cdm::kNeedMoreData : cdm::kSuccess;
}
#endif  // CLEAR_KEY_CDM_USE_FAKE_AUDIO_DECODER

}  // namespace webkit_media
