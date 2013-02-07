// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_MEDIA_CRYPTO_PROXY_DECRYPTOR_H_
#define WEBKIT_MEDIA_CRYPTO_PROXY_DECRYPTOR_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "media/base/decryptor.h"

namespace WebKit {
class WebFrame;
class WebMediaPlayerClient;
}

namespace webkit_media {

// A decryptor proxy that creates a real decryptor object on demand and
// forwards decryptor calls to it.
// TODO(xhwang): Currently we don't support run-time switching among decryptor
// objects. Fix this when needed.
class ProxyDecryptor {
 public:
  ProxyDecryptor(WebKit::WebMediaPlayerClient* web_media_player_client,
                 WebKit::WebFrame* web_frame,
                 const media::KeyAddedCB& key_added_cb,
                 const media::KeyErrorCB& key_error_cb,
                 const media::KeyMessageCB& key_message_cb,
                 const media::NeedKeyCB& need_key_cb);
  virtual ~ProxyDecryptor();

  // Requests the ProxyDecryptor to notify the decryptor when it's ready through
  // the |decryptor_ready_cb| provided.
  // If |decryptor_ready_cb| is null, the existing callback will be fired with
  // NULL immediately and reset.
  void SetDecryptorReadyCB(const media::DecryptorReadyCB& decryptor_ready_cb);

  bool GenerateKeyRequest(const std::string& key_system,
                          const std::string& type,
                          const uint8* init_data, int init_data_length);
  void AddKey(const std::string& key_system,
              const uint8* key, int key_length,
              const uint8* init_data, int init_data_length,
              const std::string& session_id);
  void CancelKeyRequest(const std::string& key_system,
                        const std::string& session_id);

 private:
  // Helper functions to create decryptors to handle the given |key_system|.
  scoped_ptr<media::Decryptor> CreatePpapiDecryptor(
      const std::string& key_system);
  scoped_ptr<media::Decryptor> CreateDecryptor(const std::string& key_system);

  // Callbacks for firing key events.
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
               scoped_array<uint8> init_data, int init_data_size);

  // Needed to create the PpapiDecryptor.
  WebKit::WebMediaPlayerClient* web_media_player_client_;
  WebKit::WebFrame* web_frame_;
  bool did_create_helper_plugin_;

  // Callbacks for firing key events.
  media::KeyAddedCB key_added_cb_;
  media::KeyErrorCB key_error_cb_;
  media::KeyMessageCB key_message_cb_;
  media::NeedKeyCB need_key_cb_;

  // Protects the |decryptor_|. Note that |decryptor_| itself should be thread
  // safe as per the Decryptor interface.
  base::Lock lock_;

  media::DecryptorReadyCB decryptor_ready_cb_;

  // The real decryptor that does decryption for the ProxyDecryptor.
  // This pointer is protected by the |lock_|.
  scoped_ptr<media::Decryptor> decryptor_;

  base::WeakPtrFactory<ProxyDecryptor> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ProxyDecryptor);
};

}  // namespace webkit_media

#endif  // WEBKIT_MEDIA_CRYPTO_PROXY_DECRYPTOR_H_
