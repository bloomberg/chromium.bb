// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_MEDIA_CRYPTO_PPAPI_CLEAR_KEY_CDM_H_
#define WEBKIT_MEDIA_CRYPTO_PPAPI_CLEAR_KEY_CDM_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "media/crypto/aes_decryptor.h"
#include "webkit/media/crypto/ppapi/cdm/content_decryption_module.h"

// Enable this to use the fake decoder for testing.
// TODO(tomfinegan): Move fake audio decoder into a separate class.
#if 0
#define CLEAR_KEY_CDM_USE_FAKE_AUDIO_DECODER
#endif

namespace media {
class DecoderBuffer;
}

namespace webkit_media {

class CdmVideoDecoder;
class FFmpegCdmAudioDecoder;

// Clear key implementation of the cdm::ContentDecryptionModule interface.
class ClearKeyCdm : public cdm::ContentDecryptionModule {
 public:
  explicit ClearKeyCdm(cdm::Host* host);
  virtual ~ClearKeyCdm();

  // ContentDecryptionModule implementation.
  virtual cdm::Status GenerateKeyRequest(
      const char* type, int type_size,
      const uint8_t* init_data, int init_data_size) OVERRIDE;
  virtual cdm::Status AddKey(const char* session_id, int session_id_size,
                             const uint8_t* key, int key_size,
                             const uint8_t* key_id, int key_id_size) OVERRIDE;
  virtual cdm::Status CancelKeyRequest(const char* session_id,
                                       int session_id_size) OVERRIDE;
  virtual void TimerExpired(void* context) OVERRIDE;
  virtual cdm::Status Decrypt(const cdm::InputBuffer& encrypted_buffer,
                              cdm::DecryptedBlock* decrypted_block) OVERRIDE;
  virtual cdm::Status InitializeAudioDecoder(
      const cdm::AudioDecoderConfig& audio_decoder_config) OVERRIDE;
  virtual cdm::Status InitializeVideoDecoder(
      const cdm::VideoDecoderConfig& video_decoder_config) OVERRIDE;
  virtual void DeinitializeDecoder(cdm::StreamType decoder_type) OVERRIDE;
  virtual void ResetDecoder(cdm::StreamType decoder_type) OVERRIDE;
  virtual cdm::Status DecryptAndDecodeFrame(
      const cdm::InputBuffer& encrypted_buffer,
      cdm::VideoFrame* video_frame) OVERRIDE;
  virtual cdm::Status DecryptAndDecodeSamples(
      const cdm::InputBuffer& encrypted_buffer,
      cdm::AudioFrames* audio_frames) OVERRIDE;
  virtual void Destroy() OVERRIDE;

 private:
  // TODO(xhwang): After we removed DecryptorClient. We probably can also remove
  // this Client class as well. Investigate this possibility.
  class Client {
   public:
    enum Status {
      kKeyAdded,
      kKeyError,
      kKeyMessage,
      kNeedKey
    };

    Client();
    virtual ~Client();

    Status status() { return status_; }
    const std::string& session_id() { return session_id_; }
    const std::string& key_message() { return key_message_; }
    const std::string& default_url() { return default_url_; }

    // Resets the Client to a clean state.
    void Reset();

    void KeyAdded(const std::string& key_system, const std::string& session_id);
    void KeyError(const std::string& key_system,
                  const std::string& session_id,
                  media::Decryptor::KeyError error_code,
                  int system_code);
    void KeyMessage(const std::string& key_system,
                    const std::string& session_id,
                    const std::string& message,
                    const std::string& default_url);
    void NeedKey(const std::string& key_system,
                 const std::string& session_id,
                 const std::string& type,
                 scoped_ptr<uint8[]> init_data, int init_data_length);

   private:
    Status status_;
    std::string session_id_;
    std::string key_message_;
    std::string default_url_;
  };

  // Prepares next heartbeat message and sets a timer for it.
  void ScheduleNextHeartBeat();

  // Decrypts the |encrypted_buffer| and puts the result in |decrypted_buffer|.
  // Returns cdm::kSuccess if decryption succeeded. The decrypted result is
  // put in |decrypted_buffer|. If |encrypted_buffer| is empty, the
  // |decrypted_buffer| is set to an empty (EOS) buffer.
  // Returns cdm::kNoKey if no decryption key was available. In this case
  // |decrypted_buffer| should be ignored by the caller.
  // Returns cdm::kDecryptError if any decryption error occurred. In this case
  // |decrypted_buffer| should be ignored by the caller.
  cdm::Status DecryptToMediaDecoderBuffer(
      const cdm::InputBuffer& encrypted_buffer,
      scoped_refptr<media::DecoderBuffer>* decrypted_buffer);

#if defined(CLEAR_KEY_CDM_USE_FAKE_AUDIO_DECODER)
  int64 CurrentTimeStampInMicroseconds() const;

  // Generates fake video frames with |duration_in_microseconds|.
  // Returns the number of samples generated in the |audio_frames|.
  int GenerateFakeAudioFramesFromDuration(int64 duration_in_microseconds,
                                          cdm::AudioFrames* audio_frames) const;

  // Generates fake video frames given |input_timestamp|.
  // Returns cdm::kSuccess if any audio frame is successfully generated.
  cdm::Status GenerateFakeAudioFrames(int64 timestamp_in_microseconds,
                                      cdm::AudioFrames* audio_frames);
#endif  // CLEAR_KEY_CDM_USE_FAKE_AUDIO_DECODER

  Client client_;
  media::AesDecryptor decryptor_;

  // Protects the |client_| from being accessed by the |decryptor_|
  // simultaneously.
  base::Lock client_lock_;

  cdm::Host* host_;

  std::string heartbeat_session_id_;
  std::string next_heartbeat_message_;

  // Timer delay in milliseconds for the next host_->SetTimer() call.
  int64 timer_delay_ms_;

  // Indicates whether a timer has been set to prevent multiple timers from
  // running.
  bool timer_set_;

#if defined(CLEAR_KEY_CDM_USE_FAKE_AUDIO_DECODER)
  int channel_count_;
  int bits_per_channel_;
  int samples_per_second_;
  int64 output_timestamp_base_in_microseconds_;
  int total_samples_generated_;
#endif  // CLEAR_KEY_CDM_USE_FAKE_AUDIO_DECODER

#if defined(CLEAR_KEY_CDM_USE_FFMPEG_DECODER)
  scoped_ptr<FFmpegCdmAudioDecoder> audio_decoder_;
#endif  // CLEAR_KEY_CDM_USE_FFMPEG_DECODER

  scoped_ptr<CdmVideoDecoder> video_decoder_;

  DISALLOW_COPY_AND_ASSIGN(ClearKeyCdm);
};

}  // namespace webkit_media

#endif  // WEBKIT_MEDIA_CRYPTO_PPAPI_CLEAR_KEY_CDM_H_
