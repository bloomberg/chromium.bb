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

namespace webkit {
namespace ppapi {
class ContentDecryptorDelegate;
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
      const scoped_refptr<webkit::ppapi::PluginInstance>& plugin_instance,
      const media::KeyAddedCB& key_added_cb,
      const media::KeyErrorCB& key_error_cb,
      const media::KeyMessageCB& key_message_cb,
      const media::NeedKeyCB& need_key_cb);
  virtual ~PpapiDecryptor();

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
  virtual void RegisterNewKeyCB(StreamType stream_type,
                                const NewKeyCB& key_added_cb) OVERRIDE;
  virtual void Decrypt(StreamType stream_type,
                       const scoped_refptr<media::DecoderBuffer>& encrypted,
                       const DecryptCB& decrypt_cb) OVERRIDE;
  virtual void CancelDecrypt(StreamType stream_type) OVERRIDE;
  virtual void InitializeAudioDecoder(const media::AudioDecoderConfig& config,
                                      const DecoderInitCB& init_cb) OVERRIDE;
  virtual void InitializeVideoDecoder(const media::VideoDecoderConfig& config,
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
  void ReportFailureToCallPlugin(const std::string& key_system,
                                 const std::string& session_id);

  void OnDecoderInitialized(StreamType stream_type, bool success);

  // Callbacks for |plugin_cdm_delegate_| to fire key events.
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

  // Hold a reference of the plugin instance to make sure the plugin outlives
  // the |plugin_cdm_delegate_|. This is needed because |plugin_cdm_delegate_|
  // is owned by the |plugin_instance_|.
  scoped_refptr<webkit::ppapi::PluginInstance> plugin_instance_;

  // Callbacks for firing key events.
  media::KeyAddedCB key_added_cb_;
  media::KeyErrorCB key_error_cb_;
  media::KeyMessageCB key_message_cb_;
  media::NeedKeyCB need_key_cb_;

  webkit::ppapi::ContentDecryptorDelegate* plugin_cdm_delegate_;

  scoped_refptr<base::MessageLoopProxy> render_loop_proxy_;

  DecoderInitCB audio_decoder_init_cb_;
  DecoderInitCB video_decoder_init_cb_;
  NewKeyCB new_audio_key_cb_;
  NewKeyCB new_video_key_cb_;

  base::WeakPtrFactory<PpapiDecryptor> weak_ptr_factory_;
  base::WeakPtr<PpapiDecryptor> weak_this_;

  DISALLOW_COPY_AND_ASSIGN(PpapiDecryptor);
};

}  // namespace webkit_media

#endif  // WEBKIT_MEDIA_CRYPTO_PPAPI_DECRYPTOR_H_
