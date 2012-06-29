// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_MEDIA_CRYPTO_PROXY_DECRYPTOR_H_
#define WEBKIT_MEDIA_CRYPTO_PROXY_DECRYPTOR_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "media/base/decryptor.h"

namespace media {
class DecryptorClient;
}

namespace webkit_media {

// A decryptor proxy that creates a real decryptor object on demand and
// forwards decryptor calls to it.
// TODO(xhwang): Currently we don't support run-time switching among decryptor
// objects. Fix this when needed.
class ProxyDecryptor : public media::Decryptor {
 public:
  explicit ProxyDecryptor(media::DecryptorClient* client);
  virtual ~ProxyDecryptor();

  // media::Decryptor implementation.
  virtual void GenerateKeyRequest(const std::string& key_system,
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
  virtual scoped_refptr<media::DecoderBuffer> Decrypt(
      const scoped_refptr<media::DecoderBuffer>& input) OVERRIDE;

 private:
  media::DecryptorClient* const client_;
  // Protects the |decryptor_|. The Decryptor interface specifies that the
  // Decrypt() function will be called on the decoder thread and all other
  // methods on the renderer thread. The |decryptor_| itself is thread safe
  // when this rule is obeyed. This lock is solely to prevent the race condition
  // between setting the |decryptor_| in GenerateKeyRequest() and using it in
  // Decrypt().
  base::Lock lock_;
  scoped_ptr<media::Decryptor> decryptor_;  // Protected by the |lock_|.

  DISALLOW_COPY_AND_ASSIGN(ProxyDecryptor);
};

}  // namespace webkit_media

#endif  // WEBKIT_MEDIA_CRYPTO_PROXY_DECRYPTOR_H_
