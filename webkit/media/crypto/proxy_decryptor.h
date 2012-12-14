// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_MEDIA_CRYPTO_PROXY_DECRYPTOR_H_
#define WEBKIT_MEDIA_CRYPTO_PROXY_DECRYPTOR_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "media/base/decryptor.h"

namespace base {
class MessageLoopProxy;
}

namespace media {
class DecryptorClient;
}

namespace WebKit {
class WebFrame;
class WebMediaPlayerClient;
}

namespace webkit_media {

// A decryptor proxy that creates a real decryptor object on demand and
// forwards decryptor calls to it.
// TODO(xhwang): Currently we don't support run-time switching among decryptor
// objects. Fix this when needed.
class ProxyDecryptor : public media::Decryptor {
 public:
  ProxyDecryptor(const scoped_refptr<base::MessageLoopProxy>& message_loop,
                 media::DecryptorClient* decryptor_client,
                 WebKit::WebMediaPlayerClient* web_media_player_client,
                 WebKit::WebFrame* web_frame);
  virtual ~ProxyDecryptor();

  void set_decryptor_for_testing(scoped_ptr<media::Decryptor> decryptor) {
    decryptor_ = decryptor.Pass();
  }

  // Requests the ProxyDecryptor to notify the decryptor when it's ready through
  // the |decryptor_ready_cb| provided.
  // If |decryptor_ready_cb| is null, the existing callback will be fired with
  // NULL immediately and reset.
  void SetDecryptorReadyCB(const media::DecryptorReadyCB& decryptor_ready_cb);

  // media::Decryptor implementation.
  virtual bool GenerateKeyRequest(const std::string& key_system,
                                  const std::string& type,
                                  const uint8* init_data,
                                  int init_data_length) OVERRIDE;
  virtual void AddKey(const std::string& key_system,
                      const uint8* key,
                      int key_length,
                      const uint8* init_data,
                      int init_data_length,
                      const std::string& session_id) OVERRIDE;
  virtual void CancelKeyRequest(const std::string& key_system,
                                const std::string& session_id) OVERRIDE;
  virtual void RegisterKeyAddedCB(StreamType stream_type,
                                  const KeyAddedCB& key_added_cb) OVERRIDE;
  virtual void Decrypt(StreamType stream_type,
                       const scoped_refptr<media::DecoderBuffer>& encrypted,
                       const DecryptCB& decrypt_cb) OVERRIDE;
  virtual void CancelDecrypt(StreamType stream_type) OVERRIDE;
  virtual void InitializeAudioDecoder(
      scoped_ptr<media::AudioDecoderConfig> config,
      const DecoderInitCB& init_cb) OVERRIDE;
  virtual void InitializeVideoDecoder(
      scoped_ptr<media::VideoDecoderConfig> config,
      const DecoderInitCB& init_cb) OVERRIDE;
  virtual void DecryptAndDecodeAudio(
      const scoped_refptr<media::DecoderBuffer>& encrypted,
      const AudioDecodeCB& audio_decode_cb) OVERRIDE;
  virtual void DecryptAndDecodeVideo(
      const scoped_refptr<media::DecoderBuffer>& encrypted,
      const VideoDecodeCB& video_decode_cb) OVERRIDE;
  virtual void ResetDecoder(StreamType stream_type) OVERRIDE;
  virtual void DeinitializeDecoder(StreamType stream_type) OVERRIDE;

 private:
  // Helper functions to create decryptors to handle the given |key_system|.
  scoped_ptr<media::Decryptor> CreatePpapiDecryptor(
      const std::string& key_system);
  scoped_ptr<media::Decryptor> CreateDecryptor(const std::string& key_system);

  void OnNewKeyAdded();

  // Sends |pending_buffer_to_decrypt_| to |decryptor_| for decryption.
  void DecryptPendingBuffer();

  // Callback passed to decryptor_->Decrypt(). It processes the |status| and
  // |decrypted| buffer from the |decryptor_|.
  void OnBufferDecrypted(media::Decryptor::Status status,
                         const scoped_refptr<media::DecoderBuffer>& decrypted);

  // Message loop on which decryption-related methods happen. Note that
  // key/session-related methods do not run on this message loop.
  scoped_refptr<base::MessageLoopProxy> const decryption_message_loop_;

  // DecryptorClient through which key events are fired.
  media::DecryptorClient* client_;

  // Needed to create the PpapiDecryptor.
  WebKit::WebMediaPlayerClient* web_media_player_client_;
  WebKit::WebFrame* web_frame_;

  // Protects the |decryptor_|. Note that |decryptor_| itself should be thread
  // safe as per the Decryptor interface.
  base::Lock lock_;

  media::DecryptorReadyCB decryptor_ready_cb_;

  // The real decryptor that does decryption for the ProxyDecryptor.
  // This pointer is protected by the |lock_|.
  scoped_ptr<media::Decryptor> decryptor_;

  // These two variables should be set whenever the ProxyDecryptor owes a
  // pending DecryptCB to the caller (e.g. the decoder).
  // TODO(xhwang): Here and below, it seems we have too many state variables
  // to track. Use a state enum instead!
  scoped_refptr<media::DecoderBuffer> pending_buffer_to_decrypt_;
  DecryptCB pending_decrypt_cb_;

  // Indicates the situation where the ProxyDecryptor has called
  // decryptor_->Decrypt() and is still waiting for it to return asynchronously.
  // This can only be true when |pending_buffer_to_decrypt_| is not NULL.
  bool is_waiting_for_decryptor_;

  // Indicates the situation where CancelDecrypt() is called but the
  // ProxyDecryptor needs to wait until |is_waiting_for_decryptor_| becomes
  // false to finish the canceling process.
  // This can only be true when |is_waiting_for_decryptor_| is true;
  bool is_canceling_decrypt_;

  // Indicates the situation where AddKey() is called when
  // |is_waiting_for_decryptor_| is true. The ProxyDecryptor will try to decrypt
  // the |pending_buffer_to_decrypt_| again if kNoKey is returned because a
  // new key has been added.
  bool has_new_key_added_;

  DISALLOW_COPY_AND_ASSIGN(ProxyDecryptor);
};

}  // namespace webkit_media

#endif  // WEBKIT_MEDIA_CRYPTO_PROXY_DECRYPTOR_H_
