// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_MEDIA_CRYPTO_PROXY_DECRYPTOR_H_
#define WEBKIT_MEDIA_CRYPTO_PROXY_DECRYPTOR_H_

#include <string>
#include <vector>

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

  void set_decryptor_for_testing(scoped_ptr<media::Decryptor> decryptor) {
    decryptor_ = decryptor.Pass();
  }

  // media::Decryptor implementation.
  virtual bool GenerateKeyRequest(const std::string& key_system,
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
  virtual void Decrypt(const scoped_refptr<media::DecoderBuffer>& encrypted,
                       const DecryptCB& decrypt_cb) OVERRIDE;
  virtual void CancelDecrypt() OVERRIDE;

 private:
  scoped_ptr<media::Decryptor> CreatePpapiDecryptor(
      const std::string& key_system);
  scoped_ptr<media::Decryptor> CreateDecryptor(const std::string& key_system);

  // Helper function that makes sure decryptor_->Decrypt() runs on the
  // |message_loop|.
  void DecryptOnMessageLoop(
      const scoped_refptr<base::MessageLoopProxy>& message_loop_proxy,
      const scoped_refptr<media::DecoderBuffer>& encrypted,
      const media::Decryptor::DecryptCB& decrypt_cb);

  // Callback used to pass into decryptor_->Decrypt().
  void OnBufferDecrypted(
      const scoped_refptr<base::MessageLoopProxy>& message_loop_proxy,
      const scoped_refptr<media::DecoderBuffer>& encrypted,
      const media::Decryptor::DecryptCB& decrypt_cb,
      media::Decryptor::Status status,
      const scoped_refptr<media::DecoderBuffer>& decrypted);

  media::DecryptorClient* client_;

  // Needed to create the PpapiDecryptor.
  WebKit::WebMediaPlayerClient* web_media_player_client_;
  WebKit::WebFrame* web_frame_;

  // Protects the |decryptor_| and |pending_decrypt_closures_|. Note that
  // |decryptor_| itself should be thread safe as per the Decryptor interface.
  base::Lock lock_;
  scoped_ptr<media::Decryptor> decryptor_;
  std::vector<base::Closure> pending_decrypt_closures_;
  bool stopped_;

  DISALLOW_COPY_AND_ASSIGN(ProxyDecryptor);
};

}  // namespace webkit_media

#endif  // WEBKIT_MEDIA_CRYPTO_PROXY_DECRYPTOR_H_
