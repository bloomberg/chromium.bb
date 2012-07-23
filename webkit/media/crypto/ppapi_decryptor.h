// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_MEDIA_CRYPTO_PPAPI_DECRYPTOR_H_
#define WEBKIT_MEDIA_CRYPTO_PPAPI_DECRYPTOR_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "media/base/decryptor.h"

namespace media {
class DecryptorClient;
}

namespace webkit {
namespace ppapi {
class PluginInstance;
}
}

namespace webkit_media {

class PpapiDecryptor : public media::Decryptor {
 public:
  PpapiDecryptor(
    media::DecryptorClient* client,
    const scoped_refptr<webkit::ppapi::PluginInstance>& plugin_instance);
  virtual ~PpapiDecryptor();

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
  virtual void Decrypt(const scoped_refptr<media::DecoderBuffer>& encrypted,
                       const DecryptCB& decrypt_cb) OVERRIDE;

 private:
  // Callback for the plugin to hand back the decrypted data.
  void DataReady(const DecryptCB& decrypt_cb, const uint8* data, int data_size);

  // TODO(xhwang): Need to figure out how the CDM plugin fires key events
  // (e.g. KeyMessage).
  media::DecryptorClient* client_;
  scoped_refptr<webkit::ppapi::PluginInstance> cdm_plugin_;

  DISALLOW_COPY_AND_ASSIGN(PpapiDecryptor);
};

}  // namespace webkit_media

#endif  // WEBKIT_MEDIA_CRYPTO_PPAPI_DECRYPTOR_H_
