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
  ProxyDecryptor(media::DecryptorClient* decryptor_client,
                 WebKit::WebMediaPlayerClient* web_media_player_client,
                 WebKit::WebFrame* web_frame);
  virtual ~ProxyDecryptor();

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

  DISALLOW_COPY_AND_ASSIGN(ProxyDecryptor);
};

}  // namespace webkit_media

#endif  // WEBKIT_MEDIA_CRYPTO_PROXY_DECRYPTOR_H_
