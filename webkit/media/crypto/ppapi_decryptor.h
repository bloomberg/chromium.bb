// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_MEDIA_CRYPTO_PPAPI_DECRYPTOR_H_
#define WEBKIT_MEDIA_CRYPTO_PPAPI_DECRYPTOR_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "media/base/decryptor.h"
#include "media/base/video_decoder_config.h"

namespace base {
class MessageLoopProxy;
}

namespace media {
class DecryptorClient;
}

namespace webkit {
namespace ppapi {
class PluginInstance;
}
}

namespace webkit_media {

// PpapiDecryptor implements media::Decryptor and forwards all calls to the
// PluginInstance.
// This class should always be created & destroyed on the main renderer thread.
class PpapiDecryptor : public media::Decryptor {
 public:
  PpapiDecryptor(
    media::DecryptorClient* client,
    const scoped_refptr<webkit::ppapi::PluginInstance>& plugin_instance);
  virtual ~PpapiDecryptor();

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
  virtual void InitializeVideoDecoder(
      scoped_ptr<media::VideoDecoderConfig> config,
      const DecoderInitCB& init_cb,
      const KeyAddedCB& key_added_cb) OVERRIDE;
  virtual void DecryptAndDecodeVideo(
      const scoped_refptr<media::DecoderBuffer>& encrypted,
      const VideoDecodeCB& video_decode_cb) OVERRIDE;
  virtual void CancelDecryptAndDecodeVideo() OVERRIDE;
  virtual void StopVideoDecoder() OVERRIDE;

 private:
  void ReportFailureToCallPlugin(const std::string& key_system,
                                 const std::string& session_id);

  void OnVideoDecoderInitialized(bool success);

  media::DecryptorClient* client_;
  scoped_refptr<webkit::ppapi::PluginInstance> cdm_plugin_;
  scoped_refptr<base::MessageLoopProxy> render_loop_proxy_;

  DecoderInitCB video_decoder_init_cb_;
  KeyAddedCB key_added_cb_;

  base::WeakPtrFactory<PpapiDecryptor> weak_ptr_factory_;
  base::WeakPtr<PpapiDecryptor> weak_this_;

  DISALLOW_COPY_AND_ASSIGN(PpapiDecryptor);
};

}  // namespace webkit_media

#endif  // WEBKIT_MEDIA_CRYPTO_PPAPI_DECRYPTOR_H_
