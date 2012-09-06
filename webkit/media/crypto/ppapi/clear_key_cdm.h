// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_MEDIA_CRYPTO_PPAPI_CLEAR_KEY_CDM_H_
#define WEBKIT_MEDIA_CRYPTO_PPAPI_CLEAR_KEY_CDM_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "media/base/decryptor_client.h"
#include "media/crypto/aes_decryptor.h"
#include "webkit/media/crypto/ppapi/content_decryption_module.h"

namespace media {
class DecoderBuffer;
}

namespace webkit_media {

// Clear key implementation of the cdm::ContentDecryptionModule interface.
class ClearKeyCdm : public cdm::ContentDecryptionModule {
 public:
  ClearKeyCdm();
  virtual ~ClearKeyCdm();

  // ContentDecryptionModule implementation.
  virtual cdm::Status GenerateKeyRequest(const uint8_t* init_data,
                                         int init_data_size,
                                         cdm::KeyMessage* key_request) OVERRIDE;
  virtual cdm::Status AddKey(const char* session_id,
                             int session_id_size,
                             const uint8_t* key,
                             int key_size,
                             const uint8_t* key_id,
                             int key_id_size) OVERRIDE;
  virtual cdm::Status CancelKeyRequest(const char* session_id,
                                       int session_id_size) OVERRIDE;
  virtual cdm::Status Decrypt(const cdm::InputBuffer& encrypted_buffer,
                              cdm::OutputBuffer* decrypted_buffer) OVERRIDE;
  virtual cdm::Status InitializeVideoDecoder(
      const cdm::VideoDecoderConfig& video_decoder_config) OVERRIDE;
  virtual cdm::Status DecryptAndDecodeVideo(
      const cdm::InputBuffer& encrypted_buffer,
      cdm::VideoFrame* video_frame) OVERRIDE;
  virtual void ResetVideoDecoder() OVERRIDE;
  virtual void StopVideoDecoder() OVERRIDE;

 private:
  class Client : public media::DecryptorClient {
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
    const uint8* key_message() { return key_message_.get(); }
    int key_message_length() { return key_message_length_; }
    const std::string& default_url() { return default_url_; }

    // Resets the Client to a clean state.
    void Reset();

    // media::DecryptorClient implementation.
    virtual void KeyAdded(const std::string& key_system,
                          const std::string& session_id) OVERRIDE;
    virtual void KeyError(const std::string& key_system,
                          const std::string& session_id,
                          media::Decryptor::KeyError error_code,
                          int system_code) OVERRIDE;
    virtual void KeyMessage(const std::string& key_system,
                            const std::string& session_id,
                            scoped_array<uint8> message,
                            int message_length,
                            const std::string& default_url) OVERRIDE;
    virtual void NeedKey(const std::string& key_system,
                         const std::string& session_id,
                         scoped_array<uint8> init_data,
                         int init_data_length) OVERRIDE;

   private:
    Status status_;
    std::string session_id_;
    scoped_array<uint8> key_message_;
    int key_message_length_;
    std::string default_url_;
  };

  Client client_;
  media::AesDecryptor decryptor_;
  // Protects the |client_| from being accessed by the |decryptor_|
  // simultaneously.
  base::Lock client_lock_;
};

}  // namespace webkit_media

#endif  // WEBKIT_MEDIA_CRYPTO_PPAPI_CLEAR_KEY_CDM_H_
